package views

import tea "github.com/charmbracelet/bubbletea"

// ViewType represents different view types
type ViewType int

const (
	MenuViewType ViewType = iota
	LoginViewType
	RegisterViewType
)

// ViewTransition represents a view change request
type ViewTransition struct {
	ShouldTransition bool
	TargetView       ViewType
}

// TransitionTo creates a view transition
func TransitionTo(viewType ViewType) ViewTransition {
	return ViewTransition{
		ShouldTransition: true,
		TargetView:       viewType,
	}
}

// NoTransition returns no transition
func NoTransition() ViewTransition {
	return ViewTransition{
		ShouldTransition: false,
	}
}

// View is an interface that all views must implement
type View interface {
	View() string
	Update(msg tea.KeyMsg) (View, tea.Cmd, ViewTransition)
	Reset() View
}
