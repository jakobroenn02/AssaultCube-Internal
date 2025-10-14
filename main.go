package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"

	"go-project/views"

	tea "github.com/charmbracelet/bubbletea"
)

// model is the main application model that orchestrates different views
type model struct {
	currentView   views.ViewType
	menuModel     views.MenuModel
	loginModel    views.LoginModel
	registerModel views.RegisterModel
}

// initialModel creates the initial application model
func initialModel() model {
	return model{
		currentView:   views.MenuViewType,
		menuModel:     views.NewMenuModel(),
		loginModel:    views.NewLoginModel(),
		registerModel: views.NewRegisterModel(DB),
	}
}

// Init initializes the model
func (m model) Init() tea.Cmd {
	return nil
}

// Update handles all input and delegates to specific views
func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		// Global shortcuts
		switch msg.String() {
		case "ctrl+c", "q":
			return m, tea.Quit

		case "esc":
			// Go back to menu from any view
			if m.currentView != views.MenuViewType {
				m.currentView = views.MenuViewType
				m.menuModel = m.menuModel.Reset()
				m.loginModel = m.loginModel.Reset()
				m.registerModel = m.registerModel.Reset()
			}
			return m, nil
		}

		// Delegate to current view
		var transition views.ViewTransition
		switch m.currentView {
		case views.MenuViewType:
			var cmd tea.Cmd
			m.menuModel, cmd, transition = m.menuModel.Update(msg)
			if transition.ShouldTransition {
				m.currentView = transition.TargetView
			}
			return m, cmd

		case views.LoginViewType:
			var cmd tea.Cmd
			m.loginModel, cmd, transition = m.loginModel.Update(msg)
			if transition.ShouldTransition {
				m.currentView = transition.TargetView
			}
			return m, cmd

		case views.RegisterViewType:
			var cmd tea.Cmd
			m.registerModel, cmd, transition = m.registerModel.Update(msg)
			if transition.ShouldTransition {
				m.currentView = transition.TargetView
			}
			return m, cmd
		}
	}

	return m, nil
}

// View renders the current view
func (m model) View() string {
	switch m.currentView {
	case views.MenuViewType:
		return m.menuModel.View()
	case views.LoginViewType:
		return m.loginModel.View()
	case views.RegisterViewType:
		return m.registerModel.View()
	default:
		return "Unknown view"
	}
}

func main() {
	// Initialize database connection
	if err := InitDB(); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	defer CloseDB()

	// Clear terminal before starting TUI
	cmd := exec.Command("clear")
	cmd.Stdout = os.Stdout
	if err := cmd.Run(); err != nil {
		log.Fatal(err)
	}

	// Start the Bubble Tea program
	p := tea.NewProgram(initialModel())
	if _, err := p.Run(); err != nil {
		fmt.Printf("Alas, there's been an error: %v", err)
		os.Exit(1)
	}
}
