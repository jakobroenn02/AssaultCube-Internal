package views

import (
	"crypto/md5"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"go-project/injection"
	"go-project/ipc"
	"io"
	"os"
	"path/filepath"
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"
)

// Known Assault Cube version checksums (example - you'll need to update these)
var knownVersions = map[string]string{
	"1.3.0.2": "your_sha256_checksum_here", // Replace with actual checksum
	// Add more versions as needed
}

// Helper functions for formatting
func formatStatValue(value int) string {
	if value > 0 {
		return SuccessStyle.Render(fmt.Sprintf("%d", value))
	}
	return ErrorStyle.Render(fmt.Sprintf("%d", value))
}

func formatFeatureStatus(enabled bool) string {
	if enabled {
		return SuccessStyle.Render("ON")
	}
	return HelpStyle.Render("OFF")
}

// Message types for IPC updates
type trainerStatusMsg struct {
	health       int
	armor        int
	ammo         int
	godMode      bool
	infiniteAmmo bool
	noRecoil     bool
}

type trainerLogMsg struct {
	message string
}

type trainerConnectedMsg struct {
	client *ipc.PipeClient
}

// listenToPipe reads messages from the trainer and sends them as tea.Msg
func listenToPipe() tea.Cmd {
	return func() tea.Msg {
		// Try to connect to the pipe
		_, err := ipc.Connect(10 * time.Second)
		if err != nil {
			return trainerLogMsg{message: fmt.Sprintf("Failed to connect to trainer: %v", err)}
		}

		// Signal that we're connected
		return trainerConnectedMsg{}
	}
}

// readPipeMessages continuously reads messages from the pipe
func readPipeMessages(client *ipc.PipeClient) tea.Cmd {
	return func() tea.Msg {
		msg, err := client.ReadMessage()
		if err != nil {
			return trainerLogMsg{message: fmt.Sprintf("Pipe error: %v", err)}
		}

		// Handle different message types
		switch msg.Type {
		case ipc.MsgTypeStatus:
			if status, ok := msg.Payload.(ipc.StatusPayload); ok {
				return trainerStatusMsg{
					health:       status.Health,
					armor:        status.Armor,
					ammo:         status.Ammo,
					godMode:      status.GodMode,
					infiniteAmmo: status.InfiniteAmmo,
					noRecoil:     status.NoRecoil,
				}
			}

		case ipc.MsgTypeLog:
			if log, ok := msg.Payload.(ipc.LogPayload); ok {
				return trainerLogMsg{message: fmt.Sprintf("[%s] %s", log.Level, log.Message)}
			}

		case ipc.MsgTypeError:
			if errPayload, ok := msg.Payload.(ipc.ErrorPayload); ok {
				return trainerLogMsg{message: fmt.Sprintf("ERROR: %s", errPayload.Message)}
			}

		case ipc.MsgTypeInit:
			if init, ok := msg.Payload.(ipc.InitPayload); ok {
				return trainerLogMsg{message: fmt.Sprintf("Trainer v%s initialized", init.Version)}
			}
		}

		return nil
	}
}

// LoadAssaultCubeModel represents the assault cube loader view
type LoadAssaultCubeModel struct {
	Username      string
	GamePath      string
	GameFound     bool
	GameVersion   string
	ChecksumValid bool
	Error         string
	Status        string
	Searching     bool
	Launching     bool
	Injected      bool
	DLLPath       string

	// Real-time trainer data (from IPC)
	Health       int
	Armor        int
	Ammo         int
	GodMode      bool
	InfiniteAmmo bool
	NoRecoil     bool
	TrainerLogs  []string
	Connected    bool

	// IPC connection
	pipeClient *ipc.PipeClient
}

// NewLoadAssaultCubeModel creates a new assault cube loader model
func NewLoadAssaultCubeModel(username string) LoadAssaultCubeModel {
	// Get the DLL path (assuming it's in trainer/actrainer.dll)
	dllPath := filepath.Join("trainer", "actrainer.dll")
	absPath, _ := filepath.Abs(dllPath)

	return LoadAssaultCubeModel{
		Username:      username,
		GamePath:      "",
		GameFound:     false,
		GameVersion:   "",
		ChecksumValid: false,
		Error:         "",
		Status:        "Ready to search for Assault Cube",
		Searching:     false,
		Launching:     false,
		Injected:      false,
		DLLPath:       absPath,
		Health:        0,
		Armor:         0,
		Ammo:          0,
		GodMode:       false,
		InfiniteAmmo:  false,
		NoRecoil:      false,
		TrainerLogs:   make([]string, 0),
		Connected:     false,
	}
}

// Update handles all messages (keyboard input and IPC updates)
func (m LoadAssaultCubeModel) Update(msg tea.Msg) (LoadAssaultCubeModel, tea.Cmd, ViewTransition) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		return m.handleKeyPress(msg)

	case trainerConnectedMsg:
		// Pipe connected! Start reading messages
		m.Connected = true
		m.pipeClient = msg.client
		m.TrainerLogs = append(m.TrainerLogs, "Connected to trainer")
		return m, UpdateFromPipe(m.pipeClient), NoTransition()

	case trainerStatusMsg:
		// Update stats from trainer
		m.Health = msg.health
		m.Armor = msg.armor
		m.Ammo = msg.ammo
		m.GodMode = msg.godMode
		m.InfiniteAmmo = msg.infiniteAmmo
		m.NoRecoil = msg.noRecoil

		// Continue reading from pipe
		if m.pipeClient != nil {
			return m, UpdateFromPipe(m.pipeClient), NoTransition()
		}

	case trainerLogMsg:
		// Add log message
		m.TrainerLogs = append(m.TrainerLogs, msg.message)

		// Keep only last 10 logs
		if len(m.TrainerLogs) > 10 {
			m.TrainerLogs = m.TrainerLogs[len(m.TrainerLogs)-10:]
		}

		// Continue reading from pipe
		if m.pipeClient != nil {
			return m, UpdateFromPipe(m.pipeClient), NoTransition()
		}
	}

	return m, nil, NoTransition()
}

// handleKeyPress handles keyboard input
func (m LoadAssaultCubeModel) handleKeyPress(msg tea.KeyMsg) (LoadAssaultCubeModel, tea.Cmd, ViewTransition) {
	switch msg.String() {
	case "enter", "l":
		// Trigger game search and validation
		m = m.FindAndValidateGame()
	case "s":
		// Launch game (only if valid)
		if m.GameFound {
			m = m.LaunchAndInject()
			// Start IPC connection after injection
			if m.Injected && m.Error == "" {
				return m, StartIPCConnection, NoTransition()
			}
		} else {
			m.Error = "Cannot launch - game not found"
		}
	}

	return m, nil, NoTransition()
}

// View renders the assault cube loader view
func (m LoadAssaultCubeModel) View() string {
	var s strings.Builder

	// Title
	title := TitleStyle.Render("🎮 Assault Cube Loader")
	s.WriteString(title)
	s.WriteString("\n\n")

	// User info
	userInfo := SubtitleStyle.Render("User: " + m.Username)
	s.WriteString(userInfo)
	s.WriteString("\n\n")

	// Status
	s.WriteString("Status: ")
	if m.Searching {
		s.WriteString(SubtitleStyle.Render("🔍 Searching for game..."))
	} else if m.Launching {
		s.WriteString(SubtitleStyle.Render("⏳ Launching game..."))
	} else {
		s.WriteString(m.Status)
	}
	s.WriteString("\n\n")

	// Show error if present
	if m.Error != "" {
		s.WriteString(ErrorStyle.Render("❌ Error: " + m.Error))
		s.WriteString("\n\n")
	}

	// Game detection results
	if m.GameFound {
		s.WriteString(SuccessStyle.Render("✓ Game Found"))
		s.WriteString("\n")
		s.WriteString(fmt.Sprintf("Path: %s\n", m.GamePath))
		s.WriteString(fmt.Sprintf("Version: %s\n", m.GameVersion))
		s.WriteString("\n")

		// Show DLL status
		if _, err := os.Stat(m.DLLPath); err == nil {
			s.WriteString(SuccessStyle.Render("✓ Trainer DLL Ready"))
			s.WriteString("\n")
		} else {
			s.WriteString(ErrorStyle.Render("❌ Trainer DLL Not Found"))
			s.WriteString("\n")
			s.WriteString(fmt.Sprintf("Expected at: %s\n", m.DLLPath))
		}
		s.WriteString("\n")

		if m.Injected {
			s.WriteString(SuccessStyle.Render("✓ Trainer Active!"))
			s.WriteString("\n\n")

			// Show connection status
			if m.Connected {
				s.WriteString(SuccessStyle.Render("✓ Connected to Trainer"))
				s.WriteString("\n\n")

				// Real-time stats
				s.WriteString(SubtitleStyle.Render("=== Live Stats ==="))
				s.WriteString("\n")
				s.WriteString(fmt.Sprintf("Health: %s\n", formatStatValue(m.Health)))
				s.WriteString(fmt.Sprintf("Armor:  %s\n", formatStatValue(m.Armor)))
				s.WriteString(fmt.Sprintf("Ammo:   %s\n", formatStatValue(m.Ammo)))
				s.WriteString("\n")

				// Feature status
				s.WriteString(SubtitleStyle.Render("=== Features ==="))
				s.WriteString("\n")
				s.WriteString(fmt.Sprintf("God Mode:      %s\n", formatFeatureStatus(m.GodMode)))
				s.WriteString(fmt.Sprintf("Infinite Ammo: %s\n", formatFeatureStatus(m.InfiniteAmmo)))
				s.WriteString(fmt.Sprintf("No Recoil:     %s\n", formatFeatureStatus(m.NoRecoil)))
				s.WriteString("\n")

				// Recent logs
				if len(m.TrainerLogs) > 0 {
					s.WriteString(SubtitleStyle.Render("=== Recent Activity ==="))
					s.WriteString("\n")
					// Show last 5 logs
					start := 0
					if len(m.TrainerLogs) > 5 {
						start = len(m.TrainerLogs) - 5
					}
					for i := start; i < len(m.TrainerLogs); i++ {
						s.WriteString(fmt.Sprintf("• %s\n", m.TrainerLogs[i]))
					}
					s.WriteString("\n")
				}
			} else {
				s.WriteString(SubtitleStyle.Render("⏳ Waiting for trainer connection..."))
				s.WriteString("\n\n")

				// Show connection errors if any
				if len(m.TrainerLogs) > 0 {
					s.WriteString(ErrorStyle.Render("Connection Logs:"))
					s.WriteString("\n")
					// Show last 5 logs
					start := 0
					if len(m.TrainerLogs) > 5 {
						start = len(m.TrainerLogs) - 5
					}
					for i := start; i < len(m.TrainerLogs); i++ {
						s.WriteString(fmt.Sprintf("• %s\n", m.TrainerLogs[i]))
					}
					s.WriteString("\n")
				}
			}

			s.WriteString("Use hotkeys in-game:\n")
			s.WriteString("  F1  - God Mode\n")
			s.WriteString("  F2  - Infinite Ammo\n")
			s.WriteString("  F3  - No Recoil\n")
			s.WriteString("  F4  - Add Health\n")
			s.WriteString("  END - Unload Trainer\n")
		} else if m.Launching {
			s.WriteString(SubtitleStyle.Render("⏳ Launching game..."))
		} else if m.Error != "" {
			// Show error and still allow retry
			launchBtn := BlurredButtonStyle.Render("[ S ] Retry Start Game")
			s.WriteString(launchBtn)
		} else {
			launchBtn := FocusedButtonStyle.Render("[ S ] Start Game with Trainer")
			s.WriteString(launchBtn)
		}
	} else if m.Error != "" {
		s.WriteString(ErrorStyle.Render("❌ Error: " + m.Error))
		s.WriteString("\n\n")
		s.WriteString("Please ensure Assault Cube is installed in one of these locations:\n")
		s.WriteString("  • C:\\Program Files\\AssaultCube\n")
		s.WriteString("  • C:\\Program Files (x86)\\AssaultCube\n")
		s.WriteString("  • C:\\Games\\AssaultCube\n")
		s.WriteString("  • Desktop\\AssaultCube\n")
	} else {
		searchBtn := BlurredButtonStyle.Render("[ Enter ] Search for Game")
		s.WriteString(searchBtn)
	}

	// Help text
	s.WriteString("\n\n")
	helpText := HelpStyle.Render("enter/l search • s start (if valid) • esc back")
	s.WriteString(helpText)

	// Wrap in a box
	content := s.String()
	box := BoxStyle.Render(content)

	return "\n" + box + "\n"
}

// Reset resets the assault cube loader state
func (m LoadAssaultCubeModel) Reset() LoadAssaultCubeModel {
	m.GamePath = ""
	m.GameFound = false
	m.GameVersion = ""
	m.ChecksumValid = false
	m.Error = ""
	m.Status = "Ready to search for Assault Cube"
	m.Searching = false
	return m
}

// SetUsername updates the username
func (m LoadAssaultCubeModel) SetUsername(username string) LoadAssaultCubeModel {
	m.Username = username
	return m
}

// FindAndValidateGame searches for Assault Cube and validates it
func (m LoadAssaultCubeModel) FindAndValidateGame() LoadAssaultCubeModel {
	m.Searching = true
	m.Error = ""
	m.GameFound = false
	m.ChecksumValid = false

	// Common installation paths for Assault Cube
	searchPaths := []string{
		`C:\Program Files\AssaultCube\bin_win32\ac_client.exe`,
		`C:\Program Files (x86)\AssaultCube\bin_win32\ac_client.exe`,
		`C:\Games\AssaultCube\bin_win32\ac_client.exe`,
		`C:\Program Files (x86)\AssaultCube 1.3.0.2\bin_win32\ac_client.exe`,
		filepath.Join(os.Getenv("USERPROFILE"), `Desktop\AssaultCube\bin_win32\ac_client.exe`),
		filepath.Join(os.Getenv("USERPROFILE"), `Documents\AssaultCube\bin_win32\ac_client.exe`),
	}

	// Search for the game executable
	for _, path := range searchPaths {
		if _, err := os.Stat(path); err == nil {
			m.GamePath = path
			m.GameFound = true
			m.Status = "Game found, validating..."
			break
		}
	}

	if !m.GameFound {
		m.Error = "Assault Cube executable not found"
		m.Status = "Game not found"
		m.Searching = false
		return m
	}

	// Validate checksum
	checksum, err := m.calculateChecksum(m.GamePath)
	if err != nil {
		m.Error = fmt.Sprintf("Failed to calculate checksum: %v", err)
		m.Status = "Validation failed"
		m.Searching = false
		return m
	}

	// Check against known versions
	m.GameVersion = "Unknown"
	m.ChecksumValid = false

	for version, knownChecksum := range knownVersions {
		if checksum == knownChecksum {
			m.GameVersion = version
			m.ChecksumValid = true
			m.Status = "Validation successful"
			break
		}
	}

	if !m.ChecksumValid {
		// For development/testing, you might want to allow unknown versions
		// Remove this in production
		m.GameVersion = "Unverified"
		m.Error = fmt.Sprintf("Unknown version (checksum: %s)", checksum[:16]+"...")
		m.Status = "Version not recognized"
	}

	m.Searching = false
	return m
}

// calculateChecksum calculates SHA256 checksum of a file
func (m LoadAssaultCubeModel) calculateChecksum(filepath string) (string, error) {
	file, err := os.Open(filepath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	hash := sha256.New()
	if _, err := io.Copy(hash, file); err != nil {
		return "", err
	}

	return hex.EncodeToString(hash.Sum(nil)), nil
}

// calculateMD5 calculates MD5 checksum (alternative, less secure but faster)
func (m LoadAssaultCubeModel) calculateMD5(filepath string) (string, error) {
	file, err := os.Open(filepath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	hash := md5.New()
	if _, err := io.Copy(hash, file); err != nil {
		return "", err
	}

	return hex.EncodeToString(hash.Sum(nil)), nil
}

// LaunchAndInject launches the game and injects the trainer DLL
func (m LoadAssaultCubeModel) LaunchAndInject() LoadAssaultCubeModel {
	m.Launching = true
	m.Status = "Launching Assault Cube..."
	m.Error = ""

	// Check if DLL exists
	if _, err := os.Stat(m.DLLPath); os.IsNotExist(err) {
		m.Error = fmt.Sprintf("Trainer DLL not found at: %s", m.DLLPath)
		m.Status = "Launch failed"
		m.Launching = false
		return m
	}

	// Launch the game and inject DLL
	err := injection.LaunchAndInject(m.GamePath, m.DLLPath)
	if err != nil {
		m.Error = fmt.Sprintf("Failed to inject trainer: %v", err)
		m.Status = "Injection failed"
		m.Launching = false
		return m
	}

	// Give the DLL a moment to initialize and create the pipe
	time.Sleep(500 * time.Millisecond)

	m.Status = "✓ Trainer injected successfully!"
	m.Injected = true
	m.Launching = false

	return m
}

// StartIPCConnection initiates the connection to the trainer's named pipe
// This should be called as a tea.Cmd after successful injection
func StartIPCConnection() tea.Msg {
	// Try to connect to the pipe (with timeout)
	client, err := ipc.Connect(10 * time.Second)
	if err != nil {
		return trainerLogMsg{message: fmt.Sprintf("Failed to connect to trainer: %v", err)}
	}

	// Connection successful!
	return trainerConnectedMsg{client: client}
}

// UpdateFromPipe reads a message from the pipe and returns it as a tea.Msg
func UpdateFromPipe(client *ipc.PipeClient) tea.Cmd {
	return func() tea.Msg {
		msg, err := client.ReadMessage()
		if err != nil {
			return trainerLogMsg{message: fmt.Sprintf("Pipe disconnected: %v", err)}
		}

		// Handle different message types
		switch msg.Type {
		case ipc.MsgTypeStatus:
			if status, ok := msg.Payload.(ipc.StatusPayload); ok {
				return trainerStatusMsg{
					health:       status.Health,
					armor:        status.Armor,
					ammo:         status.Ammo,
					godMode:      status.GodMode,
					infiniteAmmo: status.InfiniteAmmo,
					noRecoil:     status.NoRecoil,
				}
			}

		case ipc.MsgTypeLog:
			if log, ok := msg.Payload.(ipc.LogPayload); ok {
				return trainerLogMsg{message: fmt.Sprintf("[%s] %s", log.Level, log.Message)}
			}

		case ipc.MsgTypeError:
			if errPayload, ok := msg.Payload.(ipc.ErrorPayload); ok {
				return trainerLogMsg{message: fmt.Sprintf("ERROR: %s", errPayload.Message)}
			}

		case ipc.MsgTypeInit:
			if init, ok := msg.Payload.(ipc.InitPayload); ok {
				return trainerLogMsg{message: fmt.Sprintf("Trainer v%s initialized", init.Version)}
			}
		}

		return nil
	}
}
