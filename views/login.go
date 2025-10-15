package views

import (
	"database/sql"
	"strings"

	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"golang.org/x/crypto/bcrypt"
)

// LoginModel represents the login form view
type LoginModel struct {
	UsernameInput textinput.Model
	PasswordInput textinput.Model
	FocusedField  int // 0 = username, 1 = password, 2 = submit button
	Error         string
	Success       bool
	LoggedInUser  string
	DB            *sql.DB
}

// NewLoginModel creates a new login model
func NewLoginModel(db *sql.DB) LoginModel {
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

	return LoginModel{
		UsernameInput: usernameInput,
		PasswordInput: passwordInput,
		FocusedField:  0,
		Error:         "",
		Success:       false,
		LoggedInUser:  "",
		DB:            db,
	}
}

// Update handles login form input
func (m LoginModel) Update(msg tea.KeyMsg) (LoginModel, tea.Cmd, ViewTransition) {
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
			return m.handleLogin()
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

// View renders the login form
func (m LoginModel) View() string {
	var s strings.Builder

	// Title
	title := TitleStyle.Render("🔐 Login")
	s.WriteString(title)
	s.WriteString("\n\n")

	// If login successful, show success message
	if m.Success {
		successMsg := SuccessStyle.Render("✓ Login successful!")
		s.WriteString(successMsg)
		s.WriteString("\n\n")
		welcomeMsg := SubtitleStyle.Render("Welcome back, " + m.LoggedInUser + "!")
		s.WriteString(welcomeMsg)
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
		button = FocusedButtonStyle.Render("[ Login ]")
	} else {
		button = BlurredButtonStyle.Render("[ Login ]")
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

// Reset resets the login form
func (m LoginModel) Reset() LoginModel {
	m.UsernameInput.SetValue("")
	m.PasswordInput.SetValue("")
	m.FocusedField = 0
	m.Error = ""
	m.Success = false
	m.LoggedInUser = ""
	m.UsernameInput.Focus()
	m.PasswordInput.Blur()
	return m
}

// handleLogin processes the login attempt
func (m LoginModel) handleLogin() (LoginModel, tea.Cmd, ViewTransition) {
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

	// Query the database for the user
	var storedHash string
	var userID int
	query := "SELECT id, password_hash FROM users WHERE username = ?"
	err := m.DB.QueryRow(query, username).Scan(&userID, &storedHash)

	if err == sql.ErrNoRows {
		m.Error = "Invalid username or password"
		return m, nil, NoTransition()
	} else if err != nil {
		m.Error = "Database error occurred"
		return m, nil, NoTransition()
	}

	// Compare the password with the stored hash
	err = bcrypt.CompareHashAndPassword([]byte(storedHash), []byte(password))
	if err != nil {
		m.Error = "Invalid username or password"
		return m, nil, NoTransition()
	}

	// Login successful!
	m.Success = true
	m.LoggedInUser = username

	// TODO: Transition to a home/dashboard view
	// For now, just show success message
	return m, nil, NoTransition()
}
