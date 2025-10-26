#ifndef TRAINER_H
#define TRAINER_H
 
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <atomic>
#include "memory.h"
// Forward declare UIRenderer to avoid circular dependency
class UIRenderer;
struct FeatureToggle;
struct PlayerStats;

class Trainer {
    // ...existing code...
public:
    // Reads the view matrix from game memory
    void GetViewMatrix(float* outMatrix);

    // Reads the projection matrix from game memory
    void GetProjectionMatrix(float* outMatrix);

    // Gets the combined view-projection matrix (modelview * projection)
    void GetViewProjectionMatrix(float* outMatrix);
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
    std::atomic<bool> esp;  // Wallhack/ESP feature
    std::atomic<bool> aimbot;  // Aimbot feature
    std::atomic<float> aimbotSmoothness;  // Aimbot smoothness (1.0 = instant, higher = smoother)
    std::atomic<float> aimbotFOV;  // FOV cone for aimbot (degrees)
    std::atomic<bool> aimbotUseFOV;  // True = aim at closest to crosshair, False = closest distance
    
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
    static constexpr uintptr_t OFFSET_LOCALPLAYER = 0x0017E0A8;  // Absolute: 0x0057E0A8 - Also the camera pointer!
    static constexpr uintptr_t OFFSET_CAMERA_PTR = 0x0017E0A8;   // Absolute: 0x0057E0A8 - Camera/view pointer (same as local player)
    static constexpr uintptr_t OFFSET_ENTITY_LIST = 0x0018AC04;   // Absolute: 0x0058AC04
    static constexpr uintptr_t OFFSET_VIEW_MATRIX = 0x0017E010;   // Absolute: 0x0057E010 - GL_MODELVIEW_MATRIX (row-major: rotation + translation)
    static constexpr uintptr_t OFFSET_PROJECTION_MATRIX = 0x0017E0B0;  // Absolute: 0x0057E0B0 - GL_PROJECTION_MATRIX (64 bytes)
    static constexpr uintptr_t OFFSET_PLAYER_COUNT = 0x0018AC0C;  // Absolute: 0x0058AC0C
    
    // Camera structure offsets (from camera pointer at 0x0057E0A8)
    static constexpr uintptr_t OFFSET_CAMERA_X = 0x04;  // Eye/camera X position
    static constexpr uintptr_t OFFSET_CAMERA_Y = 0x08;  // Eye/camera Y position
    static constexpr uintptr_t OFFSET_CAMERA_Z = 0x0C;  // Eye/camera Z position (already at eye height!)

    // Player offsets
    static constexpr uintptr_t OFFSET_HEALTH = 0xEC;
    static constexpr uintptr_t OFFSET_ARMOR = 0xF0;
    static constexpr uintptr_t OFFSET_TEAM_ID = 0x30C;
    
    // Recoil system (CORRECTED - based on disassembly)
    static constexpr uintptr_t OFFSET_WEAPON_RECOIL_PROPERTY = 0x40;  // Float: weapon recoil calculation multiplier
    static constexpr uintptr_t OFFSET_RECOIL_X = 0x32C;  // Float: Accumulated recoil X (horizontal)
    static constexpr uintptr_t OFFSET_RECOIL_Y = 0x330;  // Float: Accumulated recoil Y (vertical)
    static constexpr uintptr_t OFFSET_MAX_RECOIL = 0x324; // Float: Maximum recoil limit
    static constexpr uintptr_t OFFSET_IS_SHOOTING = 0x61;  // Byte: 1 when firing
    
    // Camera angles (affected by recoil)
    static constexpr uintptr_t OFFSET_YAW = 0x34;    // Float: Horizontal rotation (camera)
    static constexpr uintptr_t OFFSET_PITCH = 0x38;  // Float: Vertical rotation (modified by recoil)
    static constexpr uintptr_t OFFSET_ROLL = 0x3C;   // Float: Roll angle (influenced by recoil)
    
    // Movement system (DO NOT MODIFY - controls player input)
    static constexpr uintptr_t OFFSET_MOVEMENT_DIR1 = 0x74;  // Signed byte: forward/backward
    static constexpr uintptr_t OFFSET_MOVEMENT_DIR2 = 0x75;  // Signed byte: strafe left/right (A/D keys)
    static constexpr uintptr_t OFFSET_PLAYER_STATE1 = 0x76;  // Byte: player mode/state
    static constexpr uintptr_t OFFSET_PLAYER_STATE2 = 0x77;  // Byte: environment state (water/ground)
    
    // Position & Velocity
    static constexpr uintptr_t OFFSET_POS_X_ACTUAL = 0x04;  // Float: Player X position (feet)
    static constexpr uintptr_t OFFSET_POS_Y_ACTUAL = 0x08;  // Float: Player Y position (feet)
    static constexpr uintptr_t OFFSET_POS_Z_ACTUAL = 0x0C;  // Float: Player Z position (feet)
    static constexpr uintptr_t OFFSET_VEL_X = 0x10;  // Float: Velocity X
    static constexpr uintptr_t OFFSET_VEL_Y = 0x14;  // Float: Velocity Y
    static constexpr uintptr_t OFFSET_VEL_Z = 0x18;  // Float: Velocity Z
    static constexpr uintptr_t OFFSET_MOVEMENT_SPEED = 0x44;  // Float: Movement speed multiplier

    // Player height and head position (more accurate for ESP)
    static constexpr uintptr_t OFFSET_PLAYER_HEIGHT = 0x38;    // Float: Current player height constant
    static constexpr uintptr_t OFFSET_EYE_HEIGHT = 0x50;       // Float: Eye height offset from feet (old method)
    static constexpr uintptr_t OFFSET_HEAD_X = 0x3F8;          // Float: Pre-calculated head X position
    static constexpr uintptr_t OFFSET_HEAD_Y = 0x3FC;          // Float: Pre-calculated head Y position
    static constexpr uintptr_t OFFSET_HEAD_Z = 0x400;          // Float: Pre-calculated head Z position
    
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
    void ToggleESP();
    void ToggleAimbot();
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
    void RefreshPlayerAddresses();

    // Build feature toggles for UI
    std::vector<FeatureToggle> BuildFeatureToggles();
    PlayerStats GetPlayerStats();
    
    // ESP / Wallhack functions
    bool GetPlayerList(std::vector<uintptr_t>& players);
    int GetPlayerCount();
    bool IsPlayerValid(uintptr_t playerPtr);
    bool IsPlayerAlive(uintptr_t playerPtr);
    int GetPlayerTeam(uintptr_t playerPtr);
    void GetPlayerPosition(uintptr_t playerPtr, float& x, float& y, float& z);
    void GetPlayerHeadPosition(uintptr_t playerPtr, float& x, float& y, float& z);
    void GetPlayerName(uintptr_t playerPtr, char* name, size_t maxLen);
    void GetLocalPlayerPosition(float& x, float& y, float& z);
    void GetLocalPlayerAngles(float& yaw, float& pitch);

    // Aimbot helper functions
    uintptr_t FindClosestEnemy(float& outDistance);
    uintptr_t FindClosestEnemyToCrosshair(float& outFOV);
    void CalculateAngles(const float* from, const float* to, float& yaw, float& pitch);
    float CalculateFOVToTarget(uintptr_t targetPtr);
    void SmoothAim(float targetYaw, float targetPitch);
    void UpdateAimbot();

    // Aimbot settings accessors
    float GetAimbotSmoothness() const { return aimbotSmoothness.load(); }
    void SetAimbotSmoothness(float value) { aimbotSmoothness.store(value); }
    float GetAimbotFOV() const { return aimbotFOV.load(); }
    void SetAimbotFOV(float value) { aimbotFOV.store(value); }
    bool GetAimbotUseFOV() const { return aimbotUseFOV.load(); }
    void SetAimbotUseFOV(bool value) { aimbotUseFOV.store(value); }

    // Accessors
    UIRenderer* GetUIRenderer() const { return uiRenderer; }
    HWND GetGameWindowHandle() const { return gameWindowHandle; }
    void SetMessagePumpInputEnabled(bool enabled);
    bool IsMessagePumpInputEnabled() const { return messagePumpInputEnabled.load(); }
    void SetOverlayMenuVisible(bool visible);
    bool IsESPEnabled() const { return esp.load(); }
    bool IsAimbotEnabled() const { return aimbot.load(); }
};

#endif // TRAINER_H
