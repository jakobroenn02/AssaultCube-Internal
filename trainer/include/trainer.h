#ifndef TRAINER_H
#define TRAINER_H
 
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <atomic>

// Forward declare UIRenderer to avoid circular dependency
class UIRenderer;
struct FeatureToggle;
struct PlayerStats;

class Trainer {
private:
    uintptr_t moduleBase;
    bool isRunning;
    UIRenderer* uiRenderer;
    HWND gameWindowHandle;
    
    // Feature toggles (thread-safe)
    std::atomic<bool> godMode;
    std::atomic<bool> infiniteAmmo;
    std::atomic<bool> noRecoil;
    std::atomic<bool> regenHealth;
    
    // Recoil patch data
    uintptr_t recoilPatchAddress;
    std::vector<BYTE> originalRecoilBytes;
    bool recoilPatched;
    
    // Player addresses (to be found)
    uintptr_t playerBase;
    uintptr_t healthAddress;
    uintptr_t armorAddress;
    uintptr_t ammoAddress;
    uintptr_t recoilXAddress;
    uintptr_t recoilYAddress;

    // Frame timing
    std::chrono::steady_clock::time_point lastRenderTime;

    // Input routing state
    std::atomic<bool> messagePumpInputEnabled;
    
    // Original bytes for patching/unpatching
    std::vector<BYTE> originalHealthBytes;
    std::vector<BYTE> originalAmmoBytes;
    
    // Static offsets from addresses.md (updated)
    static constexpr uintptr_t OFFSET_LOCALPLAYER = 0x17B0B8;
    static constexpr uintptr_t OFFSET_ENTITY_LIST = 0x187C10;
    static constexpr uintptr_t OFFSET_VIEW_MATRIX = 0x17AFE0;
    static constexpr uintptr_t OFFSET_PLAYER_COUNT = 0x18EFE4;
    
    // Player offsets
    static constexpr uintptr_t OFFSET_HEALTH = 0xEC;
    static constexpr uintptr_t OFFSET_ARMOR = 0xF0;
    static constexpr uintptr_t OFFSET_TEAM_ID = 0x30C;
    static constexpr uintptr_t OFFSET_RECOIL_X = 0xCB;
    static constexpr uintptr_t OFFSET_RECOIL_Y = 0xCC;
    
    // Ammo offsets
    static constexpr uintptr_t OFFSET_AR_AMMO = 0x140;
    static constexpr uintptr_t OFFSET_SMG_AMMO = 0x138;
    static constexpr uintptr_t OFFSET_SNIPER_AMMO = 0x13C;
    static constexpr uintptr_t OFFSET_SHOTGUN_AMMO = 0x134;
    static constexpr uintptr_t OFFSET_PISTOL_AMMO = 0x12C;
    static constexpr uintptr_t OFFSET_GRENADE_AMMO = 0x144;
    
    // Position offsets (vec3)
    static constexpr uintptr_t OFFSET_VEC3_ORIGIN = 0x28;
    static constexpr uintptr_t OFFSET_VEC3_HEAD = 0x4;
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

    void ShutdownOverlay();

public:
    Trainer(uintptr_t base);
    ~Trainer();
    
    // Initialize trainer and find addresses
    bool Initialize();
    
    // Main loop
    void Run();

    // Overlay input processing
    void ProcessOverlayInput();
    bool ProcessMessage(MSG& msg, bool inputCaptureEnabled);

    // Feature functions
    void ToggleGodMode();
    void ToggleInfiniteAmmo();
    void ToggleNoRecoil();
    void ToggleRegenHealth();
    void InstantRefillHealth();
    void RequestUnload();
    
    void SetHealth(int value);
    void SetArmor(int value);
    void SetAmmo(int value);
    
    // Address finding
    bool FindPlayerBase();
    bool FindHealthAddress();
    bool FindAmmoAddress();
    bool FindRecoilPatchAddress();
    
    // Recoil patching
    void ApplyRecoilPatch();
    void RestoreRecoilBytes();

    // Utility
    void UpdatePlayerData();
    void DisplayStatus();
    
    // Build feature toggles for UI
    std::vector<FeatureToggle> BuildFeatureToggles();
    PlayerStats GetPlayerStats();

    // Accessors
    UIRenderer* GetUIRenderer() const { return uiRenderer; }
    HWND GetGameWindowHandle() const { return gameWindowHandle; }
    void SetMessagePumpInputEnabled(bool enabled);
    bool IsMessagePumpInputEnabled() const { return messagePumpInputEnabled.load(); }
    void SetOverlayMenuVisible(bool visible);
};

#endif // TRAINER_H
