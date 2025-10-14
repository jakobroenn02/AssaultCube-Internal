package views

import (
	tea "github.com/charmbracelet/bubbletea"
)

// LoginModel represents the login form view
type LoginModel struct {
	Username     string
	Password     string
	FocusedField int // 0 = username, 1 = password, 2 = submit button
	Error        string
}

// NewLoginModel creates a new login model
func NewLoginModel() LoginModel {
	return LoginModel{
		Username:     "",
		Password:     "",
		FocusedField: 0,
		Error:        "",
	}
}

// Update handles login form input
func (m LoginModel) Update(msg tea.KeyMsg) (LoginModel, tea.Cmd, ViewTransition) {
	// TODO: Implement login logic
	return m, nil, NoTransition()
}

// View renders the login form
func (m LoginModel) View() string {
	s := "=== Login ===\n\n"
	s += "This is the login view.\n"
	s += "TODO: Add login form here.\n\n"
	s += "Press 'esc' to go back to menu, q to quit.\n"
	return s
}

// Reset resets the login form
func (m LoginModel) Reset() LoginModel {
	m.Username = ""
	m.Password = ""
	m.FocusedField = 0
	m.Error = ""
	return m
}
