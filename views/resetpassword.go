package views

import (
	"database/sql"
	"strings"

	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"golang.org/x/crypto/bcrypt"
)

// ResetPasswordModel represents the reset password view
type ResetPasswordModel struct {
	Username        string
	CurrentPassword textinput.Model
	NewPassword     textinput.Model
	ConfirmPassword textinput.Model
	FocusedField    int // 0 = current, 1 = new, 2 = confirm, 3 = submit
	Error           string
	Success         bool
	DB              *sql.DB
}

// NewResetPasswordModel creates a new reset password model
func NewResetPasswordModel(username string, db *sql.DB) ResetPasswordModel {
	currentPassword := textinput.New()
	currentPassword.Placeholder = "Current password"
	currentPassword.Focus()
	currentPassword.CharLimit = 100
	currentPassword.Width = 30
	currentPassword.EchoMode = textinput.EchoPassword
	currentPassword.EchoCharacter = '•'
	currentPassword.PromptStyle = FocusedStyle
	currentPassword.TextStyle = FocusedStyle

	newPassword := textinput.New()
	newPassword.Placeholder = "New password"
	newPassword.CharLimit = 100
	newPassword.Width = 30
	newPassword.EchoMode = textinput.EchoPassword
	newPassword.EchoCharacter = '•'

	confirmPassword := textinput.New()
	confirmPassword.Placeholder = "Confirm new password"
	confirmPassword.CharLimit = 100
	confirmPassword.Width = 30
	confirmPassword.EchoMode = textinput.EchoPassword
	confirmPassword.EchoCharacter = '•'

	return ResetPasswordModel{
		Username:        username,
		CurrentPassword: currentPassword,
		NewPassword:     newPassword,
		ConfirmPassword: confirmPassword,
		FocusedField:    0,
		Error:           "",
		Success:         false,
		DB:              db,
	}
}

// Update handles reset password input
func (m ResetPasswordModel) Update(msg tea.KeyMsg) (ResetPasswordModel, tea.Cmd, ViewTransition) {
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
			m.FocusedField = 3
		} else if m.FocusedField > 3 {
			m.FocusedField = 0
		}

		// Update focus states
		m.CurrentPassword.Blur()
		m.NewPassword.Blur()
		m.ConfirmPassword.Blur()
		m.CurrentPassword.PromptStyle = BlurredStyle
		m.CurrentPassword.TextStyle = BlurredStyle
		m.NewPassword.PromptStyle = BlurredStyle
		m.NewPassword.TextStyle = BlurredStyle
		m.ConfirmPassword.PromptStyle = BlurredStyle
		m.ConfirmPassword.TextStyle = BlurredStyle

		if m.FocusedField == 0 {
			m.CurrentPassword.Focus()
			m.CurrentPassword.PromptStyle = FocusedStyle
			m.CurrentPassword.TextStyle = FocusedStyle
		} else if m.FocusedField == 1 {
			m.NewPassword.Focus()
			m.NewPassword.PromptStyle = FocusedStyle
			m.NewPassword.TextStyle = FocusedStyle
		} else if m.FocusedField == 2 {
			m.ConfirmPassword.Focus()
			m.ConfirmPassword.PromptStyle = FocusedStyle
			m.ConfirmPassword.TextStyle = FocusedStyle
		}

	case "enter":
		if m.FocusedField == 3 {
			return m.handlePasswordReset()
		}
	}

	// Update the focused input
	if m.FocusedField == 0 {
		m.CurrentPassword, cmd = m.CurrentPassword.Update(msg)
	} else if m.FocusedField == 1 {
		m.NewPassword, cmd = m.NewPassword.Update(msg)
	} else if m.FocusedField == 2 {
		m.ConfirmPassword, cmd = m.ConfirmPassword.Update(msg)
	}

	return m, cmd, NoTransition()
}

// View renders the reset password view
func (m ResetPasswordModel) View() string {
	var s strings.Builder

	// Title
	title := TitleStyle.Render("🔑 Reset Password")
	s.WriteString(title)
	s.WriteString("\n\n")

	if m.Success {
		successMsg := SuccessStyle.Render("✓ Password reset successful!")
		s.WriteString(successMsg)
		s.WriteString("\n\n")
		helpText := HelpStyle.Render("Press 'esc' to go back")
		s.WriteString(helpText)

		content := s.String()
		box := BoxStyle.Render(content)
		return "\n" + box + "\n"
	}

	// Current password field
	s.WriteString("Current Password\n")
	s.WriteString(m.CurrentPassword.View())
	s.WriteString("\n\n")

	// New password field
	s.WriteString("New Password\n")
	s.WriteString(m.NewPassword.View())
	s.WriteString("\n\n")

	// Confirm password field
	s.WriteString("Confirm New Password\n")
	s.WriteString(m.ConfirmPassword.View())
	s.WriteString("\n\n")

	// Submit button
	var button string
	if m.FocusedField == 3 {
		button = FocusedButtonStyle.Render("[ Reset Password ]")
	} else {
		button = BlurredButtonStyle.Render("[ Reset Password ]")
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

// Reset resets the reset password state
func (m ResetPasswordModel) Reset() ResetPasswordModel {
	m.CurrentPassword.SetValue("")
	m.NewPassword.SetValue("")
	m.ConfirmPassword.SetValue("")
	m.FocusedField = 0
	m.Error = ""
	m.Success = false
	m.CurrentPassword.Focus()
	return m
}

// SetUsername updates the username
func (m ResetPasswordModel) SetUsername(username string) ResetPasswordModel {
	m.Username = username
	return m
}

// handlePasswordReset processes the password reset
func (m ResetPasswordModel) handlePasswordReset() (ResetPasswordModel, tea.Cmd, ViewTransition) {
	m.Error = ""

	currentPwd := m.CurrentPassword.Value()
	newPwd := m.NewPassword.Value()
	confirmPwd := m.ConfirmPassword.Value()

	// Validate input
	if currentPwd == "" {
		m.Error = "Current password cannot be empty"
		return m, nil, NoTransition()
	}

	if newPwd == "" {
		m.Error = "New password cannot be empty"
		return m, nil, NoTransition()
	}

	if len(newPwd) < 6 {
		m.Error = "New password must be at least 6 characters"
		return m, nil, NoTransition()
	}

	if newPwd != confirmPwd {
		m.Error = "New passwords do not match"
		return m, nil, NoTransition()
	}

	// Verify current password
	var storedHash string
	query := "SELECT password_hash FROM users WHERE username = ?"
	err := m.DB.QueryRow(query, m.Username).Scan(&storedHash)
	if err != nil {
		m.Error = "Database error occurred"
		return m, nil, NoTransition()
	}

	err = bcrypt.CompareHashAndPassword([]byte(storedHash), []byte(currentPwd))
	if err != nil {
		m.Error = "Current password is incorrect"
		return m, nil, NoTransition()
	}

	// Hash the new password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(newPwd), bcrypt.DefaultCost)
	if err != nil {
		m.Error = "Failed to hash new password"
		return m, nil, NoTransition()
	}

	// Update the password in database
	updateQuery := "UPDATE users SET password_hash = ? WHERE username = ?"
	_, err = m.DB.Exec(updateQuery, string(hashedPassword), m.Username)
	if err != nil {
		m.Error = "Failed to update password"
		return m, nil, NoTransition()
	}

	// Success!
	m.Success = true
	return m, nil, NoTransition()
}
