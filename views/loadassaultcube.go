package views

import (
	"crypto/md5"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
)

// Known Assault Cube version checksums (example - you'll need to update these)
var knownVersions = map[string]string{
	"1.3.0.2": "your_sha256_checksum_here", // Replace with actual checksum
	// Add more versions as needed
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
}

// NewLoadAssaultCubeModel creates a new assault cube loader model
func NewLoadAssaultCubeModel(username string) LoadAssaultCubeModel {
	return LoadAssaultCubeModel{
		Username:      username,
		GamePath:      "",
		GameFound:     false,
		GameVersion:   "",
		ChecksumValid: false,
		Error:         "",
		Status:        "Ready to search for Assault Cube",
		Searching:     false,
	}
}

// Update handles assault cube loader input
func (m LoadAssaultCubeModel) Update(msg tea.KeyMsg) (LoadAssaultCubeModel, tea.Cmd, ViewTransition) {
	switch msg.String() {
	case "enter", "l":
		// Trigger game search and validation
		m = m.FindAndValidateGame()
	case "s":
		// Launch game (only if valid)
		if m.GameFound && m.ChecksumValid {
			m.Status = "Launching Assault Cube..."
			// TODO: Implement DLL injection and launch
		} else {
			m.Error = "Cannot launch - game not validated"
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
	} else {
		s.WriteString(m.Status)
	}
	s.WriteString("\n\n")

	// Game detection results
	if m.GameFound {
		s.WriteString(SuccessStyle.Render("✓ Game Found"))
		s.WriteString("\n")
		s.WriteString(fmt.Sprintf("Path: %s\n", m.GamePath))
		s.WriteString(fmt.Sprintf("Version: %s\n", m.GameVersion))
		s.WriteString("\n")

		if m.ChecksumValid {
			s.WriteString(SuccessStyle.Render("✓ Checksum Valid - Ready to Launch"))
			s.WriteString("\n\n")
			launchBtn := FocusedButtonStyle.Render("[ S ] Start Game")
			s.WriteString(launchBtn)
		} else {
			s.WriteString(ErrorStyle.Render("❌ Checksum Invalid"))
			s.WriteString("\n")
			s.WriteString("The game executable doesn't match known versions.\n")
			s.WriteString("This could indicate:\n")
			s.WriteString("  • Wrong game version\n")
			s.WriteString("  • Modified executable\n")
			s.WriteString("  • Corrupted installation\n")
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
