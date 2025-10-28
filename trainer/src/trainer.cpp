#include "pch.h"
#include "trainer.h"
#include "ui.h"
#include "memory.h"
#include "gl_hook.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace TrainerInternal {
float ReadClampedEyeHeight(uintptr_t playerPtr) {
    if (!playerPtr) {
        return 12.5f;
    }

    float eyeHeight = Memory::Read<float>(playerPtr + Trainer::OFFSET_EYE_HEIGHT);
    if (!std::isfinite(static_cast<double>(eyeHeight)) || eyeHeight < 1.0f || eyeHeight > 150.0f) {
        eyeHeight = 12.5f;
    }
    return std::clamp(eyeHeight, 4.0f, 32.0f);
}
}  // namespace TrainerInternal

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
      aimbot(false),
      aimbotSmoothness(3.0f),
      aimbotFOV(45.0f),
      aimbotUseFOV(false),
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

        // Update aimbot if active
        if (aimbot) {
            UpdateAimbot();
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

void Trainer::ToggleAimbot() {
    aimbot = !aimbot;
    std::cout << "\n========================================" << std::endl;
    std::cout << "Aimbot: " << (aimbot ? "ON" : "OFF") << std::endl;

    if (aimbot) {
        std::cout << "  Mode: " << (aimbotUseFOV ? "Closest to Crosshair" : "Closest Distance") << std::endl;
        std::cout << "  Smoothness: " << aimbotSmoothness.load() << "x" << std::endl;
        if (aimbotUseFOV) {
            std::cout << "  FOV: " << aimbotFOV.load() << " degrees" << std::endl;
        }
        std::cout << "  Tip: Open menu to adjust settings" << std::endl;
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
    float baseZ = Memory::Read<float>(playerPtr + OFFSET_POS_Z_ACTUAL);
    float eyeHeight = TrainerInternal::ReadClampedEyeHeight(playerPtr);
    z = baseZ - eyeHeight;
}

void Trainer::GetPlayerHeadPosition(uintptr_t playerPtr, float& x, float& y, float& z) {
    if (!playerPtr) {
        x = y = z = 0.0f;
        return;
    }

    // Read feet position once so we can validate head data or fall back.
    const float feetX = Memory::Read<float>(playerPtr + OFFSET_POS_X_ACTUAL);
    const float feetY = Memory::Read<float>(playerPtr + OFFSET_POS_Y_ACTUAL);
    const float baseZ = Memory::Read<float>(playerPtr + OFFSET_POS_Z_ACTUAL);

    // Method 1: Use pre-calculated head position (updated by the game each frame).
    const float storedHeadX = Memory::Read<float>(playerPtr + OFFSET_HEAD_X);   // 0x3F8
    const float storedHeadY = Memory::Read<float>(playerPtr + OFFSET_HEAD_Y);   // 0x3FC
    const float storedHeadZ = Memory::Read<float>(playerPtr + OFFSET_HEAD_Z);   // 0x400

    auto isFinite = [](float value) {
        return std::isfinite(static_cast<double>(value));
    };

    float eyeHeight = TrainerInternal::ReadClampedEyeHeight(playerPtr);
    const float feetZ = baseZ - eyeHeight;

    bool headValid = isFinite(storedHeadX) && isFinite(storedHeadY) && isFinite(storedHeadZ);
    if (headValid) {
        const float dx = std::fabs(storedHeadX - feetX);
        const float dy = std::fabs(storedHeadY - feetY);
        const float dz = storedHeadZ - feetZ;

        // In AssaultCube the head is always near the feet in X/Y, and higher on Z.
        // Some builds expose sentinel values (-1 or 0) when the render cache misses;
        // treat those as invalid so we can fall back gracefully.
        if (dx > 50.0f || dy > 50.0f || dz < 0.1f || dz > 150.0f ||
            (std::fabs(storedHeadX) < 1e-3f && std::fabs(storedHeadY) < 1e-3f && std::fabs(storedHeadZ) < 1e-3f) ||
            (std::fabs(storedHeadX + 1.0f) < 0.25f && std::fabs(storedHeadY + 1.0f) < 0.25f && std::fabs(storedHeadZ + 1.0f) < 0.25f)) {
            headValid = false;
        }
    }

    if (headValid) {
        x = storedHeadX;
        y = storedHeadY;
        z = storedHeadZ;
        return;
    }

    // Method 2 (Fallback): Calculate head position from feet + height when the cached
    // head values look suspicious (e.g. -1,-1,-1). This keeps ESP overlays aligned.
    x = feetX;
    y = feetY;
    z = feetZ + eyeHeight;
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
    float baseZ = Memory::Read<float>(playerBase + OFFSET_POS_Z_ACTUAL);
    float eyeHeight = TrainerInternal::ReadClampedEyeHeight(playerBase);
    z = baseZ - eyeHeight;
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

    // Aimbot toggle
    FeatureToggle aimbotToggle;
    aimbotToggle.name = "Aimbot";
    aimbotToggle.description = "Auto-aim at nearest enemy";
    aimbotToggle.onToggle = [this]() { this->ToggleAimbot(); };
    aimbotToggle.isActive = [this]() { return this->aimbot.load(); };
    toggles.push_back(aimbotToggle);

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

// ========== MATRIX IMPLEMENTATION ==========
// Get the view (modelview) matrix from game memory
void Trainer::GetViewMatrix(float* outMatrix) {
    uintptr_t matrixAddr = moduleBase + OFFSET_VIEW_MATRIX;
    for (int i = 0; i < 16; ++i) {
        outMatrix[i] = Memory::Read<float>(matrixAddr + i * sizeof(float));
    }

    // Fallback: Try hook if static address returns all zeros
    bool hasValidMatrix = false;
    for (int i = 0; i < 16; ++i) {
        if (outMatrix[i] != 0.0f) {
            hasValidMatrix = true;
            break;
        }
    }

    if (!hasValidMatrix && !GetCapturedViewMatrix(outMatrix)) {
        // Last resort: identity matrix
        memset(outMatrix, 0, 16 * sizeof(float));
        outMatrix[0] = outMatrix[5] = outMatrix[10] = outMatrix[15] = 1.0f;
    }
}

// Get the combined view-projection matrix from game memory
void Trainer::GetProjectionMatrix(float* outMatrix) {
    uintptr_t matrixAddr = moduleBase + OFFSET_VIEWPROJECTION_MATRIX;
    for (int i = 0; i < 16; ++i) {
        outMatrix[i] = Memory::Read<float>(matrixAddr + i * sizeof(float));
    }

    // Check if valid
    bool hasValidMatrix = false;
    for (int i = 0; i < 16; ++i) {
        if (outMatrix[i] != 0.0f) {
            hasValidMatrix = true;
            break;
        }
    }

    if (!hasValidMatrix) {
        // Return identity if invalid
        memset(outMatrix, 0, 16 * sizeof(float));
        outMatrix[0] = outMatrix[5] = outMatrix[10] = outMatrix[15] = 1.0f;
    }
}

// Multiply two 4x4 matrices (column-major storage): result = a * b
static void MultiplyMatrix(const float* a, const float* b, float* result) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

// Get the combined view-projection matrix
// In AssaultCube, the view matrix at 0x0057E010 is already the combined modelview-projection matrix
void Trainer::GetViewProjectionMatrix(float* outMatrix) {
    // The "view matrix" in AC is actually already combined with projection
    GetViewMatrix(outMatrix);
}

// ========== AIMBOT IMPLEMENTATION ==========
// Educational: This demonstrates how aimbots work in 3D games

// Check if target is likely visible (not through walls)
// This is a simple heuristic based on height difference and angle
bool Trainer::IsTargetLikelyVisible(float localX, float localY, float localZ,
                                      float targetX, float targetY, float targetZ) {
    // Calculate distance components
    float dx = targetX - localX;
    float dy = targetY - localY;
    float dz = targetZ - localZ;

    float horizontalDist = sqrt(dx * dx + dy * dy);
    float totalDist = sqrt(dx * dx + dy * dy + dz * dz);

    // Skip targets that are too close (might cause weird angles)
    if (totalDist < 5.0f) {
        return false;
    }

    // Calculate vertical angle to target
    float verticalAngle = atan2(fabs(dz), horizontalDist) * (180.0f / 3.14159265f);

    // If the target is at a steep vertical angle, they're likely on a different floor
    // Typical player height in AssaultCube is around 15 units
    // If height difference is more than 20 units and angle is steep, likely through floor/ceiling
    const float maxHeightDiff = 20.0f;  // Max height difference for same floor
    const float maxVerticalAngle = 35.0f;  // Max vertical angle (degrees)

    if (fabs(dz) > maxHeightDiff && verticalAngle > maxVerticalAngle) {
        return false;  // Target is on a different floor with steep angle
    }

    // Additional check: if horizontal distance is small but vertical is large,
    // likely directly above/below through floor
    if (horizontalDist < 30.0f && fabs(dz) > 25.0f) {
        return false;  // Target is directly above/below through floor
    }

    return true;
}

// Find the closest enemy player to aim at
uintptr_t Trainer::FindClosestEnemy(float& outDistance) {
    if (!playerBase) {
        outDistance = -1.0f;
        return 0;
    }

    // Get local player position
    float localX, localY, localZ;
    GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Adjust to eye level for better visibility checks

    // Get local player team
    int localTeam = Memory::Read<int>(playerBase + OFFSET_TEAM_ID);

    // Get all players
    std::vector<uintptr_t> players;
    if (!GetPlayerList(players)) {
        outDistance = -1.0f;
        return 0;
    }

    uintptr_t closestEnemy = 0;
    float closestDistance = 99999.0f;

    for (uintptr_t playerPtr : players) {
        // Skip invalid or dead players
        if (!IsPlayerValid(playerPtr) || !IsPlayerAlive(playerPtr)) {
            continue;
        }

        // Skip teammates (only aim at enemies)
        int playerTeam = GetPlayerTeam(playerPtr);
        if (playerTeam == localTeam) {
            continue;
        }

        // Get enemy position
        float enemyX, enemyY, enemyZ;
        GetPlayerPosition(playerPtr, enemyX, enemyY, enemyZ);
        enemyZ += 10.0f;  // Adjust to target's head level

        // Calculate 3D distance
        float dx = enemyX - localX;
        float dy = enemyY - localY;
        float dz = enemyZ - localZ;
        float distance = sqrt(dx * dx + dy * dy + dz * dz);

        // Check if target is likely visible (not through walls/floors)
        if (!IsTargetLikelyVisible(localX, localY, localZ, enemyX, enemyY, enemyZ)) {
            continue;  // Skip targets that are likely behind walls
        }

        // Track closest enemy
        if (distance < closestDistance) {
            closestDistance = distance;
            closestEnemy = playerPtr;
        }
    }

    outDistance = closestDistance;
    return closestEnemy;
}

// Calculate yaw and pitch angles to aim from position 'from' to position 'to'
void Trainer::CalculateAngles(const float* from, const float* to, float& yaw, float& pitch) {
    // Calculate delta vector
    float dx = to[0] - from[0];
    float dy = to[1] - from[1];
    float dz = to[2] - from[2];

    // Calculate yaw (horizontal angle)
    // Using AssaultCube's formula: -atan2(delta.x, delta.y) * 180/π + 180
    yaw = -atan2(dx, dy) * (180.0f / 3.14159265f);
    yaw += 180.0f;

    // Normalize to 0-360 range
    while (yaw >= 360.0f) yaw -= 360.0f;
    while (yaw < 0.0f) yaw += 360.0f;

    // Calculate pitch (vertical angle)
    // Using atan2(delta.z, hipotenuse) * 180/π
    float horizontalDist = sqrt(dx * dx + dy * dy);
    pitch = atan2(dz, horizontalDist) * (180.0f / 3.14159265f);
}

// Calculate FOV (field of view) angle to a target
// Returns the angle in degrees between crosshair and target
float Trainer::CalculateFOVToTarget(uintptr_t targetPtr) {
    if (!playerBase || !targetPtr) return 999.0f;

    // Get local player position and angles
    float localX, localY, localZ;
    GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Head height

    float currentYaw, currentPitch;
    GetLocalPlayerAngles(currentYaw, currentPitch);

    // Get target position
    float targetX, targetY, targetZ;
    GetPlayerPosition(targetPtr, targetX, targetY, targetZ);
    targetZ += 10.0f;  // Target head

    // Calculate angles to target
    float localPos[3] = { localX, localY, localZ };
    float targetPos[3] = { targetX, targetY, targetZ };
    float targetYaw, targetPitch;
    CalculateAngles(localPos, targetPos, targetYaw, targetPitch);

    // Calculate angular distance (FOV)
    float deltaYaw = targetYaw - currentYaw;
    float deltaPitch = targetPitch - currentPitch;

    // Normalize yaw
    while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
    while (deltaYaw < -180.0f) deltaYaw += 360.0f;

    // Calculate total FOV distance (Pythagorean theorem in angle space)
    float fov = sqrt(deltaYaw * deltaYaw + deltaPitch * deltaPitch);
    return fov;
}

// Find enemy closest to crosshair (within FOV cone)
uintptr_t Trainer::FindClosestEnemyToCrosshair(float& outFOV) {
    if (!playerBase) {
        outFOV = -1.0f;
        return 0;
    }

    // Get local player position for visibility checks
    float localX, localY, localZ;
    GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Adjust to eye level

    // Get local player team
    int localTeam = Memory::Read<int>(playerBase + OFFSET_TEAM_ID);

    // Get all players
    std::vector<uintptr_t> players;
    if (!GetPlayerList(players)) {
        outFOV = -1.0f;
        return 0;
    }

    uintptr_t closestEnemy = 0;
    float closestFOV = 999.0f;
    float maxFOV = aimbotFOV.load();  // Only consider enemies within this FOV

    for (uintptr_t playerPtr : players) {
        // Skip invalid or dead players
        if (!IsPlayerValid(playerPtr) || !IsPlayerAlive(playerPtr)) {
            continue;
        }

        // Skip teammates
        int playerTeam = GetPlayerTeam(playerPtr);
        if (playerTeam == localTeam) {
            continue;
        }

        // Get target position for visibility check
        float targetX, targetY, targetZ;
        GetPlayerPosition(playerPtr, targetX, targetY, targetZ);
        targetZ += 10.0f;  // Adjust to target's head level

        // Check if target is likely visible (not through walls/floors)
        if (!IsTargetLikelyVisible(localX, localY, localZ, targetX, targetY, targetZ)) {
            continue;  // Skip targets that are likely behind walls
        }

        // Calculate FOV to this target
        float fov = CalculateFOVToTarget(playerPtr);

        // Only consider targets within max FOV
        if (fov > maxFOV) {
            continue;
        }

        // Track closest to crosshair
        if (fov < closestFOV) {
            closestFOV = fov;
            closestEnemy = playerPtr;
        }
    }

    outFOV = closestFOV;
    return closestEnemy;
}

// Smoothly adjust aim toward target angles
void Trainer::SmoothAim(float targetYaw, float targetPitch) {
    if (!playerBase) return;

    // Get current angles
    float currentYaw = Memory::Read<float>(playerBase + OFFSET_YAW);
    float currentPitch = Memory::Read<float>(playerBase + OFFSET_PITCH);

    // Calculate angle difference
    float deltaYaw = targetYaw - currentYaw;
    float deltaPitch = targetPitch - currentPitch;

    // Normalize yaw to -180..180 range
    while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
    while (deltaYaw < -180.0f) deltaYaw += 360.0f;

    // Apply smoothing (higher smoothness = slower aim)
    // smoothness of 1.0 = instant snap, 5.0 = very smooth
    float smoothness = aimbotSmoothness.load();
    float newYaw = currentYaw + (deltaYaw / smoothness);
    float newPitch = currentPitch + (deltaPitch / smoothness);

    // Clamp pitch to valid range (-90 to 90 degrees)
    if (newPitch > 90.0f) newPitch = 90.0f;
    if (newPitch < -90.0f) newPitch = -90.0f;

    // Write new angles
    Memory::Write<float>(playerBase + OFFSET_YAW, newYaw);
    Memory::Write<float>(playerBase + OFFSET_PITCH, newPitch);
}

// Main aimbot update - called every frame when aimbot is active
void Trainer::UpdateAimbot() {
    RefreshPlayerAddresses();

    if (!playerBase) return;

    uintptr_t target = 0;
    bool useFOV = aimbotUseFOV.load();

    // DEBUG: Check target finding
    static int targetCheckCount = 0;
    bool debugPrint = (targetCheckCount++ % 200 == 0);

    if (useFOV) {
        // FOV-based targeting: aim at enemy closest to crosshair
        float fov = 0.0f;
        target = FindClosestEnemyToCrosshair(fov);

        if (debugPrint) {
            std::cout << "[AIMBOT] FOV mode - Found target: " << (target ? "YES" : "NO")
                      << " (FOV: " << fov << ")" << std::endl;
        }

        if (!target || fov < 0.0f) {
            return;  // No target found within FOV
        }
    } else {
        // Distance-based targeting: aim at closest enemy
        float distance = 0.0f;
        target = FindClosestEnemy(distance);

        if (debugPrint) {
            std::cout << "[AIMBOT] Distance mode - Found target: " << (target ? "YES" : "NO")
                      << " (Distance: " << distance << ")" << std::endl;
        }

        if (!target || distance < 0.0f) {
            return;  // No valid target found
        }

        // Don't aim at targets too far away
        const float maxAimbotDistance = 500.0f;
        if (distance > maxAimbotDistance) {
            if (debugPrint) {
                std::cout << "[AIMBOT] Target too far: " << distance << " > " << maxAimbotDistance << std::endl;
            }
            return;
        }
    }

    // Get local player head position (aim from head, not feet)
    float localX, localY, localZ;
    GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Add approximate head height offset

    // Get target head position
    float targetX, targetY, targetZ;
    GetPlayerPosition(target, targetX, targetY, targetZ);
    targetZ += 10.0f;  // Aim at enemy head

    // DEBUG: Print positions AND raw memory values every 100 frames
    static int frameCount = 0;
    if (frameCount++ % 100 == 0) {
        std::cout << "[AIMBOT DEBUG]" << std::endl;
        std::cout << "  Local pos: (" << localX << ", " << localY << ", " << localZ << ")" << std::endl;
        std::cout << "  Target pos: (" << targetX << ", " << targetY << ", " << targetZ << ")" << std::endl;

        // VERIFY: Read raw position and velocity from target to check offsets
        float rawX = Memory::Read<float>(target + 0x04);
        float rawY = Memory::Read<float>(target + 0x08);
        float rawZ = Memory::Read<float>(target + 0x0C);
        float velX = Memory::Read<float>(target + 0x10);
        float velY = Memory::Read<float>(target + 0x14);
        float velZ = Memory::Read<float>(target + 0x18);
        std::cout << "  RAW Target +0x04/08/0C: (" << rawX << ", " << rawY << ", " << rawZ << ")" << std::endl;
        std::cout << "  RAW Velocity +0x10/14/18: (" << velX << ", " << velY << ", " << velZ << ")" << std::endl;

        // Get current angles
        float currentYaw = Memory::Read<float>(playerBase + OFFSET_YAW);
        float currentPitch = Memory::Read<float>(playerBase + OFFSET_PITCH);
        std::cout << "  Current angles: Yaw=" << currentYaw << " Pitch=" << currentPitch << std::endl;
    }

    // Calculate required angles
    float localPos[3] = { localX, localY, localZ };
    float targetPos[3] = { targetX, targetY, targetZ };
    float targetYaw, targetPitch;
    CalculateAngles(localPos, targetPos, targetYaw, targetPitch);

    // DEBUG: Print calculated angles
    if (frameCount % 100 == 1) {
        std::cout << "  Target angles: Yaw=" << targetYaw << " Pitch=" << targetPitch << std::endl;
        float dx = targetX - localX;
        float dy = targetY - localY;
        float dz = targetZ - localZ;
        std::cout << "  Delta: dx=" << dx << " dy=" << dy << " dz=" << dz << std::endl;
    }

    // Smooth aim toward target
    SmoothAim(targetYaw, targetPitch);
}
