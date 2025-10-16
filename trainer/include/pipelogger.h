#pragma once
#include <Windows.h>
#include <string>

// Named Pipe Logger - sends messages to the TUI loader
class PipeLogger {
public:
    PipeLogger();
    ~PipeLogger();
    
    // Initialize the pipe server
    bool Initialize();
    
    // Send messages
    void SendStatus(int health, int armor, int ammo, bool godMode, bool infiniteAmmo, bool noRecoil);
    void SendLog(const std::string& level, const std::string& message);
    void SendError(int code, const std::string& message);
    void SendInit(uintptr_t moduleBase, uintptr_t playerBase, const std::string& version, bool initialized);
    
    // Check if pipe is connected
    bool IsConnected() const { return pipeHandle != INVALID_HANDLE_VALUE; }
    
private:
    HANDLE pipeHandle;
    bool connected;
    
    // Helper to send JSON message
    bool SendMessage(const std::string& json);
    
    // Helper to escape JSON strings
    std::string EscapeJson(const std::string& str);
};
