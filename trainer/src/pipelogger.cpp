#include "pch.h"
#include "pipelogger.h"
#include <sstream>
#include <iostream>
#include <thread>

#define PIPE_NAME TEXT("\\\\.\\pipe\\actrainer_pipe")

PipeLogger::PipeLogger() 
    : pipeHandle(INVALID_HANDLE_VALUE), connected(false) {
}

PipeLogger::~PipeLogger() {
    if (pipeHandle != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(pipeHandle);
        DisconnectNamedPipe(pipeHandle);
        CloseHandle(pipeHandle);
    }
}

bool PipeLogger::Initialize() {
    // Create named pipe for communication with TUI  
    pipeHandle = CreateNamedPipe(
        PIPE_NAME,                      // Pipe name
        PIPE_ACCESS_DUPLEX,             // Read/write access
        PIPE_TYPE_MESSAGE |             // Message type pipe
        PIPE_READMODE_MESSAGE |         // Message-read mode
        PIPE_WAIT,                      // Blocking mode
        PIPE_UNLIMITED_INSTANCES,       // Allow multiple instances
        4096,                           // Output buffer size
        4096,                           // Input buffer size
        0,                              // Default timeout
        NULL                            // Default security
    );
    
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "Named pipe created, starting connection thread..." << std::endl;
    
    // Start a background thread to wait for client connection
    // This prevents blocking the main trainer thread
    std::thread([this]() {
        BOOL connected = ConnectNamedPipe(pipeHandle, NULL);
        DWORD error = GetLastError();
        
        if (!connected && error != ERROR_PIPE_CONNECTED) {
            std::cerr << "ConnectNamedPipe failed. Error: " << error << std::endl;
            return;
        }
        
        std::cout << "TUI client connected to pipe!" << std::endl;
        this->connected = true;
    }).detach();
    
    return true;
}

bool PipeLogger::SendMessage(const std::string& json) {
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Add newline delimiter
    std::string message = json + "\n";
    
    DWORD bytesWritten;
    BOOL success = WriteFile(
        pipeHandle,
        message.c_str(),
        (DWORD)message.length(),
        &bytesWritten,
        NULL
    );
    
    if (!success) {
        DWORD error = GetLastError();
        // Pipe not yet connected or disconnected
        if (error == ERROR_NO_DATA || error == ERROR_PIPE_NOT_CONNECTED || 
            error == ERROR_PIPE_LISTENING) {
            connected = false;
            return false;
        }
        return false;
    }
    
    if (bytesWritten != message.length()) {
        return false;
    }
    
    // Mark as connected if write succeeded
    if (!connected) {
        std::cout << "TUI client connected!" << std::endl;
        connected = true;
    }
    
    // Flush to ensure message is sent immediately
    FlushFileBuffers(pipeHandle);
    return true;
}

std::string PipeLogger::EscapeJson(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c < 0x20) {
                    // Control character - encode as unicode
                    char buf[8];
                    sprintf_s(buf, "\\u%04x", (unsigned char)c);
                    escaped += buf;
                } else {
                    escaped += c;
                }
                break;
        }
    }
    return escaped;
}

void PipeLogger::SendStatus(int health, int armor, int ammo, bool godMode, bool infiniteAmmo, bool noRecoil) {
    std::ostringstream json;
    json << "{\"type\":\"status\",\"payload\":{"
         << "\"health\":" << health << ","
         << "\"armor\":" << armor << ","
         << "\"ammo\":" << ammo << ","
         << "\"god_mode\":" << (godMode ? "true" : "false") << ","
         << "\"infinite_ammo\":" << (infiniteAmmo ? "true" : "false") << ","
         << "\"no_recoil\":" << (noRecoil ? "true" : "false")
         << "}}";
    
    SendMessage(json.str());
}

void PipeLogger::SendLog(const std::string& level, const std::string& message) {
    std::ostringstream json;
    json << "{\"type\":\"log\",\"payload\":{"
         << "\"level\":\"" << EscapeJson(level) << "\","
         << "\"message\":\"" << EscapeJson(message) << "\""
         << "}}";
    
    SendMessage(json.str());
}

void PipeLogger::SendError(int code, const std::string& message) {
    std::ostringstream json;
    json << "{\"type\":\"error\",\"payload\":{"
         << "\"code\":" << code << ","
         << "\"message\":\"" << EscapeJson(message) << "\""
         << "}}";
    
    SendMessage(json.str());
}

void PipeLogger::SendInit(uintptr_t moduleBase, uintptr_t playerBase, const std::string& version, bool initialized) {
    std::ostringstream json;
    json << "{\"type\":\"init\",\"payload\":{"
         << "\"module_base\":" << moduleBase << ","
         << "\"player_base\":" << playerBase << ","
         << "\"version\":\"" << EscapeJson(version) << "\","
         << "\"initialized\":" << (initialized ? "true" : "false")
         << "}}";
    
    SendMessage(json.str());
}
