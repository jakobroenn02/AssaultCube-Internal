package views

import (
	"fmt"

	tea "github.com/charmbracelet/bubbletea"
)

// MenuModel represents the main menu view
type MenuModel struct {
	Cursor  int
	Choices []string
}

// NewMenuModel creates a new menu model
func NewMenuModel() MenuModel {
	return MenuModel{
		Choices: []string{"Login", "Register"},
		Cursor:  0,
	}
}

// Update handles menu-specific input
func (m MenuModel) Update(msg tea.KeyMsg) (MenuModel, tea.Cmd, ViewTransition) {
	switch msg.String() {
	case "up", "k":
		if m.Cursor > 0 {
			m.Cursor--
		}

	case "down", "j":
		if m.Cursor < len(m.Choices)-1 {
			m.Cursor++
		}

	case "enter":
		switch m.Cursor {
		case 0:
			return m, nil, TransitionTo(LoginViewType)
		case 1:
			return m, nil, TransitionTo(RegisterViewType)
		}
	}

	return m, nil, NoTransition()
}

// View renders the menu
func (m MenuModel) View() string {
	s := "Welcome! Please select an option:\n\n"

	for i, choice := range m.Choices {
		cursor := " "
		if m.Cursor == i {
			cursor = ">"
		}
		s += fmt.Sprintf("%s %s\n", cursor, choice)
	}

	s += "\nUse arrow keys to navigate, Enter to select, q to quit.\n"

	return s
}

// Reset resets the menu state
func (m MenuModel) Reset() MenuModel {
	m.Cursor = 0
	return m
}
