package ipc

import "encoding/json"

// MessageType defines the type of message being sent
type MessageType string

const (
	// Messages from Trainer -> TUI
	MsgTypeStatus MessageType = "status" // Periodic status update
	MsgTypeLog    MessageType = "log"    // Log message
	MsgTypeError  MessageType = "error"  // Error message
	MsgTypeInit   MessageType = "init"   // Trainer initialized

	// Messages from TUI -> Trainer (future use)
	MsgTypeCommand MessageType = "command" // Command to trainer
)

// TrainerMessage is the base message structure
type TrainerMessage struct {
	Type    MessageType `json:"type"`
	Payload interface{} `json:"payload"`
}

// StatusPayload contains trainer status information
type StatusPayload struct {
	Health       int  `json:"health"`
	Armor        int  `json:"armor"`
	Ammo         int  `json:"ammo"`
	GodMode      bool `json:"god_mode"`
	InfiniteAmmo bool `json:"infinite_ammo"`
	NoRecoil     bool `json:"no_recoil"`
}

// LogPayload contains a log message
type LogPayload struct {
	Level   string `json:"level"` // "info", "warning", "error"
	Message string `json:"message"`
}

// ErrorPayload contains an error message
type ErrorPayload struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

// InitPayload contains initialization information
type InitPayload struct {
	ModuleBase  uint64 `json:"module_base"`
	PlayerBase  uint64 `json:"player_base"`
	Version     string `json:"version"`
	Initialized bool   `json:"initialized"`
}

// CreateStatusMessage creates a status message
func CreateStatusMessage(health, armor, ammo int, godMode, infiniteAmmo, noRecoil bool) []byte {
	msg := TrainerMessage{
		Type: MsgTypeStatus,
		Payload: StatusPayload{
			Health:       health,
			Armor:        armor,
			Ammo:         ammo,
			GodMode:      godMode,
			InfiniteAmmo: infiniteAmmo,
			NoRecoil:     noRecoil,
		},
	}

	data, _ := json.Marshal(msg)
	return append(data, '\n') // Add newline delimiter
}

// CreateLogMessage creates a log message
func CreateLogMessage(level, message string) []byte {
	msg := TrainerMessage{
		Type: MsgTypeLog,
		Payload: LogPayload{
			Level:   level,
			Message: message,
		},
	}

	data, _ := json.Marshal(msg)
	return append(data, '\n')
}

// CreateErrorMessage creates an error message
func CreateErrorMessage(code int, message string) []byte {
	msg := TrainerMessage{
		Type: MsgTypeError,
		Payload: ErrorPayload{
			Code:    code,
			Message: message,
		},
	}

	data, _ := json.Marshal(msg)
	return append(data, '\n')
}

// CreateInitMessage creates an initialization message
func CreateInitMessage(moduleBase, playerBase uint64, version string, initialized bool) []byte {
	msg := TrainerMessage{
		Type: MsgTypeInit,
		Payload: InitPayload{
			ModuleBase:  moduleBase,
			PlayerBase:  playerBase,
			Version:     version,
			Initialized: initialized,
		},
	}

	data, _ := json.Marshal(msg)
	return append(data, '\n')
}

// ParseMessage parses a JSON message into TrainerMessage
func ParseMessage(data []byte) (*TrainerMessage, error) {
	var msg TrainerMessage
	err := json.Unmarshal(data, &msg)
	if err != nil {
		return nil, err
	}

	// Convert the payload map back to the correct struct type
	switch msg.Type {
	case MsgTypeStatus:
		payloadBytes, _ := json.Marshal(msg.Payload)
		var status StatusPayload
		json.Unmarshal(payloadBytes, &status)
		msg.Payload = status

	case MsgTypeLog:
		payloadBytes, _ := json.Marshal(msg.Payload)
		var log LogPayload
		json.Unmarshal(payloadBytes, &log)
		msg.Payload = log

	case MsgTypeError:
		payloadBytes, _ := json.Marshal(msg.Payload)
		var errPayload ErrorPayload
		json.Unmarshal(payloadBytes, &errPayload)
		msg.Payload = errPayload

	case MsgTypeInit:
		payloadBytes, _ := json.Marshal(msg.Payload)
		var init InitPayload
		json.Unmarshal(payloadBytes, &init)
		msg.Payload = init
	}

	return &msg, nil
}
