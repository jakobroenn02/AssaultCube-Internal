#ifndef TRAINER_H
#define TRAINER_H

#include <Windows.h>
#include <cstdint>
#include "pipelogger.h"

class Trainer {
private:
    uintptr_t moduleBase;
    bool isRunning;
    
    // IPC Communication
    PipeLogger* pipeLogger;
    
    // Feature toggles
    bool godMode;
    bool infiniteAmmo;
    bool noRecoil;
    
    // Player addresses (to be found)
    uintptr_t playerBase;
    uintptr_t healthAddress;
    uintptr_t armorAddress;
    uintptr_t ammoAddress;
    
    // Original bytes for patching/unpatching
    std::vector<BYTE> originalHealthBytes;
    std::vector<BYTE> originalAmmoBytes;
    
    // Static offsets from addresses.md
    static constexpr uintptr_t OFFSET_LOCALPLAYER = 0x0017E0A8;
    static constexpr uintptr_t OFFSET_ENTITY_LIST = 0x18AC04;
    static constexpr uintptr_t OFFSET_FOV = 0x18A7CC;
    static constexpr uintptr_t OFFSET_PLAYER_COUNT = 0x18AC0C;
    
    // Player offsets
    static constexpr uintptr_t OFFSET_HEALTH = 0xEC;
    static constexpr uintptr_t OFFSET_ARMOR = 0xF0;
    
    // Ammo offsets
    static constexpr uintptr_t OFFSET_AR_AMMO = 0x140;
    static constexpr uintptr_t OFFSET_SMG_AMMO = 0x138;
    static constexpr uintptr_t OFFSET_SNIPER_AMMO = 0x13C;
    static constexpr uintptr_t OFFSET_SHOTGUN_AMMO = 0x134;
    static constexpr uintptr_t OFFSET_PISTOL_AMMO = 0x12C;
    static constexpr uintptr_t OFFSET_GRENADE_AMMO = 0x144;
    
    // Position offsets
    static constexpr uintptr_t OFFSET_POS_X = 0x2C;
    static constexpr uintptr_t OFFSET_POS_Y = 0x30;
    static constexpr uintptr_t OFFSET_POS_Z = 0x28;
    
    // Camera offsets
    static constexpr uintptr_t OFFSET_CAMERA_X = 0x34;
    static constexpr uintptr_t OFFSET_CAMERA_Y = 0x38;
    
    // Fast fire offsets
    static constexpr uintptr_t OFFSET_FASTFIRE_AR = 0x164;
    static constexpr uintptr_t OFFSET_FASTFIRE_SNIPER = 0x160;
    static constexpr uintptr_t OFFSET_FASTFIRE_SHOTGUN = 0x158;
    
    // Other offsets
    static constexpr uintptr_t OFFSET_AUTO_SHOOT = 0x204;
    static constexpr uintptr_t OFFSET_PLAYER_NAME = 0x205;
    
public:
    Trainer(uintptr_t base);
    ~Trainer();
    
    // Initialize trainer and find addresses
    bool Initialize();
    
    // Main loop
    void Run();
    
    // Feature functions
    void ToggleGodMode();
    void ToggleInfiniteAmmo();
    void ToggleNoRecoil();
    void AddHealth(int amount);
    void SetHealth(int value);
    void SetArmor(int value);
    void SetAmmo(int value);
    
    // Address finding
    bool FindPlayerBase();
    bool FindHealthAddress();
    bool FindAmmoAddress();
    
    // Utility
    void UpdatePlayerData();
    void DisplayStatus();
};

#endif // TRAINER_H
