package main

import (
	"fmt"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"golang.org/x/crypto/bcrypt"
)

func (m model) renderRegister() string {
	var s strings.Builder

	s.WriteString("=== Register ===\n\n")

	if m.regSuccess {
		s.WriteString("✓ Registration successful!\n\n")
		s.WriteString("Press 'esc' to go back to menu.\n")
		return s.String()
	}

	// Username field
	if m.regFocusedField == 0 {
		s.WriteString("> Username: " + m.regUsername + "_\n")
	} else {
		s.WriteString("  Username: " + m.regUsername + "\n")
	}

	// Password field (masked)
	maskedPassword := strings.Repeat("*", len(m.regPassword))
	if m.regFocusedField == 1 {
		s.WriteString("> Password: " + maskedPassword + "_\n")
	} else {
		s.WriteString("  Password: " + maskedPassword + "\n")
	}

	s.WriteString("\n")

	// Submit button
	if m.regFocusedField == 2 {
		s.WriteString("> [Register]\n")
	} else {
		s.WriteString("  [Register]\n")
	}

	// Error message
	if m.regError != "" {
		s.WriteString("\n❌ Error: " + m.regError + "\n")
	}

	s.WriteString("\nUse arrow keys/tab to navigate, type to enter text, Enter to submit.\n")
	s.WriteString("Press 'esc' to go back to menu, q to quit.\n")

	return s.String()
}

func (m model) handleRegistration() (tea.Model, tea.Cmd) {
	// Reset error
	m.regError = ""

	// Validate input
	if m.regUsername == "" {
		m.regError = "Username cannot be empty"
		return m, nil
	}

	if m.regPassword == "" {
		m.regError = "Password cannot be empty"
		return m, nil
	}

	if len(m.regPassword) < 6 {
		m.regError = "Password must be at least 6 characters"
		return m, nil
	}

	// Hash the password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(m.regPassword), bcrypt.DefaultCost)
	if err != nil {
		m.regError = "Failed to hash password"
		return m, nil
	}

	// Insert into database
	query := "INSERT INTO users (username, password_hash) VALUES (?, ?)"
	_, err = DB.Exec(query, m.regUsername, string(hashedPassword))
	if err != nil {
		// Check if username already exists
		if strings.Contains(err.Error(), "Duplicate entry") {
			m.regError = "Username already exists"
		} else {
			m.regError = fmt.Sprintf("Database error: %v", err)
		}
		return m, nil
	}

	// Success!
	m.regSuccess = true

	return m, nil
}
