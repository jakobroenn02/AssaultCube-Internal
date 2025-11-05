#include "pch.h"
#include "trainer.h"
#include "aimbot.h"
#include "memory.h"
#include <iostream>
#include <cmath>
#include <vector>

namespace Aimbot {

// Global debug flag (set by trainer, read by aimbot functions)
static std::atomic<bool> g_debugLogging{false};

// Cache storage (valid for 100ms per entry, max 50 entries)
static std::vector<LOSCache> losCache;

// The actual LOS check function (lightweight - only checks midpoint)
bool CheckLOSSimple(uintptr_t localEntity, uintptr_t targetEntity) {
    // Get positions
    float x1 = *(float*)(localEntity + 0x4);
    float y1 = *(float*)(localEntity + 0x8);
    float z1 = *(float*)(localEntity + 0xC);

    float x2 = *(float*)(targetEntity + 0x4);
    float y2 = *(float*)(targetEntity + 0x8);
    float z2 = *(float*)(targetEntity + 0xC);

    // Quick distance check first
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    float distSq = dx*dx + dy*dy + dz*dz;

    if (distSq > 40000.0f) {
        return false; // Too far (>200 units)
    }

    __try {
        // GET BASE ADDRESS FIRST!
        uintptr_t acClientBase = 0;
        size_t moduleSize = 0;
        if (!Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) {
            return true;
        }

        // Use relative offsets from base
        uintptr_t mapData = Memory::Read<uintptr_t>(acClientBase + 0x00182938);
        if (!mapData) {
            if (g_debugLogging.load()) {
                std::cout << "[LOS] No map data" << std::endl;
            }
            return true;
        }

        byte gridShift = Memory::Read<byte>(acClientBase + 0x00182930);
        int mapSize = Memory::Read<int>(acClientBase + 0x00182934);

        if (g_debugLogging.load()) {
            std::cout << "[LOS] Base: 0x" << std::hex << acClientBase
                      << " MapData: 0x" << mapData << std::dec
                      << " GridShift: " << (int)gridShift
                      << " MapSize: " << mapSize << std::endl;

            std::cout << "[LOS] Positions: Local(" << x1 << "," << y1 << "," << z1
                      << ") Target(" << x2 << "," << y2 << "," << z2 << ")" << std::endl;
        }

        // Check multiple points along the ray (not just midpoint)
        // This catches walls that aren't exactly at the center
        const int numChecks = 5; // Check 5 points along the line
        for (int i = 0; i < numChecks; i++) {
            float t = (i + 1.0f) / (numChecks + 1.0f); // Interpolation factor (skip endpoints)
            float checkX = x1 + (x2 - x1) * t;
            float checkY = y1 + (y2 - y1) * t;
            float checkZ = z1 + (z2 - z1) * t;

            // World coordinates ARE grid coordinates - just convert to int
            int gx = (int)checkX;
            int gy = (int)checkY;

            if (g_debugLogging.load() && i == numChecks / 2) { // Log the middle check point
                std::cout << "[LOS] Check point " << i << ": (" << checkX << ", " << checkY << ", " << checkZ << ")" << std::endl;
                std::cout << "[LOS] Grid: (" << gx << ", " << gy << ")" << std::endl;
            }

            // Bounds check
            if (gx < 0 || gy < 0 || gx >= mapSize || gy >= mapSize) {
                if (g_debugLogging.load() && i == numChecks / 2) {
                    std::cout << "[LOS] Out of bounds" << std::endl;
                }
                continue; // Skip this check point
            }

            // Calculate index: (GridY << gridShift) + GridX
            int index = (gy << gridShift) + gx;

            // Read cube data
            byte cubeType = Memory::Read<byte>(mapData + index * 0x10);

            // Cube types (from AssaultCube source code world.h):
            // 0 = SOLID - entirely solid cube (full wall)
            // 1 = CORNER - half full corner wall (diagonal)
            // 2 = FHF - floor heightfield
            // 3 = CHF - ceiling heightfield
            if (cubeType >= 0 && cubeType <= 3) { // Any geometry type can block line of sight
                char floor = Memory::Read<char>(mapData + index * 0x10 + 1);
                char ceiling = Memory::Read<char>(mapData + index * 0x10 + 2);

                // Check if the ray passes through the solid geometry
                // SOLID (0) blocks everything between floor and ceiling
                // CORNER (1), FHF (2), CHF (3) also block if within their height range
                if (checkZ >= floor && checkZ <= ceiling) {
                    if (g_debugLogging.load()) {
                        std::cout << "[LOS] *** BLOCKED BY WALL at check point " << i << " ***" << std::endl;
                        std::cout << "[LOS] Type=" << (int)cubeType << " Floor=" << (int)floor
                                  << " Ceil=" << (int)ceiling << " CheckZ=" << checkZ << std::endl;
                    }
                    return false;
                }
            }
        }

        return true;

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        if (g_debugLogging.load()) {
            std::cout << "[LOS] Exception!" << std::endl;
        }
        return true;
    }
}

// The cached wrapper (performance optimization to avoid redundant map lookups)
bool HasClearLineOfSight(uintptr_t localEntity, uintptr_t targetEntity) {
    DWORD now = GetTickCount();

    static int callCount = 0;
    if (g_debugLogging.load() && ++callCount % 30 == 0) {  // Log every 30th call
        std::cout << "[LOS] HasClearLineOfSight called (local: 0x" << std::hex << localEntity
                  << ", target: 0x" << targetEntity << std::dec << ")" << std::endl;
    }

    // Check cache first (valid for 100ms)
    for (auto& cache : losCache) {
        if (cache.entity1 == localEntity && cache.entity2 == targetEntity) {
            if (now - cache.timestamp < 100) {
                if (g_debugLogging.load() && callCount % 30 == 0) {
                    std::cout << "[LOS] Cache HIT - returning " << (cache.result ? "true" : "false") << std::endl;
                }
                return cache.result; // Return cached - no map lookup needed!
            }
        }
    }

    if (g_debugLogging.load() && callCount % 30 == 0) {
        std::cout << "[LOS] Cache MISS - performing actual check" << std::endl;
    }

    // Cache miss - do the actual check
    bool result = CheckLOSSimple(localEntity, targetEntity);

    // Store in cache for next time
    losCache.push_back({localEntity, targetEntity, result, now});
    if (losCache.size() > 50) losCache.erase(losCache.begin());

    return result;
}

// Check if target is likely visible (not through walls)
// This is a simple heuristic based on height difference and angle
bool IsTargetLikelyVisible(float localX, float localY, float localZ,
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
uintptr_t FindClosestEnemy(Trainer* trainer, float& outDistance) {
    if (!trainer) {
        outDistance = -1.0f;
        return 0;
    }

    // Get local player position
    float localX, localY, localZ;
    trainer->GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Adjust to eye level for better visibility checks

    // Get player base and local player team
    uintptr_t acClientBase = 0;
    size_t moduleSize = 0;
    if (!Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) {
        outDistance = -1.0f;
        return 0;
    }
    uintptr_t playerBase = Memory::Read<uintptr_t>(acClientBase + 0x0017E0A8);
    int localTeam = Memory::Read<int>(playerBase + 0x30C);

    // Get all players
    std::vector<uintptr_t> players;
    if (!trainer->GetPlayerList(players)) {
        outDistance = -1.0f;
        return 0;
    }

    if (g_debugLogging.load()) {
        std::cout << "[AIMBOT] FindClosestEnemy: Got " << players.size() << " players to check" << std::endl;
        std::cout << "[AIMBOT] Local team: " << localTeam << ", playerBase: 0x" << std::hex << playerBase << std::dec << std::endl;
    }

    // Detect FFA mode: In FFA, all players have same team but are enemies
    // If there are 2+ players and they all have the same team, it's FFA
    bool isFFA = false;
    if (players.size() >= 2) {
        int firstPlayerTeam = trainer->GetPlayerTeam(players[0]);
        bool allSameTeam = true;
        for (size_t i = 1; i < players.size() && allSameTeam; ++i) {
            if (trainer->GetPlayerTeam(players[i]) != firstPlayerTeam) {
                allSameTeam = false;
            }
        }
        isFFA = (allSameTeam && firstPlayerTeam == localTeam);
    }

    if (g_debugLogging.load()) {
        std::cout << "[AIMBOT] Game mode: " << (isFFA ? "FFA (everyone is enemy)" : "Team-based") << std::endl;
    }

    uintptr_t closestEnemy = 0;
    float closestDistance = 99999.0f;

    for (uintptr_t playerPtr : players) {
        // Skip invalid or dead players
        if (!trainer->IsPlayerValid(playerPtr) || !trainer->IsPlayerAlive(playerPtr)) {
            if (g_debugLogging.load()) {
                std::cout << "[AIMBOT] Skipping player 0x" << std::hex << playerPtr << std::dec
                          << " - invalid or dead" << std::endl;
            }
            continue;
        }

        // Skip teammates (only in team-based modes, not FFA)
        if (!isFFA) {
            int playerTeam = trainer->GetPlayerTeam(playerPtr);
            if (playerTeam == localTeam) {
                if (g_debugLogging.load()) {
                    std::cout << "[AIMBOT] Skipping player 0x" << std::hex << playerPtr << std::dec
                              << " - teammate (team " << playerTeam << ")" << std::endl;
                }
                continue;
            }
        }

        // Get enemy position
        float enemyX, enemyY, enemyZ;
        trainer->GetPlayerPosition(playerPtr, enemyX, enemyY, enemyZ);
        enemyZ += 10.0f;  // Adjust to target's head level

        // Calculate 3D distance
        float dx = enemyX - localX;
        float dy = enemyY - localY;
        float dz = enemyZ - localZ;
        float distance = sqrt(dx * dx + dy * dy + dz * dz);

        // Wall detection check (only if not ignoring walls)
        if (!trainer->GetAimbotIgnoreWalls()) {
            if (!HasClearLineOfSight(playerBase, playerPtr)) {
                continue;  // Blocked by wall/obstacle
            }

            // Second check: Heuristic fallback for vertical walls/floors
            if (!IsTargetLikelyVisible(localX, localY, localZ, enemyX, enemyY, enemyZ)) {
                continue;  // Skip targets that are likely behind walls
            }
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
void CalculateAngles(const float* from, const float* to, float& yaw, float& pitch) {
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

// Predict target position based on velocity (for moving targets)
void PredictTargetPosition(uintptr_t targetPtr, float& targetX, float& targetY, float& targetZ, float predictionTime) {
    // Read current velocity from target entity
    float velX = Memory::Read<float>(targetPtr + 0x10);
    float velY = Memory::Read<float>(targetPtr + 0x14);
    float velZ = Memory::Read<float>(targetPtr + 0x18);

    // Calculate velocity magnitude to check if target is moving
    float velocityMag = sqrt(velX * velX + velY * velY + velZ * velZ);

    // Only apply prediction if target is actually moving (threshold: 1 unit/sec)
    if (velocityMag > 1.0f) {
        // For faster moving targets, slightly increase prediction multiplier
        float speedMultiplier = 1.0f + (velocityMag / 100.0f) * 0.2f;  // Up to 20% boost for fast targets
        float adjustedPrediction = predictionTime * speedMultiplier;

        // Calculate predicted position: position + (velocity * time)
        targetX += velX * adjustedPrediction;
        targetY += velY * adjustedPrediction;
        targetZ += velZ * adjustedPrediction;
    }
}

// Calculate FOV (field of view) angle to a target
// Returns the angle in degrees between crosshair and target
float CalculateFOVToTarget(Trainer* trainer, uintptr_t targetPtr) {
    if (!trainer || !targetPtr) return 999.0f;

    // Get local player position and angles
    float localX, localY, localZ;
    trainer->GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Head height

    float currentYaw, currentPitch;
    trainer->GetLocalPlayerAngles(currentYaw, currentPitch);

    // Get target position
    float targetX, targetY, targetZ;
    trainer->GetPlayerPosition(targetPtr, targetX, targetY, targetZ);
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
uintptr_t FindClosestEnemyToCrosshair(Trainer* trainer, float& outFOV) {
    if (!trainer) {
        outFOV = -1.0f;
        return 0;
    }

    // Get local player position for visibility checks
    float localX, localY, localZ;
    trainer->GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Adjust to eye level

    // Get player base and local player team
    uintptr_t acClientBase = 0;
    size_t moduleSize = 0;
    if (!Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) {
        outFOV = -1.0f;
        return 0;
    }
    uintptr_t playerBase = Memory::Read<uintptr_t>(acClientBase + 0x0017E0A8);
    int localTeam = Memory::Read<int>(playerBase + 0x30C);

    // Get all players
    std::vector<uintptr_t> players;
    if (!trainer->GetPlayerList(players)) {
        outFOV = -1.0f;
        return 0;
    }

    if (g_debugLogging.load()) {
        std::cout << "[AIMBOT] FindClosestEnemyToCrosshair: Got " << players.size() << " players to check" << std::endl;
        std::cout << "[AIMBOT] Local team: " << localTeam << ", playerBase: 0x" << std::hex << playerBase << std::dec << std::endl;
    }

    // Detect FFA mode: In FFA, all players have same team but are enemies
    bool isFFA = false;
    if (players.size() >= 2) {
        int firstPlayerTeam = trainer->GetPlayerTeam(players[0]);
        bool allSameTeam = true;
        for (size_t i = 1; i < players.size() && allSameTeam; ++i) {
            if (trainer->GetPlayerTeam(players[i]) != firstPlayerTeam) {
                allSameTeam = false;
            }
        }
        isFFA = (allSameTeam && firstPlayerTeam == localTeam);
    }

    if (g_debugLogging.load()) {
        std::cout << "[AIMBOT] Game mode: " << (isFFA ? "FFA (everyone is enemy)" : "Team-based") << std::endl;
    }

    uintptr_t closestEnemy = 0;
    float closestFOV = 999.0f;
    float maxFOV = trainer->GetAimbotFOV();  // Only consider enemies within this FOV

    for (uintptr_t playerPtr : players) {
        // Skip invalid or dead players
        if (!trainer->IsPlayerValid(playerPtr) || !trainer->IsPlayerAlive(playerPtr)) {
            if (g_debugLogging.load()) {
                std::cout << "[AIMBOT] Skipping player 0x" << std::hex << playerPtr << std::dec
                          << " - invalid or dead" << std::endl;
            }
            continue;
        }

        // Skip teammates (only in team-based modes, not FFA)
        if (!isFFA) {
            int playerTeam = trainer->GetPlayerTeam(playerPtr);
            if (playerTeam == localTeam) {
                if (g_debugLogging.load()) {
                    std::cout << "[AIMBOT] Skipping player 0x" << std::hex << playerPtr << std::dec
                              << " - teammate (team " << playerTeam << ")" << std::endl;
                }
                continue;
            }
        }

        // Get target position for visibility check
        float targetX, targetY, targetZ;
        trainer->GetPlayerPosition(playerPtr, targetX, targetY, targetZ);
        targetZ += 10.0f;  // Adjust to target's head level

        // Wall detection check (only if not ignoring walls)
        if (!trainer->GetAimbotIgnoreWalls()) {
            // First check: Use game's true line-of-sight function
            if (!HasClearLineOfSight(playerBase, playerPtr)) {
                continue;  // Blocked by wall/obstacle according to game engine
            }

            // Second check: Heuristic fallback for vertical walls/floors
            if (!IsTargetLikelyVisible(localX, localY, localZ, targetX, targetY, targetZ)) {
                continue;  // Skip targets that are likely behind walls
            }
        }

        // Calculate FOV to this target
        float fov = CalculateFOVToTarget(trainer, playerPtr);

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

// Smoothly adjust aim toward target angles with exponential smoothing
void SmoothAim(Trainer* trainer, float targetYaw, float targetPitch) {
    if (!trainer) return;

    uintptr_t acClientBase = 0;
    size_t moduleSize = 0;
    if (!Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) return;

    uintptr_t playerBase = Memory::Read<uintptr_t>(acClientBase + 0x0017E0A8);
    if (!playerBase) return;

    // Get current angles
    float currentYaw = Memory::Read<float>(playerBase + 0x34);
    float currentPitch = Memory::Read<float>(playerBase + 0x38);

    // Calculate angle difference
    float deltaYaw = targetYaw - currentYaw;
    float deltaPitch = targetPitch - currentPitch;

    // Normalize yaw to -180..180 range
    while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
    while (deltaYaw < -180.0f) deltaYaw += 360.0f;

    // Get smoothness setting
    float smoothness = trainer->GetAimbotSmoothness();

    // Exponential smoothing: uses a smooth factor that creates natural easing
    // Convert smoothness (1-10 range) to exponential factor (0.1-0.9 range)
    // Lower factor = faster aim, higher factor = smoother aim
    float expFactor = 1.0f - (1.0f / (smoothness + 1.0f));

    // Apply exponential smoothing (creates natural acceleration/deceleration)
    // Formula: newAngle = currentAngle + (targetDelta * (1 - expFactor))
    float smoothYaw = deltaYaw * (1.0f - expFactor);
    float smoothPitch = deltaPitch * (1.0f - expFactor);

    // Calculate new angles
    float newYaw = currentYaw + smoothYaw;
    float newPitch = currentPitch + smoothPitch;

    // Clamp pitch to valid range (-90 to 90 degrees)
    if (newPitch > 90.0f) newPitch = 90.0f;
    if (newPitch < -90.0f) newPitch = -90.0f;

    // Write new angles
    Memory::Write<float>(playerBase + 0x34, newYaw);
    Memory::Write<float>(playerBase + 0x38, newPitch);
}

// Main aimbot update - called every frame when aimbot is active
void UpdateAimbot(Trainer* trainer) {
    if (!trainer) return;

    trainer->RefreshPlayerAddresses();

    uintptr_t acClientBase = 0;
    size_t moduleSize = 0;
    if (!Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) return;

    uintptr_t playerBase = Memory::Read<uintptr_t>(acClientBase + 0x0017E0A8);
    if (!playerBase) return;

    uintptr_t target = 0;
    bool useFOV = trainer->GetAimbotUseFOV();

    // DEBUG: Check target finding
    static int targetCheckCount = 0;
    bool debugPrint = (targetCheckCount++ % 200 == 0);

    if (useFOV) {
        // FOV-based targeting: aim at enemy closest to crosshair
        float fov = 0.0f;
        target = FindClosestEnemyToCrosshair(trainer, fov);

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
        target = FindClosestEnemy(trainer, distance);

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

    // Validate target is still valid and alive (might have died since finding it)
    if (!trainer->IsPlayerValid(target) || !trainer->IsPlayerAlive(target)) {
        // Target died - clear LOS cache to remove stale entries
        ClearLOSCache();
        return;  // Target died or became invalid - skip this frame and find new target next frame
    }

    // Get local player head position (aim from head, not feet)
    float localX, localY, localZ;
    trainer->GetLocalPlayerPosition(localX, localY, localZ);
    localZ += 10.0f;  // Add approximate head height offset

    // Get target head position
    float targetX, targetY, targetZ;
    trainer->GetPlayerPosition(target, targetX, targetY, targetZ);
    targetZ += 10.0f;  // Aim at enemy head

    // Calculate distance to target for prediction time scaling
    float dx = targetX - localX;
    float dy = targetY - localY;
    float dz = targetZ - localZ;
    float distance = sqrt(dx * dx + dy * dy + dz * dz);

    // Apply velocity-based prediction with smoothing compensation
    // Prediction time needs to account for:
    // 1. Base reaction time (~50ms)
    // 2. Distance-based bullet travel time
    // 3. Smoothing delay (higher smoothness = more prediction needed)
    float smoothness = trainer->GetAimbotSmoothness();
    float smoothingDelay = smoothness * 0.016f;  // ~1 frame per smoothness point at 60fps
    float distanceDelay = distance / 3000.0f;     // Reduced divisor for more aggressive prediction
    float predictionTime = 0.05f + distanceDelay + smoothingDelay;
    PredictTargetPosition(target, targetX, targetY, targetZ, predictionTime);

    // DEBUG: Print positions AND raw memory values every 100 frames
    static int frameCount = 0;
    if (g_debugLogging.load() && frameCount++ % 100 == 0) {
        std::cout << "[AIMBOT DEBUG]" << std::endl;
        std::cout << "  Local pos: (" << localX << ", " << localY << ", " << localZ << ")" << std::endl;
        std::cout << "  Target pos (predicted): (" << targetX << ", " << targetY << ", " << targetZ << ")" << std::endl;
        std::cout << "  Distance: " << distance << " Prediction time: " << predictionTime << "s" << std::endl;

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
        float currentYaw = Memory::Read<float>(playerBase + 0x34);
        float currentPitch = Memory::Read<float>(playerBase + 0x38);
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
    SmoothAim(trainer, targetYaw, targetPitch);
}

// Get the LOS cache
std::vector<LOSCache>& GetLOSCache() {
    return losCache;
}

// Clear the LOS cache
void ClearLOSCache() {
    losCache.clear();
}

// Check if crosshair is on an enemy (within triggerbot FOV tolerance)
bool ShouldTriggerShoot(Trainer* trainer) {
    if (!trainer) return false;

    // Get triggerbot FOV tolerance
    float maxFOV = trainer->GetTriggerbotFOV();

    // Find enemy closest to crosshair
    float fov = 0.0f;
    uintptr_t target = FindClosestEnemyToCrosshair(trainer, fov);

    // Check if we found a valid target within FOV tolerance
    if (!target || fov < 0.0f) {
        return false;  // No target found
    }

    // Check if target is within triggerbot FOV tolerance
    return (fov <= maxFOV);
}

// Main triggerbot update - called every frame when triggerbot is active
void UpdateTriggerbot(Trainer* trainer) {
    if (!trainer) return;

    // Static variables for delay handling
    static DWORD lastCheckTime = 0;
    static bool targetAcquired = false;
    static DWORD targetAcquiredTime = 0;

    DWORD currentTime = GetTickCount();

    // Check if we should shoot (crosshair on enemy)
    bool shouldShoot = ShouldTriggerShoot(trainer);

    if (shouldShoot) {
        // Target is in crosshair
        if (!targetAcquired) {
            // Just acquired target - start delay timer
            targetAcquired = true;
            targetAcquiredTime = currentTime;
        } else {
            // Target still in crosshair - check if delay has passed
            float delay = trainer->GetTriggerbotDelay();
            DWORD elapsedTime = currentTime - targetAcquiredTime;

            if (elapsedTime >= (DWORD)delay) {
                // Delay passed - simulate mouse click (shoot)
                uintptr_t acClientBase = 0;
                size_t moduleSize = 0;
                if (!Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) return;

                uintptr_t playerBase = Memory::Read<uintptr_t>(acClientBase + 0x0017E0A8);
                if (!playerBase) return;

                // Set auto-shoot flag (simulates holding mouse button)
                // This is cleaner than simulating actual mouse input
                Memory::Write<byte>(playerBase + 0x204, 1);

                // Debug output
                static int shootCount = 0;
                if (g_debugLogging.load() && shootCount++ % 10 == 0) {
                    std::cout << "[TRIGGERBOT] Shooting! (delay: " << delay << "ms)" << std::endl;
                }
            }
        }
    } else {
        // No target in crosshair
        if (targetAcquired) {
            // Lost target - reset delay timer
            targetAcquired = false;

            // Stop shooting
            uintptr_t acClientBase = 0;
            size_t moduleSize = 0;
            if (Memory::GetModuleInfo("ac_client.exe", acClientBase, moduleSize)) {
                uintptr_t playerBase = Memory::Read<uintptr_t>(acClientBase + 0x0017E0A8);
                if (playerBase) {
                    Memory::Write<byte>(playerBase + 0x204, 0);
                }
            }
        }
    }
}

// Set debug logging flag
void SetDebugLogging(bool enabled) {
    g_debugLogging.store(enabled);
}

} // namespace Aimbot
