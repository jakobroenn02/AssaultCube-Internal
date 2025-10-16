package main

import (
	"fmt"
	"log"
	"os"
	"runtime"
	"syscall"
	"unsafe"

	"go-project/injection"
	"go-project/views"

	tea "github.com/charmbracelet/bubbletea"
)

var (
	kernel32                       = syscall.NewLazyDLL("kernel32.dll")
	procGetStdHandle               = kernel32.NewProc("GetStdHandle")
	procGetConsoleScreenBufferInfo = kernel32.NewProc("GetConsoleScreenBufferInfo")
	procFillConsoleOutputCharacter = kernel32.NewProc("FillConsoleOutputCharacterW")
	procFillConsoleOutputAttribute = kernel32.NewProc("FillConsoleOutputAttribute")
	procSetConsoleCursorPosition   = kernel32.NewProc("SetConsoleCursorPosition")
)

const (
	STD_OUTPUT_HANDLE = ^uintptr(10) + 1 // -11 but uintptr
)

type coord struct {
	X int16
	Y int16
}

type smallRect struct {
	Left   int16
	Top    int16
	Right  int16
	Bottom int16
}

type consoleScreenBufferInfo struct {
	Size              coord
	CursorPosition    coord
	Attributes        uint16
	Window            smallRect
	MaximumWindowSize coord
}

// clearScreen clears the terminal screen using Windows API (no shell execution)
func clearScreen() {
	if runtime.GOOS != "windows" {
		// Fallback to ANSI escape codes for non-Windows
		fmt.Print("\033[2J\033[H")
		return
	}

	// Get console handle
	handle, _, _ := procGetStdHandle.Call(STD_OUTPUT_HANDLE)
	if handle == 0 {
		// Fallback if we can't get handle
		fmt.Print("\033[2J\033[H")
		return
	}

	// Get console screen buffer info
	var csbi consoleScreenBufferInfo
	ret, _, _ := procGetConsoleScreenBufferInfo.Call(
		handle,
		uintptr(unsafe.Pointer(&csbi)),
	)
	if ret == 0 {
		// Fallback if we can't get buffer info
		fmt.Print("\033[2J\033[H")
		return
	}

	// Calculate number of cells in the console
	cellCount := uint32(csbi.Size.X) * uint32(csbi.Size.Y)

	// Fill console with spaces
	var written uint32
	homeCoord := coord{X: 0, Y: 0}
	procFillConsoleOutputCharacter.Call(
		handle,
		uintptr(' '),
		uintptr(cellCount),
		*(*uintptr)(unsafe.Pointer(&homeCoord)),
		uintptr(unsafe.Pointer(&written)),
	)

	// Reset attributes
	procFillConsoleOutputAttribute.Call(
		handle,
		uintptr(csbi.Attributes),
		uintptr(cellCount),
		*(*uintptr)(unsafe.Pointer(&homeCoord)),
		uintptr(unsafe.Pointer(&written)),
	)

	// Move cursor to top-left
	procSetConsoleCursorPosition.Call(
		handle,
		*(*uintptr)(unsafe.Pointer(&homeCoord)),
	)
}

// model is the main application model that orchestrates different views
type model struct {
	currentView          views.ViewType
	currentUser          string // Track the logged-in username
	menuModel            views.MenuModel
	loginModel           views.LoginModel
	registerModel        views.RegisterModel
	dashboardModel       views.DashboardModel
	loadAssaultCubeModel views.LoadAssaultCubeModel
	resetPasswordModel   views.ResetPasswordModel
}

// initialModel creates the initial application model
func initialModel() model {
	return model{
		currentView:          views.MenuViewType,
		currentUser:          "",
		menuModel:            views.NewMenuModel(),
		loginModel:           views.NewLoginModel(DB),
		registerModel:        views.NewRegisterModel(DB),
		dashboardModel:       views.NewDashboardModel(""),
		loadAssaultCubeModel: views.NewLoadAssaultCubeModel(""),
		resetPasswordModel:   views.NewResetPasswordModel("", DB),
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
			// Go back logic
			switch m.currentView {
			case views.DashboardViewType:
				// From dashboard, logout and go to menu
				m.currentView = views.MenuViewType
				m.currentUser = ""
				m.menuModel = m.menuModel.Reset()
				m.loginModel = m.loginModel.Reset()
			case views.LoadAssaultCubeViewType, views.ResetPasswordViewType:
				// From sub-views, go back to dashboard
				m.currentView = views.DashboardViewType
			case views.MenuViewType:
				// Already at menu, do nothing
			default:
				// From login/register, go to menu
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
				// If transitioning to dashboard, save username and update dashboard
				if transition.TargetView == views.DashboardViewType {
					m.currentUser = m.loginModel.LoggedInUser
					m.dashboardModel = m.dashboardModel.SetUsername(m.currentUser)
				}
			}
			return m, cmd

		case views.RegisterViewType:
			var cmd tea.Cmd
			m.registerModel, cmd, transition = m.registerModel.Update(msg)
			if transition.ShouldTransition {
				m.currentView = transition.TargetView
			}
			return m, cmd

		case views.DashboardViewType:
			var cmd tea.Cmd
			m.dashboardModel, cmd, transition = m.dashboardModel.Update(msg)
			if transition.ShouldTransition {
				m.currentView = transition.TargetView
				// Update sub-views with current username
				if transition.TargetView == views.LoadAssaultCubeViewType {
					m.loadAssaultCubeModel = m.loadAssaultCubeModel.SetUsername(m.currentUser)
					m.loadAssaultCubeModel = m.loadAssaultCubeModel.Reset()
				} else if transition.TargetView == views.ResetPasswordViewType {
					m.resetPasswordModel = m.resetPasswordModel.SetUsername(m.currentUser)
					m.resetPasswordModel = m.resetPasswordModel.Reset()
				} else if transition.TargetView == views.MenuViewType {
					// Logout
					m.currentUser = ""
					m.loginModel = m.loginModel.Reset()
				}
			}
			return m, cmd

		case views.LoadAssaultCubeViewType:
			var cmd tea.Cmd
			m.loadAssaultCubeModel, cmd, transition = m.loadAssaultCubeModel.Update(msg)
			if transition.ShouldTransition {
				m.currentView = transition.TargetView
			}
			return m, cmd

		case views.ResetPasswordViewType:
			var cmd tea.Cmd
			m.resetPasswordModel, cmd, transition = m.resetPasswordModel.Update(msg)
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
	case views.DashboardViewType:
		return m.dashboardModel.View()
	case views.LoadAssaultCubeViewType:
		return m.loadAssaultCubeModel.View()
	case views.ResetPasswordViewType:
		return m.resetPasswordModel.View()
	default:
		return "Unknown view"
	}
}

func main() {
	// Load configuration
	config, err := LoadConfig("appsettings.json")
	if err != nil {
		log.Fatalf("Failed to load configuration: %v", err)
	}

	// Initialize database connection
	if err := InitDB(config); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	defer CloseDB()

	// Clear terminal before starting TUI (cross-platform)
	clearScreen()

	// Ensure game process is terminated when application exits
	defer injection.TerminateActiveGame()

	// Start the Bubble Tea program
	p := tea.NewProgram(initialModel())
	if _, err := p.Run(); err != nil {
		fmt.Printf("Alas, there's been an error: %v", err)
		os.Exit(1)
	}
}
