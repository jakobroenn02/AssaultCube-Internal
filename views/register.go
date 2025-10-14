package views

import (
	"database/sql"
	"fmt"
	"strings"

	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"golang.org/x/crypto/bcrypt"
)

// RegisterModel represents the registration form view
type RegisterModel struct {
	UsernameInput textinput.Model
	PasswordInput textinput.Model
	FocusedField  int // 0 = username, 1 = password, 2 = submit button
	Error         string
	Success       bool
	DB            *sql.DB
}

// NewRegisterModel creates a new registration model
func NewRegisterModel(db *sql.DB) RegisterModel {
	usernameInput := textinput.New()
	usernameInput.Placeholder = "Enter username"
	usernameInput.Focus()
	usernameInput.CharLimit = 50
	usernameInput.Width = 30
	usernameInput.PromptStyle = FocusedStyle
	usernameInput.TextStyle = FocusedStyle

	passwordInput := textinput.New()
	passwordInput.Placeholder = "Enter password"
	passwordInput.CharLimit = 100
	passwordInput.Width = 30
	passwordInput.EchoMode = textinput.EchoPassword
	passwordInput.EchoCharacter = '•'

	return RegisterModel{
		UsernameInput: usernameInput,
		PasswordInput: passwordInput,
		FocusedField:  0,
		Error:         "",
		Success:       false,
		DB:            db,
	}
}

// Update handles registration form input
func (m RegisterModel) Update(msg tea.KeyMsg) (RegisterModel, tea.Cmd, ViewTransition) {
	var cmd tea.Cmd

	switch msg.String() {
	case "tab", "shift+tab", "up", "down":
		// Cycle through fields
		if msg.String() == "up" || msg.String() == "shift+tab" {
			m.FocusedField--
		} else {
			m.FocusedField++
		}

		// Wrap around
		if m.FocusedField < 0 {
			m.FocusedField = 2
		} else if m.FocusedField > 2 {
			m.FocusedField = 0
		}

		// Update focus states
		if m.FocusedField == 0 {
			m.UsernameInput.Focus()
			m.PasswordInput.Blur()
			m.UsernameInput.PromptStyle = FocusedStyle
			m.UsernameInput.TextStyle = FocusedStyle
			m.PasswordInput.PromptStyle = BlurredStyle
			m.PasswordInput.TextStyle = BlurredStyle
		} else if m.FocusedField == 1 {
			m.UsernameInput.Blur()
			m.PasswordInput.Focus()
			m.UsernameInput.PromptStyle = BlurredStyle
			m.UsernameInput.TextStyle = BlurredStyle
			m.PasswordInput.PromptStyle = FocusedStyle
			m.PasswordInput.TextStyle = FocusedStyle
		} else {
			m.UsernameInput.Blur()
			m.PasswordInput.Blur()
			m.UsernameInput.PromptStyle = BlurredStyle
			m.UsernameInput.TextStyle = BlurredStyle
			m.PasswordInput.PromptStyle = BlurredStyle
			m.PasswordInput.TextStyle = BlurredStyle
		}

	case "enter":
		if m.FocusedField == 2 {
			return m.handleSubmit()
		}
	}

	// Update the focused input
	if m.FocusedField == 0 {
		m.UsernameInput, cmd = m.UsernameInput.Update(msg)
	} else if m.FocusedField == 1 {
		m.PasswordInput, cmd = m.PasswordInput.Update(msg)
	}

	return m, cmd, NoTransition()
}

// View renders the registration form
func (m RegisterModel) View() string {
	var s strings.Builder

	// Title
	title := TitleStyle.Render("✨ Register")
	s.WriteString(title)
	s.WriteString("\n\n")

	if m.Success {
		successMsg := SuccessStyle.Render("✓ Registration successful!")
		s.WriteString(successMsg)
		s.WriteString("\n\n")
		helpText := HelpStyle.Render("Press 'esc' to go back to menu")
		s.WriteString(helpText)

		content := s.String()
		box := BoxStyle.Render(content)
		return "\n" + box + "\n"
	}

	// Username field
	s.WriteString("Username\n")
	s.WriteString(m.UsernameInput.View())
	s.WriteString("\n\n")

	// Password field
	s.WriteString("Password\n")
	s.WriteString(m.PasswordInput.View())
	s.WriteString("\n\n")

	// Submit button
	var button string
	if m.FocusedField == 2 {
		button = FocusedButtonStyle.Render("[ Register ]")
	} else {
		button = BlurredButtonStyle.Render("[ Register ]")
	}
	s.WriteString(button)

	// Error message
	if m.Error != "" {
		s.WriteString("\n\n")
		errorMsg := ErrorStyle.Render("❌ " + m.Error)
		s.WriteString(errorMsg)
	}

	// Help text
	s.WriteString("\n\n")
	helpText := HelpStyle.Render("tab navigate • enter submit • esc back")
	s.WriteString(helpText)

	content := s.String()
	box := BoxStyle.Render(content)
	return "\n" + box + "\n"
}

// Reset resets the registration form
func (m RegisterModel) Reset() RegisterModel {
	m.UsernameInput.SetValue("")
	m.PasswordInput.SetValue("")
	m.FocusedField = 0
	m.Error = ""
	m.Success = false
	m.UsernameInput.Focus()
	m.PasswordInput.Blur()
	return m
}

// handleSubmit processes the registration
func (m RegisterModel) handleSubmit() (RegisterModel, tea.Cmd, ViewTransition) {
	// Reset error
	m.Error = ""

	username := m.UsernameInput.Value()
	password := m.PasswordInput.Value()

	// Validate input
	if username == "" {
		m.Error = "Username cannot be empty"
		return m, nil, NoTransition()
	}

	if password == "" {
		m.Error = "Password cannot be empty"
		return m, nil, NoTransition()
	}

	if len(password) < 6 {
		m.Error = "Password must be at least 6 characters"
		return m, nil, NoTransition()
	}

	// Hash the password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		m.Error = "Failed to hash password"
		return m, nil, NoTransition()
	}

	// Insert into database
	query := "INSERT INTO users (username, password_hash) VALUES (?, ?)"
	_, err = m.DB.Exec(query, username, string(hashedPassword))
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
