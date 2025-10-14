package main

// These imports will be used later on the tutorial. If you save the file
// now, Go might complain they are unused, but that's fine.
// You may also need to run `go mod tidy` to download bubbletea and its
// dependencies.
import (
	"fmt"
	"log"
	"os"
	"os/exec"

	tea "github.com/charmbracelet/bubbletea"
)

// viewState represents different screens in the app
type viewState int

const (
	menuView viewState = iota
	loginView
	registerView
)

type model struct {
	state   viewState // current view/screen
	cursor  int       // which menu item our cursor is pointing at
	choices []string  // menu options

	// Registration form fields
	regUsername     string
	regPassword     string
	regFocusedField int    // 0 = username, 1 = password, 2 = submit button
	regError        string // error message to display
	regSuccess      bool   // registration success flag
}

func initialModel() model {
	return model{
		state:   menuView,
		choices: []string{"Login", "Register"},
		cursor:  0,
	}
}

func (m model) Init() tea.Cmd {
	// Just return `nil`, which means "no I/O right now, please."
	return nil
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {

	// Is it a key press?
	case tea.KeyMsg:

		// Cool, what was the actual key pressed?
		switch msg.String() {

		// These keys should exit the program.
		case "ctrl+c", "q":
			return m, tea.Quit

		// Handle "esc" to go back to menu
		case "esc":
			if m.state != menuView {
				m.state = menuView
				m.cursor = 0
				// Reset registration form
				m.regUsername = ""
				m.regPassword = ""
				m.regFocusedField = 0
				m.regError = ""
				m.regSuccess = false
			}

		// The "up" and "k" keys move the cursor up
		case "up", "k":
			if m.state == menuView && m.cursor > 0 {
				m.cursor--
			} else if m.state == registerView && m.regFocusedField > 0 {
				m.regFocusedField--
			}

		// The "down" and "j" keys move the cursor down
		case "down", "j":
			if m.state == menuView && m.cursor < len(m.choices)-1 {
				m.cursor++
			} else if m.state == registerView && m.regFocusedField < 2 {
				m.regFocusedField++
			}

		// Tab key to move between fields in register view
		case "tab":
			if m.state == registerView {
				m.regFocusedField = (m.regFocusedField + 1) % 3
			}

		// The "enter" key switches views based on selection
		case "enter":
			if m.state == menuView {
				switch m.cursor {
				case 0: // Login selected
					m.state = loginView
				case 1: // Register selected
					m.state = registerView
					m.regFocusedField = 0
				}
			} else if m.state == registerView && m.regFocusedField == 2 {
				// Submit button pressed
				return m.handleRegistration()
			}

		// Backspace to delete characters
		case "backspace":
			if m.state == registerView {
				if m.regFocusedField == 0 && len(m.regUsername) > 0 {
					m.regUsername = m.regUsername[:len(m.regUsername)-1]
				} else if m.regFocusedField == 1 && len(m.regPassword) > 0 {
					m.regPassword = m.regPassword[:len(m.regPassword)-1]
				}
			}

		// Regular character input
		default:
			if m.state == registerView && len(msg.String()) == 1 {
				if m.regFocusedField == 0 {
					m.regUsername += msg.String()
				} else if m.regFocusedField == 1 {
					m.regPassword += msg.String()
				}
			}
		}
	}

	// Return the updated model to the Bubble Tea runtime for processing.
	// Note that we're not returning a command.
	return m, nil
}

func (m model) View() string {
	switch m.state {
	case menuView:
		return m.renderMenu()
	case loginView:
		return m.renderLogin()
	case registerView:
		return m.renderRegister()
	default:
		return ""
	}
}

func (m model) renderMenu() string {
	s := "Welcome! Please select an option:\n\n"

	// Iterate over our choices
	for i, choice := range m.choices {
		// Is the cursor pointing at this choice?
		cursor := " "
		if m.cursor == i {
			cursor = ">"
		}
		// Render the row
		s += fmt.Sprintf("%s %s\n", cursor, choice)
	}

	// The footer
	s += "\nUse arrow keys to navigate, Enter to select, q to quit.\n"

	return s
}

func main() {
	// Initialize database connection
	if err := InitDB(); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	defer CloseDB()

	//Clear terminal before starting TUI
	cmd := exec.Command("clear")
	cmd.Stdout = os.Stdout
	if err := cmd.Run(); err != nil {
		log.Fatal(err)
	}

	p := tea.NewProgram(initialModel())
	if _, err := p.Run(); err != nil {
		fmt.Printf("Alas, there's been an error: %v", err)
		os.Exit(1)
	}
}
