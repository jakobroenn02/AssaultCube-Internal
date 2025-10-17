#include "pch.h"
#include "trainer.h"
#include "memory.h"
#include <iostream>
#include <fstream>

// Helper to write to log file
void LogToFile(const std::string& message) {
    // Write to user's temp folder - guaranteed writable
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string logPath = std::string(tempPath) + "trainer_debug.txt";
    
    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}

// Clear console using Windows API (better than system("cls"))
void ClearConsole() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD cellCount;
    DWORD count;
    COORD homeCoords = { 0, 0 };
    
    if (hConsole == INVALID_HANDLE_VALUE) return;
    
    // Get console size
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    
    // Fill console with spaces
    if (!FillConsoleOutputCharacter(hConsole, (TCHAR)' ', cellCount, homeCoords, &count)) return;
    
    // Fill attributes
    if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cellCount, homeCoords, &count)) return;
    
    // Move cursor to top-left
    SetConsoleCursorPosition(hConsole, homeCoords);
}

Trainer::Trainer(uintptr_t base)
    : moduleBase(base),
      isRunning(true),
      pipeLogger(nullptr),
      godMode(false),
      infiniteAmmo(false),
      noRecoil(false),
      prevF1State(false),
      prevF2State(false),
      prevF3State(false),
      prevF4State(false),
      prevEndState(false),
      playerBase(0),
      healthAddress(0),
      armorAddress(0),
      ammoAddress(0),
      lastPipeConnectionState(false) {
}

Trainer::~Trainer() {
    // Clean up pipe logger
    if (pipeLogger) {
        delete pipeLogger;
        pipeLogger = nullptr;
    }
    // Restore original bytes if needed
    // Clean up any patches
}

bool Trainer::Initialize() {
#ifdef _DEBUG
    std::cout << "Initializing trainer..." << std::endl;
#endif
    LogToFile("=== TRAINER INITIALIZING ===");
    
    // Initialize pipe logger
    pipeLogger = new PipeLogger();
    if (pipeLogger->Initialize()) {
        pipeLogger->SendLog("info", "Trainer initializing...");
    } else {
#ifdef _DEBUG
        std::cout << "WARNING: Failed to initialize pipe logger (TUI not connected)" << std::endl;
#endif
        LogToFile("WARNING: Pipe logger initialization failed");
    }
    
    // Read LocalPlayer pointer from ac_client.exe + 0x0017E0A8
    playerBase = Memory::Read<uintptr_t>(moduleBase + 0x0017E0A8);
    
    char buffer[256];
    sprintf_s(buffer, "Module Base: 0x%08X", moduleBase);
    LogToFile(buffer);
    sprintf_s(buffer, "Player Base: 0x%08X", playerBase);
    LogToFile(buffer);
    
    if (playerBase == 0) {
#ifdef _DEBUG
        std::cout << "ERROR: Failed to read player base from 0x" << std::hex << (moduleBase + 0x0017E0A8) << std::dec << std::endl;
#endif
        LogToFile("ERROR: Player base is NULL!");
        
        if (pipeLogger && pipeLogger->IsConnected()) {
            pipeLogger->SendError(1, "Failed to read player base - pointer is NULL");
        }
        return false;
    }
    
#ifdef _DEBUG
    std::cout << "Player base found at: 0x" << std::hex << playerBase << std::dec << std::endl;
#endif
    
    // Calculate addresses using found offsets
    healthAddress = playerBase + 0xEC;  // Health Value
    armorAddress = playerBase + 0xF0;   // Armor Value
    ammoAddress = playerBase + 0x140;   // Assault Rifle Ammo
    
    sprintf_s(buffer, "Health Address: 0x%08X", healthAddress);
    LogToFile(buffer);
    sprintf_s(buffer, "Armor Address: 0x%08X", armorAddress);
    LogToFile(buffer);
    sprintf_s(buffer, "Ammo Address: 0x%08X", ammoAddress);
    LogToFile(buffer);
    
#ifdef _DEBUG
    std::cout << "Health address: 0x" << std::hex << healthAddress << std::dec << std::endl;
    std::cout << "Armor address: 0x" << std::hex << armorAddress << std::dec << std::endl;
    std::cout << "Ammo address: 0x" << std::hex << ammoAddress << std::dec << std::endl;
#endif
    
    // Read and display current values to verify addresses are correct
    int currentHealth = Memory::Read<int>(healthAddress);
    int currentArmor = Memory::Read<int>(armorAddress);
    int currentAmmo = Memory::Read<int>(ammoAddress);
    
    sprintf_s(buffer, "Current Health: %d", currentHealth);
    LogToFile(buffer);
    sprintf_s(buffer, "Current Armor: %d", currentArmor);
    LogToFile(buffer);
    sprintf_s(buffer, "Current Ammo: %d", currentAmmo);
    LogToFile(buffer);
    
#ifdef _DEBUG
    std::cout << "\nCurrent values:" << std::endl;
    std::cout << "  Health: " << currentHealth << std::endl;
    std::cout << "  Armor: " << currentArmor << std::endl;
    std::cout << "  Ammo: " << currentAmmo << std::endl;
    
    std::cout << "\nTrainer initialized successfully!" << std::endl;
#endif
    LogToFile("=== TRAINER READY ===\n");
    
    // Send initialization message to TUI
    if (pipeLogger && pipeLogger->IsConnected()) {
        pipeLogger->SendInit(moduleBase, playerBase, "1.0.0", true);
        pipeLogger->SendLog("info", "Trainer initialized successfully!");
        BroadcastStatus();
    }

    return true;
}

void Trainer::Run() {
#ifdef _DEBUG
    std::cout << "\nTrainer is running...\n" << std::endl;
#endif

    int statusCounter = 0;
    const int STATUS_UPDATE_INTERVAL = 100; // Send status every 100 iterations (~1 second)

    while (isRunning) {
        if (pipeLogger) {
            bool pipeConnected = pipeLogger->IsConnected();
            if (pipeConnected && !lastPipeConnectionState) {
                pipeLogger->SendInit(moduleBase, playerBase, "1.0.0", true);
                pipeLogger->SendLog("info", "Trainer connected to TUI client");
                BroadcastStatus();
            }
            lastPipeConnectionState = pipeConnected;
        }

        // Check for hotkeys
        bool f1Pressed = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (f1Pressed && !prevF1State) {
            ToggleGodMode();
        }
        prevF1State = f1Pressed;

        bool f2Pressed = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (f2Pressed && !prevF2State) {
            ToggleInfiniteAmmo();
        }
        prevF2State = f2Pressed;

        bool f3Pressed = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
        if (f3Pressed && !prevF3State) {
            ToggleNoRecoil();
        }
        prevF3State = f3Pressed;

        bool f4Pressed = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (f4Pressed && !prevF4State) {
            AddHealth(1000);
        }
        prevF4State = f4Pressed;

        bool endPressed = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (endPressed && !prevEndState) {
            isRunning = false;
            if (pipeLogger && pipeLogger->IsConnected()) {
                pipeLogger->SendLog("info", "Trainer shutting down...");
            }
            break;
        }
        prevEndState = endPressed;

        // Update player data if features are active
        if (godMode || infiniteAmmo) {
            UpdatePlayerData();
        }

        // Send periodic status updates to TUI
        statusCounter++;
        if (statusCounter >= STATUS_UPDATE_INTERVAL) {
            BroadcastStatus();
            statusCounter = 0;
        }

        // Sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Trainer::ToggleGodMode() {
    godMode = !godMode;
    
    LogToFile(godMode ? "=== F1 PRESSED: God Mode ON ===" : "=== F1 PRESSED: God Mode OFF ===");
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "God Mode: " << (godMode ? "ON" : "OFF") << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout.flush();
    
    // Send to TUI
    if (pipeLogger && pipeLogger->IsConnected()) {
        pipeLogger->SendLog("info", godMode ? "God Mode enabled" : "God Mode disabled");
    }
    
    if (godMode && healthAddress) {
        // Read current values first
        int currentHealth = Memory::Read<int>(healthAddress);
        int currentArmor = Memory::Read<int>(armorAddress);
        
        char buffer[256];
        sprintf_s(buffer, "Before: Health=%d, Armor=%d", currentHealth, currentArmor);
        LogToFile(buffer);
        
        std::cout << "  Current Health: " << currentHealth << ", Armor: " << currentArmor << std::endl;
        std::cout.flush();
        
        // Set health and armor to 100 (typical max in Assault Cube)
        SetHealth(100);
        SetArmor(100);
        std::cout << "  Set Health and Armor to 100" << std::endl;
        std::cout.flush();
        
        // Verify the write worked
        int newHealth = Memory::Read<int>(healthAddress);
        int newArmor = Memory::Read<int>(armorAddress);
        
        sprintf_s(buffer, "After: Health=%d, Armor=%d", newHealth, newArmor);
        LogToFile(buffer);
        
        std::cout << "  New Health: " << newHealth << ", Armor: " << newArmor << std::endl;
        std::cout << "========================================\n" << std::endl;
        std::cout.flush();
    }

    BroadcastStatus();
}

void Trainer::ToggleInfiniteAmmo() {
    infiniteAmmo = !infiniteAmmo;
    std::cout << "\n========================================" << std::endl;
    std::cout << "Infinite Ammo: " << (infiniteAmmo ? "ON" : "OFF") << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout.flush();
    
    // Send to TUI
    if (pipeLogger && pipeLogger->IsConnected()) {
        pipeLogger->SendLog("info", infiniteAmmo ? "Infinite Ammo enabled" : "Infinite Ammo disabled");
    }
    
    if (infiniteAmmo && ammoAddress) {
        int currentAmmo = Memory::Read<int>(ammoAddress);
        std::cout << "  Current Ammo: " << currentAmmo << std::endl;
        std::cout.flush();
        SetAmmo(100);
        int newAmmo = Memory::Read<int>(ammoAddress);
        std::cout << "  Set Ammo to 100, New Ammo: " << newAmmo << std::endl;
        std::cout << "========================================\n" << std::endl;
        std::cout.flush();
    }

    BroadcastStatus();
}

void Trainer::ToggleNoRecoil() {
    noRecoil = !noRecoil;
    std::cout << "\n========================================" << std::endl;
    std::cout << "No Recoil: " << (noRecoil ? "ON" : "OFF") << std::endl;
    std::cout << "========================================\n" << std::endl;
    std::cout.flush();
    
    // Send to TUI
    if (pipeLogger && pipeLogger->IsConnected()) {
        pipeLogger->SendLog("info", noRecoil ? "No Recoil enabled" : "No Recoil disabled");
    }

    // This would require patching recoil-related instructions
    // Need to find the recoil code in the game

    BroadcastStatus();
}

void Trainer::AddHealth(int amount) {
    if (healthAddress) {
        int currentHealth = Memory::Read<int>(healthAddress);
        SetHealth(currentHealth + amount);
        std::cout << "\n========================================" << std::endl;
        std::cout << "Added " << amount << " health." << std::endl;
        std::cout << "Old health: " << currentHealth << " -> New health: " << (currentHealth + amount) << std::endl;
        std::cout << "========================================\n" << std::endl;
        std::cout.flush();
    }

    BroadcastStatus();
}

void Trainer::SetHealth(int value) {
    if (healthAddress) {
        Memory::Write<int>(healthAddress, value);
    }
}

void Trainer::SetArmor(int value) {
    if (armorAddress) {
        Memory::Write<int>(armorAddress, value);
    }
}

void Trainer::SetAmmo(int value) {
    if (playerBase) {
        // Set ammo for ALL weapon types
        Memory::Write<int>(playerBase + OFFSET_AR_AMMO, value);        // Assault Rifle
        Memory::Write<int>(playerBase + OFFSET_SMG_AMMO, value);       // Submachine Gun
        Memory::Write<int>(playerBase + OFFSET_SNIPER_AMMO, value);    // Sniper
        Memory::Write<int>(playerBase + OFFSET_SHOTGUN_AMMO, value);   // Shotgun
        Memory::Write<int>(playerBase + OFFSET_PISTOL_AMMO, value);    // Pistol
        Memory::Write<int>(playerBase + OFFSET_GRENADE_AMMO, value);   // Grenades
    }
}

bool Trainer::FindPlayerBase() {
    // We now read this directly in Initialize() from moduleBase + 0x0017E0A8
    // This function is kept for compatibility but isn't needed anymore
    playerBase = Memory::Read<uintptr_t>(moduleBase + 0x0017E0A8);
    return playerBase != 0;
}

bool Trainer::FindHealthAddress() {
    if (playerBase == 0) return false;
    
    // Health Value offset: 0xEC (from addresses.md)
    healthAddress = playerBase + 0xEC;
    
    return true;
}

bool Trainer::FindAmmoAddress() {
    if (playerBase == 0) return false;
    
    // Assault Rifle Ammo offset: 0x140 (from addresses.md)
    ammoAddress = playerBase + 0x140;
    
    return true;
}

void Trainer::UpdatePlayerData() {
    // Called in main loop to maintain active features
    if (godMode && healthAddress) {
        int currentHealth = Memory::Read<int>(healthAddress);
        if (currentHealth < 100) {
            SetHealth(100);
        }
        int currentArmor = Memory::Read<int>(armorAddress);
        if (currentArmor < 100) {
            SetArmor(100);
        }
    }
    
    if (infiniteAmmo && ammoAddress) {
        // Keep assault rifle ammo at max (100 in Assault Cube)
        SetAmmo(100);
    }
}

void Trainer::DisplayStatus() {
    ClearConsole();
    std::cout << "=== Assault Cube Trainer ===" << std::endl;
    std::cout << "Module Base: 0x" << std::hex << moduleBase << std::dec << std::endl;
    std::cout << "\nStatus:" << std::endl;
    std::cout << "God Mode: " << (godMode ? "ON" : "OFF") << std::endl;
    std::cout << "Infinite Ammo: " << (infiniteAmmo ? "ON" : "OFF") << std::endl;
    std::cout << "No Recoil: " << (noRecoil ? "ON" : "OFF") << std::endl;

    if (healthAddress) {
        int health = Memory::Read<int>(healthAddress);
        std::cout << "\nHealth: " << health << std::endl;
    }
}

void Trainer::BroadcastStatus() {
    if (!pipeLogger || !pipeLogger->IsConnected()) {
        return;
    }

    int health = healthAddress ? Memory::Read<int>(healthAddress) : 0;
    int armor = armorAddress ? Memory::Read<int>(armorAddress) : 0;
    int ammo = ammoAddress ? Memory::Read<int>(ammoAddress) : 0;

    pipeLogger->SendStatus(health, armor, ammo, godMode, infiniteAmmo, noRecoil);
}
