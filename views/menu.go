package views

import (
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
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
	var s strings.Builder

	// Title with styling
	title := TitleStyle.Render("🌟 Welcome to TUI App!")
	s.WriteString(lipgloss.NewStyle().Width(50).Align(lipgloss.Center).Render(title))
	s.WriteString("\n\n")

	subtitle := SubtitleStyle.Render("Please select an option:")
	s.WriteString(subtitle)
	s.WriteString("\n\n")

	// Menu items with styling
	for i, choice := range m.Choices {
		if m.Cursor == i {
			s.WriteString(SelectedItemStyle.Render("▸ " + choice))
		} else {
			s.WriteString(UnselectedItemStyle.Render(choice))
		}
		s.WriteString("\n")
	}

	// Help text
	help := HelpStyle.Render("\n↑/k up • ↓/j down • enter select • q quit")
	s.WriteString(help)

	// Wrap everything in a nice box
	content := s.String()
	box := BoxStyle.Render(content)

	return "\n" + box + "\n"
}

// Reset resets the menu state
func (m MenuModel) Reset() MenuModel {
	m.Cursor = 0
	return m
}
