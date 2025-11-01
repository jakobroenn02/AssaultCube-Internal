#ifndef AIMBOT_H
#define AIMBOT_H

#include <Windows.h>
#include <cstdint>
#include <vector>

// Forward declarations
class Trainer;

namespace Aimbot {

// Line-of-sight cache structure
struct LOSCache {
    uintptr_t entity1;
    uintptr_t entity2;
    bool result;
    DWORD timestamp;
};

// Check if target has clear line of sight (no walls blocking)
bool HasClearLineOfSight(uintptr_t localEntity, uintptr_t targetEntity);

// Simple LOS check using game's collision grid (internal helper)
bool CheckLOSSimple(uintptr_t localEntity, uintptr_t targetEntity);

// Heuristic check for target visibility based on height and angle
bool IsTargetLikelyVisible(float localX, float localY, float localZ,
                           float targetX, float targetY, float targetZ);

// Find the closest enemy by distance
uintptr_t FindClosestEnemy(Trainer* trainer, float& outDistance);

// Find the closest enemy to crosshair (within FOV cone)
uintptr_t FindClosestEnemyToCrosshair(Trainer* trainer, float& outFOV);

// Calculate aim angles from one position to another
void CalculateAngles(const float* from, const float* to, float& yaw, float& pitch);

// Predict target position based on velocity
void PredictTargetPosition(uintptr_t targetPtr, float& targetX, float& targetY, float& targetZ, float predictionTime);

// Calculate FOV angle to target
float CalculateFOVToTarget(Trainer* trainer, uintptr_t targetPtr);

// Smooth aim towards target angles
void SmoothAim(Trainer* trainer, float targetYaw, float targetPitch);

// Main aimbot update function (called every frame)
void UpdateAimbot(Trainer* trainer);

// Triggerbot: Check if crosshair is on enemy and should shoot
bool ShouldTriggerShoot(Trainer* trainer);

// Triggerbot: Main update function (called every frame)
void UpdateTriggerbot(Trainer* trainer);

// Get/clear the LOS cache
std::vector<LOSCache>& GetLOSCache();
void ClearLOSCache();

} // namespace Aimbot

#endif // AIMBOT_H
