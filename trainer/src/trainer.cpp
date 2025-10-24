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

    if (noRecoil) {
        if (!recoilPatchAddress) {
            FindRecoilPatchAddress();
        }
        if (recoilXAddress && recoilYAddress) {
            std::cout << "  Recoil components will be zeroed every frame" << std::endl;
            std::cout << "  Recoil X: 0x" << std::hex << recoilXAddress << std::dec << std::endl;
            std::cout << "  Recoil Y: 0x" << std::hex << recoilYAddress << std::dec << std::endl;
            Memory::Write<int>(recoilXAddress, 0);
            Memory::Write<int>(recoilYAddress, 0);
        } else {
            std::cout << "  WARNING: Recoil addresses not found" << std::endl;
        }
        if (recoilPatchAddress) {
            ApplyRecoilPatch();
        } else {
            std::cout << "  WARNING: Recoil patch not available" << std::endl;
        }
    } else {
        if (recoilPatched) {
            RestoreRecoilBytes();
        }
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
        // Zero out recoil components every frame
        // Recoil offsets are relative to player entity
        if (recoilXAddress) {
            Memory::Write<int>(recoilXAddress, 0); // X recoil (horizontal)
        }
        if (recoilYAddress) {
            Memory::Write<int>(recoilYAddress, 0); // Y recoil (vertical)
        }
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
