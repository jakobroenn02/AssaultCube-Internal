#include "pch.h"
#include "trainer.h"
#include "ui.h"
#include "memory.h"
#include "gl_hook.h"
#include <iostream>

Trainer::Trainer(uintptr_t base)
    : moduleBase(base),
      isRunning(true),
      uiRenderer(nullptr),
      gameWindowHandle(NULL),
      messagePumpInputEnabled(false),
      godMode(false),
      infiniteAmmo(false),
      noRecoil(false),
      regenHealth(false),
      esp(false),
      recoilPatchAddress(0),
      recoilPatched(false),
      playerBase(0),
      healthAddress(0),
      armorAddress(0),
      ammoAddress(0),
      recoilXAddress(0),
      recoilYAddress(0) {}

Trainer::~Trainer() {
    // Restore recoil bytes if patched
    if (recoilPatched) {
        RestoreRecoilBytes();
    }

    ShutdownOverlay();
}

bool Trainer::Initialize() {
    std::cout << "Initializing trainer..." << std::endl;

    // Get the game window handle
    gameWindowHandle = FindWindowA(NULL, "AssaultCube");
    if (!gameWindowHandle) {
        gameWindowHandle = FindWindowA("AssaultCube", NULL);
    }

    if (gameWindowHandle) {
        std::cout << "Game window found: " << gameWindowHandle << std::endl;
        
        uiRenderer = new UIRenderer();
        if (!uiRenderer->Initialize(gameWindowHandle)) {
            std::cout << "WARNING: Failed to initialize UI renderer" << std::endl;
            delete uiRenderer;
            uiRenderer = nullptr;
        } else {
            std::cout << "UI Renderer initialized successfully!" << std::endl;
        }

        std::cout << "Attempting to install OpenGL hooks..." << std::endl;
        if (!InstallHooks(gameWindowHandle, this, uiRenderer)) {
            std::cout << "ERROR: Failed to install OpenGL hooks!" << std::endl;
            std::cout << "The overlay will not work without OpenGL hooks." << std::endl;
        } else {
            std::cout << "SUCCESS: OpenGL hooks installed!" << std::endl;
            std::cout << "Overlay should now render when INSERT is pressed." << std::endl;
        }
    } else {
        std::cout << "ERROR: Could not find game window!" << std::endl;
        std::cout << "Make sure AssaultCube is running." << std::endl;
    }
    
    RefreshPlayerAddresses();

    if (playerBase == 0) {
        std::cout << "ERROR: Failed to read player base from 0x" << std::hex
                  << (moduleBase + OFFSET_LOCALPLAYER) << std::dec << std::endl;
        return false;
    }

    std::cout << "Player base found at: 0x" << std::hex << playerBase << std::dec << std::endl;
    
    std::cout << "Health address: 0x" << std::hex << healthAddress << std::dec << std::endl;
    std::cout << "Armor address: 0x" << std::hex << armorAddress << std::dec << std::endl;
    std::cout << "Ammo address: 0x" << std::hex << ammoAddress << std::dec << std::endl;
    std::cout << "Recoil X address: 0x" << std::hex << recoilXAddress << std::dec << std::endl;
    std::cout << "Recoil Y address: 0x" << std::hex << recoilYAddress << std::dec << std::endl;
    
    // Find recoil patch address
    if (FindRecoilPatchAddress()) {
        std::cout << "Recoil patch address found at: 0x" << std::hex << recoilPatchAddress << std::dec << std::endl;
    } else {
        std::cout << "WARNING: Could not find recoil patch address" << std::endl;
    }
    
    // Read and display current values to verify addresses are correct
    int currentHealth = Memory::Read<int>(healthAddress);
    int currentArmor = Memory::Read<int>(armorAddress);
    int currentAmmo = Memory::Read<int>(ammoAddress);
    
    
    std::cout << "\nCurrent values:" << std::endl;
    std::cout << "  Health: " << currentHealth << std::endl;
    std::cout << "  Armor: " << currentArmor << std::endl;
    std::cout << "  Ammo: " << currentAmmo << std::endl;
    
    std::cout << "\nTrainer initialized successfully!" << std::endl;
    return true;
}

void Trainer::ProcessOverlayInput() {
    if (!uiRenderer) {
        return;
    }

    // Check INSERT key to toggle menu
    static bool insertHeld = false;
    bool insertPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    
    if (insertPressed && !insertHeld) {
        uiRenderer->ToggleMenu();
    }
    insertHeld = insertPressed;
}

void Trainer::Run() {
    std::cout << "\nTrainer is running with overlay support...\n" << std::endl;
    std::cout << "Press INSERT to show/hide menu" << std::endl;
    std::cout << "Use UP/DOWN arrows to navigate, ENTER to toggle" << std::endl;

    while (isRunning) {
        // Process overlay input (keyboard navigation) if message hook is unavailable
        if (!IsMessagePumpInputEnabled()) {
            ProcessOverlayInput();
        }
        
        // Update player data if features are active
        if (godMode || infiniteAmmo || noRecoil || regenHealth) {
            UpdatePlayerData();
        }

        // Sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\nTrainer shutting down..." << std::endl;

    ShutdownOverlay();
}

void Trainer::SetMessagePumpInputEnabled(bool enabled) {
    messagePumpInputEnabled.store(enabled);
}

bool Trainer::ProcessMessage(MSG& msg, bool inputCaptureEnabled) {
    if (!uiRenderer) {
        return false;
    }

    bool requestUnload = false;
    if (!inputCaptureEnabled) {
        return false;
    }

    bool handled = uiRenderer->ProcessInput(msg, requestUnload);

    if (requestUnload) {
        RequestUnload();
    }

    return handled;
}

void Trainer::SetOverlayMenuVisible(bool visible) {
    if (!uiRenderer) {
        return;
    }

    uiRenderer->SetMenuVisible(visible);
}

void Trainer::ToggleGodMode() {
    RefreshPlayerAddresses();

    godMode = !godMode;
    
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "God Mode: " << (godMode ? "ON" : "OFF") << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout.flush();
    
    if (godMode && healthAddress) {
        // Read current values first
        int currentHealth = Memory::Read<int>(healthAddress);
        int currentArmor = Memory::Read<int>(armorAddress);
        
        
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
        
        
        std::cout << "  New Health: " << newHealth << ", Armor: " << newArmor << std::endl;
        std::cout << "========================================\n" << std::endl;
        std::cout.flush();
    }
}

void Trainer::ToggleInfiniteAmmo() {
    RefreshPlayerAddresses();

    infiniteAmmo = !infiniteAmmo;
    std::cout << "\n========================================" << std::endl;
    std::cout << "Infinite Ammo: " << (infiniteAmmo ? "ON" : "OFF") << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout.flush();
    
    if (infiniteAmmo && playerBase) {
        std::cout << "  Setting all weapon ammo to maximum..." << std::endl;
        std::cout.flush();
        
        // Set all weapon types to max ammo
        Memory::Write<int>(playerBase + 0x12C, 100); // Pistol
        Memory::Write<int>(playerBase + 0x134, 100); // Shotgun
        Memory::Write<int>(playerBase + 0x138, 100); // Submachine Gun
        Memory::Write<int>(playerBase + 0x13C, 100); // Sniper
        Memory::Write<int>(playerBase + 0x140, 100); // Assault Rifle
        Memory::Write<int>(playerBase + 0x144, 100); // Grenades
        
        std::cout << "  All weapon ammo set to 100" << std::endl;
        std::cout << "========================================\n" << std::endl;
        std::cout.flush();
    }
}

void Trainer::ToggleNoRecoil() {
    RefreshPlayerAddresses();

    noRecoil = !noRecoil;
    std::cout << "\n========================================" << std::endl;
    std::cout << "No Recoil: " << (noRecoil ? "ON" : "OFF") << std::endl;

    if (noRecoil && playerBase) {
        std::cout << "  Using multi-method approach:" << std::endl;
        std::cout << "    1. Zeroing accumulated recoil (0x32C, 0x330)" << std::endl;
        std::cout << "    2. Zeroing weapon recoil property (0x40)" << std::endl;
        
        // Immediately apply all methods
        Memory::Write<float>(playerBase + OFFSET_RECOIL_X, 0.0f);
        Memory::Write<float>(playerBase + OFFSET_RECOIL_Y, 0.0f);
        Memory::Write<float>(playerBase + OFFSET_WEAPON_RECOIL_PROPERTY, 0.0f);
        
        std::cout << "  Recoil disabled!" << std::endl;
    } else if (!noRecoil && playerBase) {
        std::cout << "  Restoring normal recoil behavior" << std::endl;
    }

    std::cout << "========================================\n" << std::endl;
    std::cout.flush();
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
    if (ammoAddress) {
        Memory::Write<int>(ammoAddress, value);
    }
}

void Trainer::ToggleESP() {
    esp = !esp;
    std::cout << "\n========================================" << std::endl;
    std::cout << "ESP / Wallhack: " << (esp ? "ON" : "OFF") << std::endl;
    
    if (esp) {
        std::cout << "  Drawing player boxes and info overlay" << std::endl;
        std::cout << "  Green = Teammates | Red = Enemies" << std::endl;
        
        // Test player list reading
        std::vector<uintptr_t> testPlayers;
        if (GetPlayerList(testPlayers)) {
            std::cout << "  Found " << testPlayers.size() << " players in game" << std::endl;
        } else {
            std::cout << "  WARNING: No players found! Check if you're in a multiplayer game." << std::endl;
        }
    }
    
    std::cout << "========================================\n" << std::endl;
    std::cout.flush();
}

// ESP Helper Functions
int Trainer::GetPlayerCount() {
    uintptr_t playerCountAddr = moduleBase + OFFSET_PLAYER_COUNT;
    int count = Memory::Read<int>(playerCountAddr);
    
    // Debug output on first call
    static bool firstCall = true;
    if (firstCall && esp) {
        std::cout << "[ESP Debug] Player count address: 0x" << std::hex << playerCountAddr << std::dec << std::endl;
        std::cout << "[ESP Debug] Player count: " << count << std::endl;
        firstCall = false;
    }
    
    return count;
}

bool Trainer::GetPlayerList(std::vector<uintptr_t>& players) {
    players.clear();
    
    int count = GetPlayerCount();
    if (count <= 0 || count > 32) {
        if (esp) {
            std::cout << "[ESP Debug] Invalid player count: " << count << std::endl;
        }
        return false;  // Sanity check
    }
    
    uintptr_t entityListAddr = moduleBase + OFFSET_ENTITY_LIST;
    uintptr_t entityList = Memory::Read<uintptr_t>(entityListAddr);
    
    // Debug output
    static bool firstListCall = true;
    if (firstListCall && esp) {
        std::cout << "[ESP Debug] Entity list address: 0x" << std::hex << entityListAddr << std::dec << std::endl;
        std::cout << "[ESP Debug] Entity list pointer: 0x" << std::hex << entityList << std::dec << std::endl;
        firstListCall = false;
    }
    
    if (!entityList) {
        if (esp) {
            std::cout << "[ESP Debug] Entity list pointer is null!" << std::endl;
        }
        return false;
    }
    
    for (int i = 0; i < count; i++) {
        uintptr_t playerPtr = Memory::Read<uintptr_t>(entityList + (i * 4));
        if (playerPtr && playerPtr != playerBase) {  // Skip local player
            players.push_back(playerPtr);
        }
    }
    
    return !players.empty();
}

bool Trainer::IsPlayerValid(uintptr_t playerPtr) {
    if (!playerPtr || playerPtr == playerBase) return false;
    
    // Basic validity check - read a value and see if it's reasonable
    float x = Memory::Read<float>(playerPtr + OFFSET_POS_X_ACTUAL);
    return (x > -10000.0f && x < 10000.0f);  // Reasonable world coordinate
}

bool Trainer::IsPlayerAlive(uintptr_t playerPtr) {
    if (!playerPtr) return false;
    
    // Check if dead flag (offset 0x76) is 0 (alive)
    byte isDead = Memory::Read<byte>(playerPtr + OFFSET_PLAYER_STATE1);
    return (isDead == 0);
}

int Trainer::GetPlayerTeam(uintptr_t playerPtr) {
    if (!playerPtr) return -1;
    return Memory::Read<int>(playerPtr + OFFSET_TEAM_ID);
}

void Trainer::GetPlayerPosition(uintptr_t playerPtr, float& x, float& y, float& z) {
    if (!playerPtr) {
        x = y = z = 0.0f;
        return;
    }
    
    x = Memory::Read<float>(playerPtr + OFFSET_POS_X_ACTUAL);
    y = Memory::Read<float>(playerPtr + OFFSET_POS_Y_ACTUAL);
    z = Memory::Read<float>(playerPtr + OFFSET_POS_Z_ACTUAL);
}

void Trainer::GetPlayerName(uintptr_t playerPtr, char* name, size_t maxLen) {
    if (!playerPtr || !name || maxLen == 0) return;
    
    // Read player name from offset 0x205 (260 byte string)
    for (size_t i = 0; i < maxLen - 1 && i < 260; i++) {
        name[i] = Memory::Read<char>(playerPtr + OFFSET_PLAYER_NAME + i);
        if (name[i] == 0) break;
    }
    name[maxLen - 1] = 0;  // Ensure null termination
}

void Trainer::GetLocalPlayerPosition(float& x, float& y, float& z) {
    if (!playerBase) {
        x = y = z = 0.0f;
        return;
    }
    
    x = Memory::Read<float>(playerBase + OFFSET_POS_X_ACTUAL);
    y = Memory::Read<float>(playerBase + OFFSET_POS_Y_ACTUAL);
    z = Memory::Read<float>(playerBase + OFFSET_POS_Z_ACTUAL);
}

void Trainer::GetLocalPlayerAngles(float& yaw, float& pitch) {
    if (!playerBase) {
        yaw = pitch = 0.0f;
        return;
    }
    
    yaw = Memory::Read<float>(playerBase + OFFSET_YAW);
    pitch = Memory::Read<float>(playerBase + OFFSET_PITCH);
}

bool Trainer::FindPlayerBase() {
    // We now read this directly in Initialize() from moduleBase + OFFSET_LOCALPLAYER
    // This function is kept for compatibility but isn't needed anymore
    playerBase = Memory::Read<uintptr_t>(moduleBase + OFFSET_LOCALPLAYER);
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
    RefreshPlayerAddresses();

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
    
    if (infiniteAmmo && playerBase) {
        // Keep all weapon ammo at max (100 rounds per weapon in Assault Cube)
        Memory::Write<int>(playerBase + 0x12C, 100); // Pistol Ammo
        Memory::Write<int>(playerBase + 0x134, 100); // Shotgun Ammo
        Memory::Write<int>(playerBase + 0x138, 100); // Submachine Gun Ammo
        Memory::Write<int>(playerBase + 0x13C, 100); // Sniper Ammo
        Memory::Write<int>(playerBase + 0x140, 100); // Assault Rifle Ammo
        Memory::Write<int>(playerBase + 0x144, 100); // Grenade Ammo
    }
    
    if (noRecoil && playerBase) {
        // Method 1: Zero accumulated recoil (prevents recoil buildup)
        Memory::Write<float>(playerBase + OFFSET_RECOIL_X, 0.0f);
        Memory::Write<float>(playerBase + OFFSET_RECOIL_Y, 0.0f);
        
        // Method 2: Zero weapon recoil property (reduces recoil calculation)
        Memory::Write<float>(playerBase + OFFSET_WEAPON_RECOIL_PROPERTY, 0.0f);
    }
    
    if (regenHealth && healthAddress) {
        // Regenerative health: maintain minimum 75 HP/Armor
        int currentHealth = Memory::Read<int>(healthAddress);
        if (currentHealth < 75) {
            SetHealth(75);
        }
        int currentArmor = Memory::Read<int>(armorAddress);
        if (currentArmor < 75) {
            SetArmor(75);
        }
    }
}

// Build feature toggles for UI
std::vector<FeatureToggle> Trainer::BuildFeatureToggles() {
    std::vector<FeatureToggle> toggles;
    
    // God Mode toggle
    FeatureToggle godModeToggle;
    godModeToggle.name = "God Mode";
    godModeToggle.description = "Invincibility - maintain 100 HP/Armor";
    godModeToggle.onToggle = [this]() { this->ToggleGodMode(); };
    godModeToggle.isActive = [this]() { return this->godMode.load(); };
    toggles.push_back(godModeToggle);
    
    // Infinite Ammo toggle
    FeatureToggle infAmmoToggle;
    infAmmoToggle.name = "Infinite Ammo";
    infAmmoToggle.description = "Never run out of bullets";
    infAmmoToggle.onToggle = [this]() { this->ToggleInfiniteAmmo(); };
    infAmmoToggle.isActive = [this]() { return this->infiniteAmmo.load(); };
    toggles.push_back(infAmmoToggle);
    
    // No Recoil toggle
    FeatureToggle noRecoilToggle;
    noRecoilToggle.name = "No Recoil";
    noRecoilToggle.description = "Remove weapon recoil";
    noRecoilToggle.onToggle = [this]() { this->ToggleNoRecoil(); };
    noRecoilToggle.isActive = [this]() { return this->noRecoil.load(); };
    toggles.push_back(noRecoilToggle);
    
    // Regen Health toggle
    FeatureToggle regenToggle;
    regenToggle.name = "Regen Health";
    regenToggle.description = "Auto-restore to 75 HP/Armor";
    regenToggle.onToggle = [this]() { this->ToggleRegenHealth(); };
    regenToggle.isActive = [this]() { return this->regenHealth.load(); };
    toggles.push_back(regenToggle);
    
    // ESP / Wallhack toggle
    FeatureToggle espToggle;
    espToggle.name = "ESP / Wallhack";
    espToggle.description = "See players through walls";
    espToggle.onToggle = [this]() { this->ToggleESP(); };
    espToggle.isActive = [this]() { return this->esp.load(); };
    toggles.push_back(espToggle);
    
    return toggles;
}

// Get current player stats
PlayerStats Trainer::GetPlayerStats() {
    PlayerStats stats;
    stats.health = healthAddress ? Memory::Read<int>(healthAddress) : 0;
    stats.armor = armorAddress ? Memory::Read<int>(armorAddress) : 0;
    stats.ammo = ammoAddress ? Memory::Read<int>(ammoAddress) : 0;
    return stats;
}

// Toggle functions
void Trainer::ToggleRegenHealth() {
    regenHealth = !regenHealth;
    std::cout << "Regen Health: " << (regenHealth ? "ON" : "OFF") << std::endl;
}

void Trainer::InstantRefillHealth() {
    if (healthAddress && armorAddress) {
        SetHealth(100);
        SetArmor(100);
        std::cout << "Health refilled to 100!" << std::endl;
    }
}

void Trainer::RequestUnload() {
    std::cout << "Unload requested - shutting down trainer..." << std::endl;
    isRunning = false;
}

// Find recoil patch address using pattern scanning
bool Trainer::FindRecoilPatchAddress() {
    uintptr_t baseAddress = 0;
    size_t moduleSize = 0;

    if (!Memory::GetModuleInfo("ac_client.exe", baseAddress, moduleSize)) {
        std::cout << "WARNING: Failed to query module info for recoil scan" << std::endl;
        recoilPatchAddress = 0;
        return false;
    }

    const char pattern[] = "\xF3\x0F\x11\x83\xCC\x00\x00\x00\xF3\x0F\x11\x8B\xC8\x00\x00\x00";
    const char mask[] = "xxxxxxxxxxxxxxxx";

    recoilPatchAddress = Memory::FindPattern(baseAddress, moduleSize, pattern, mask);

    if (recoilPatchAddress == 0) {
        std::cout << "WARNING: Recoil pattern scan failed" << std::endl;
        return false;
    }

    return true;
}

// Apply recoil patch (NOP the recoil instruction)
void Trainer::ApplyRecoilPatch() {
    if (recoilPatchAddress == 0 || recoilPatched) return;

    // Save original bytes (assuming 5-byte instruction)
    constexpr size_t patchSize = 16;
    originalRecoilBytes.resize(patchSize);
    for (size_t i = 0; i < patchSize; i++) {
        originalRecoilBytes[i] = Memory::Read<BYTE>(recoilPatchAddress + i);
    }

    // Apply NOP patch
    for (size_t i = 0; i < patchSize; i++) {
        Memory::Write<BYTE>(recoilPatchAddress + i, 0x90);
    }
    
    recoilPatched = true;
    std::cout << "Recoil patch applied at 0x" << std::hex << recoilPatchAddress << std::dec << std::endl;
}

// Restore original recoil bytes
void Trainer::RestoreRecoilBytes() {
    if (recoilPatchAddress == 0 || !recoilPatched) return;
    
    for (size_t i = 0; i < originalRecoilBytes.size(); i++) {
        Memory::Write<BYTE>(recoilPatchAddress + i, originalRecoilBytes[i]);
    }
    
    recoilPatched = false;
    std::cout << "Recoil patch restored" << std::endl;
}

void Trainer::DisplayStatus() {
    // Deprecated - overlay handles display now
    std::cout << "=== Assault Cube Trainer ===" << std::endl;
    std::cout << "God Mode: " << (godMode ? "ON" : "OFF") << std::endl;
    std::cout << "Infinite Ammo: " << (infiniteAmmo ? "ON" : "OFF") << std::endl;
    std::cout << "No Recoil: " << (noRecoil ? "ON" : "OFF") << std::endl;
    std::cout << "Regen Health: " << (regenHealth ? "ON" : "OFF") << std::endl;
}

void Trainer::RefreshPlayerAddresses() {
    uintptr_t newPlayerBase = Memory::Read<uintptr_t>(moduleBase + OFFSET_LOCALPLAYER);

    if (newPlayerBase != playerBase) {
        playerBase = newPlayerBase;

        if (playerBase) {
            healthAddress = playerBase + OFFSET_HEALTH;
            armorAddress = playerBase + OFFSET_ARMOR;
            ammoAddress = playerBase + OFFSET_AR_AMMO;
            recoilXAddress = playerBase + OFFSET_RECOIL_X;
            recoilYAddress = playerBase + OFFSET_RECOIL_Y;
        } else {
            healthAddress = 0;
            armorAddress = 0;
            ammoAddress = 0;
            recoilXAddress = 0;
            recoilYAddress = 0;
        }
    }
}

void Trainer::ShutdownOverlay() {
    RemoveHooks();

    if (uiRenderer) {
        uiRenderer->Shutdown();
        delete uiRenderer;
        uiRenderer = nullptr;
    }
}
