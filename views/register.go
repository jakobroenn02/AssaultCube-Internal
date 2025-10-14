package views

import (
	"database/sql"
	"fmt"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"golang.org/x/crypto/bcrypt"
)

// RegisterModel represents the registration form view
type RegisterModel struct {
	Username     string
	Password     string
	FocusedField int // 0 = username, 1 = password, 2 = submit button
	Error        string
	Success      bool
	DB           *sql.DB
}

// NewRegisterModel creates a new registration model
func NewRegisterModel(db *sql.DB) RegisterModel {
	return RegisterModel{
		Username:     "",
		Password:     "",
		FocusedField: 0,
		Error:        "",
		Success:      false,
		DB:           db,
	}
}

// Update handles registration form input
func (m RegisterModel) Update(msg tea.KeyMsg) (RegisterModel, tea.Cmd, ViewTransition) {
	switch msg.String() {
	case "up":
		if m.FocusedField > 0 {
			m.FocusedField--
		}

	case "down":
		if m.FocusedField < 2 {
			m.FocusedField++
		}

	case "tab":
		m.FocusedField = (m.FocusedField + 1) % 3

	case "enter":
		if m.FocusedField == 2 {
			return m.handleSubmit()
		}

	case "backspace":
		if m.FocusedField == 0 && len(m.Username) > 0 {
			m.Username = m.Username[:len(m.Username)-1]
		} else if m.FocusedField == 1 && len(m.Password) > 0 {
			m.Password = m.Password[:len(m.Password)-1]
		}

	default:
		// Regular character input (including 'k' and 'j')
		if len(msg.String()) == 1 {
			if m.FocusedField == 0 {
				m.Username += msg.String()
			} else if m.FocusedField == 1 {
				m.Password += msg.String()
			}
		}
	}

	return m, nil, NoTransition()
}

// View renders the registration form
func (m RegisterModel) View() string {
	var s strings.Builder

	s.WriteString("=== Register ===\n\n")

	if m.Success {
		s.WriteString("✓ Registration successful!\n\n")
		s.WriteString("Press 'esc' to go back to menu.\n")
		return s.String()
	}

	// Username field
	if m.FocusedField == 0 {
		s.WriteString("> Username: " + m.Username + "_\n")
	} else {
		s.WriteString("  Username: " + m.Username + "\n")
	}

	// Password field (masked)
	maskedPassword := strings.Repeat("*", len(m.Password))
	if m.FocusedField == 1 {
		s.WriteString("> Password: " + maskedPassword + "_\n")
	} else {
		s.WriteString("  Password: " + maskedPassword + "\n")
	}

	s.WriteString("\n")

	// Submit button
	if m.FocusedField == 2 {
		s.WriteString("> [Register]\n")
	} else {
		s.WriteString("  [Register]\n")
	}

	// Error message
	if m.Error != "" {
		s.WriteString("\n❌ Error: " + m.Error + "\n")
	}

	s.WriteString("\nUse arrow keys/tab to navigate, type to enter text, Enter to submit.\n")
	s.WriteString("Press 'esc' to go back to menu, q to quit.\n")

	return s.String()
}

// Reset resets the registration form
func (m RegisterModel) Reset() RegisterModel {
	m.Username = ""
	m.Password = ""
	m.FocusedField = 0
	m.Error = ""
	m.Success = false
	return m
}

// handleSubmit processes the registration
func (m RegisterModel) handleSubmit() (RegisterModel, tea.Cmd, ViewTransition) {
	// Reset error
	m.Error = ""

	// Validate input
	if m.Username == "" {
		m.Error = "Username cannot be empty"
		return m, nil, NoTransition()
	}

	if m.Password == "" {
		m.Error = "Password cannot be empty"
		return m, nil, NoTransition()
	}

	if len(m.Password) < 6 {
		m.Error = "Password must be at least 6 characters"
		return m, nil, NoTransition()
	}

	// Hash the password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(m.Password), bcrypt.DefaultCost)
	if err != nil {
		m.Error = "Failed to hash password"
		return m, nil, NoTransition()
	}

	// Insert into database
	query := "INSERT INTO users (username, password_hash) VALUES (?, ?)"
	_, err = m.DB.Exec(query, m.Username, string(hashedPassword))
	if err != nil {
		// Check if username already exists
		if strings.Contains(err.Error(), "Duplicate entry") {
			m.Error = "Username already exists"
		} else {
			m.Error = fmt.Sprintf("Database error: %v", err)
		}
		return m, nil, NoTransition()
	}

	// Success!
	m.Success = true

	return m, nil, NoTransition()
}
