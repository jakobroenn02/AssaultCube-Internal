package views

import (
	"strings"

	tea "github.com/charmbracelet/bubbletea"
)

// DashboardModel represents the dashboard view after login
type DashboardModel struct {
	Username string
	Cursor   int
	Choices  []string
}

// NewDashboardModel creates a new dashboard model
func NewDashboardModel(username string) DashboardModel {
	return DashboardModel{
		Username: username,
		Choices:  []string{"Load Assault Cube", "Reset Password", "Logout"},
		Cursor:   0,
	}
}

// Update handles dashboard input
func (m DashboardModel) Update(msg tea.KeyMsg) (DashboardModel, tea.Cmd, ViewTransition) {
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
			return m, nil, TransitionTo(LoadAssaultCubeViewType)
		case 1:
			return m, nil, TransitionTo(ResetPasswordViewType)
		case 2:
			// Logout - go back to menu
			return m, nil, TransitionTo(MenuViewType)
		}
	}

	return m, nil, NoTransition()
}

// View renders the dashboard
func (m DashboardModel) View() string {
	var s strings.Builder

	// Title with welcome message
	title := TitleStyle.Render("🏠 Dashboard")
	s.WriteString(title)
	s.WriteString("\n\n")

	// Welcome message
	welcomeMsg := SubtitleStyle.Render("Welcome, " + m.Username + "!")
	s.WriteString(welcomeMsg)
	s.WriteString("\n\n")

	// Menu items
	for i, choice := range m.Choices {
		if m.Cursor == i {
			s.WriteString(SelectedItemStyle.Render("▸ " + choice))
		} else {
			s.WriteString(UnselectedItemStyle.Render(choice))
		}
		s.WriteString("\n")
	}

	// Help text
	s.WriteString("\n")
	helpText := HelpStyle.Render("↑/k up • ↓/j down • enter select • q quit")
	s.WriteString(helpText)

	// Wrap in a box
	content := s.String()
	box := BoxStyle.Render(content)

	return "\n" + box + "\n"
}

// Reset resets the dashboard state
func (m DashboardModel) Reset() DashboardModel {
	m.Cursor = 0
	return m
}

// SetUsername updates the logged-in username
func (m DashboardModel) SetUsername(username string) DashboardModel {
	m.Username = username
	return m
}
