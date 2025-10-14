package views

import "github.com/charmbracelet/lipgloss"

// Color palette
var (
	primaryColor   = lipgloss.Color("205") // Pink
	secondaryColor = lipgloss.Color("99")  // Purple
	successColor   = lipgloss.Color("42")  // Green
	errorColor     = lipgloss.Color("196") // Red
	mutedColor     = lipgloss.Color("241") // Gray
	focusedColor   = lipgloss.Color("205") // Pink
	blurredColor   = lipgloss.Color("240") // Dark Gray
)

// Common styles
var (
	// Title styles
	TitleStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(primaryColor).
			MarginBottom(1).
			Padding(0, 1)

	// Subtitle styles
	SubtitleStyle = lipgloss.NewStyle().
			Foreground(secondaryColor).
			MarginBottom(1)

	// Focused input style
	FocusedStyle = lipgloss.NewStyle().
			Foreground(focusedColor).
			Bold(true)

	// Blurred input style
	BlurredStyle = lipgloss.NewStyle().
			Foreground(blurredColor)

	// Error message style
	ErrorStyle = lipgloss.NewStyle().
			Foreground(errorColor).
			Bold(true).
			MarginTop(1).
			MarginBottom(1)

	// Success message style
	SuccessStyle = lipgloss.NewStyle().
			Foreground(successColor).
			Bold(true).
			MarginTop(1).
			MarginBottom(1)

	// Help text style
	HelpStyle = lipgloss.NewStyle().
			Foreground(mutedColor).
			MarginTop(1)

	// Menu item styles
	SelectedItemStyle = lipgloss.NewStyle().
				Foreground(focusedColor).
				Bold(true).
				PaddingLeft(2)

	UnselectedItemStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color("252")).
				PaddingLeft(4)

	// Container/box styles
	BoxStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(primaryColor).
			Padding(1, 2).
			MarginTop(1)

	// Button styles
	FocusedButtonStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color("0")).
				Background(focusedColor).
				Padding(0, 3).
				Bold(true)

	BlurredButtonStyle = lipgloss.NewStyle().
				Foreground(lipgloss.Color("252")).
				Background(lipgloss.Color("236")).
				Padding(0, 3)
)
