#include "pch.h"
#include "trainer.h"
#include "memory.h"
#include <iostream>

Trainer::Trainer(uintptr_t base) 
    : moduleBase(base),
      isRunning(true),
      godMode(false),
      infiniteAmmo(false),
      noRecoil(false),
      playerBase(0),
      healthAddress(0),
      armorAddress(0),
      ammoAddress(0) {
}

Trainer::~Trainer() {
    // Restore original bytes if needed
    // Clean up any patches
}

bool Trainer::Initialize() {
    std::cout << "Initializing trainer..." << std::endl;
    
    // Read LocalPlayer pointer from ac_client.exe + 0x0017E0A8
    playerBase = Memory::Read<uintptr_t>(moduleBase + 0x0017E0A8);
    
    
    if (playerBase == 0) {
        std::cout << "ERROR: Failed to read player base from 0x" << std::hex << (moduleBase + 0x0017E0A8) << std::dec << std::endl;
        return false;
    }
    
    std::cout << "Player base found at: 0x" << std::hex << playerBase << std::dec << std::endl;
    
    // Calculate addresses using found offsets
    healthAddress = playerBase + 0xEC;  // Health Value
    armorAddress = playerBase + 0xF0;   // Armor Value
    ammoAddress = playerBase + 0x140;   // Assault Rifle Ammo
    
    
    std::cout << "Health address: 0x" << std::hex << healthAddress << std::dec << std::endl;
    std::cout << "Armor address: 0x" << std::hex << armorAddress << std::dec << std::endl;
    std::cout << "Ammo address: 0x" << std::hex << ammoAddress << std::dec << std::endl;
    
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

void Trainer::Run() {
    std::cout << "\nTrainer is running...\n" << std::endl;
    
    while (isRunning) {
        // Check for hotkeys
        if (GetAsyncKeyState(VK_F1) & 1) {
            ToggleGodMode();
        }
        if (GetAsyncKeyState(VK_F2) & 1) {
            ToggleInfiniteAmmo();
        }
        if (GetAsyncKeyState(VK_F3) & 1) {
            ToggleNoRecoil();
        }
        if (GetAsyncKeyState(VK_F4) & 1) {
            AddHealth(1000);
        }
        if (GetAsyncKeyState(VK_END) & 1) {
            isRunning = false;
            break;
        }
        
        // Update player data if features are active
        if (godMode || infiniteAmmo) {
            UpdatePlayerData();
        }
        
        // Sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Trainer::ToggleGodMode() {
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
    infiniteAmmo = !infiniteAmmo;
    std::cout << "\n========================================" << std::endl;
    std::cout << "Infinite Ammo: " << (infiniteAmmo ? "ON" : "OFF") << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout.flush();
    
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
}

void Trainer::ToggleNoRecoil() {
    noRecoil = !noRecoil;
    std::cout << "\n========================================" << std::endl;
    std::cout << "No Recoil: " << (noRecoil ? "ON" : "OFF") << std::endl;
    std::cout << "========================================\n" << std::endl;
    std::cout.flush();
    
    // This would require patching recoil-related instructions
    // Need to find the recoil code in the game
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
    system("cls");
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
