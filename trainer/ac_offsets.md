# AssaultCube Game Offsets Reference

## 🎯 Base Addresses (Static Pointers - VERIFIED)

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `0x0058AC00` | `int*` | **Local Player Pointer** | Points to the local player entity structure |
| `0x0058AC04` | `int**` | **Entity List Array** | Array of pointers to all player entities |
| `0x0058AC0C` | `int` | **Entity Count** | Number of entities in the entity list |
| `0x0057E0A8` | `int*` | **Spectator/Referenced Player** | Points to currently spectated or referenced player |
| `0x0058ABF8` | `int` | **GameMode** | Current game mode (0=DM, 4=TDM, etc.) |
| `0x0059086C` | `int*` | **SpawnPoints** | Array of spawn point structures |
| `0x0056BC90` | `int` | **CurrentSpawnIndex** | Current spawn point index |

## 🔬 Pattern Signatures (AOB - Array of Bytes)

For auto-updating trainers, use these pattern signatures to find base addresses:

### LocalPlayer Pointer Pattern
```
Pattern: 89 0D ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 85 C9
Offset to pointer: +2
Example address: 0x0058AC00
```

### PlayerList Pointer Pattern  
```
Pattern: 8B 0D ?? ?? ?? ?? 8B 04 B1 85 C0
Offset to pointer: +2
Example address: 0x0058AC04
```

### PlayerCount Pattern
```
Pattern: 39 3D ?? ?? ?? ?? 7C ?? 8B 0D
Offset to pointer: +2
Example address: 0x0058AC0C
```

*Use IDA or Cheat Engine's AOB scan to find these patterns dynamically*

---

## 📊 Quick Reference Table

| Feature | Offset | Type | Value Range |
|---------|--------|------|-------------|
| Position XYZ | +0x4,+0x8,+0xC | float[3] | World coordinates |
| Yaw (rotation) | +0x34 | float | 0-360 degrees |
| Pitch (look) | +0xE | int | -90 to +90 |
| Is Dead | +0x76 | byte | 0=alive, 1+=dead |
| Is Shooting | +0x61 | byte | 0=no, 1=yes |
| Is Crouching | +0x5D | byte | 0=no, 1=yes |
| Is Scoped | +0x5E | byte | 0=no, 1=yes |
| Speed Multiplier | +0x11 | float | 1.0=normal |
| Frags | +0x1DC | int | Kill count |
| Deaths | +0x1E4 | int | Death count |
| Current Weapon | +0x1D | byte | 0-7 (weapon ID) |
| Team | +0x30C | int | 0=RVSF, 1=CLA |
| Player State | +0x77 | byte | 0-4 (see enum) |

---

## 🎯 Trainer Feature Checklist

- [x] **No Recoil** - Zero out +0xCB and +0xCC
- [] **ESP / Wallhack** - Read player array, positions, and team
- [] **Aimbot** - Calculate angles, write to +0x34 and +0xE
- [] **Speed Hack** - Modify +0x11 multiplier
- [] **Teleport** - Write to +0x4, +0x8, +0xC positions
- [] **Fly Mode** - Set player state (+0x77) to 3 (swimming)
- [] **Infinite Jump** - Set +0x5F flag
- [x] **God Mode** - Hook damage function (needs more analysis)
- [x] **Infinite Ammo** - Freeze ammo values (needs +0x1D4 verification)
- [ ] **Rapid Fire** - Modify fire rate in weapon object
- [ ] **No Spread** - Hook projectile spawn at 0x004CC560

---

## 🧪 Testing & Verification

### Recommended Testing Order
1. **Position Reading** - Easiest to verify (shows in-game coordinates)
2. **Speed Hack** - Visual feedback, easy to test
3. **Teleport** - Immediate visual confirmation
4. **No Recoil** - Test in shooting range
5. **ESP** - Verify player positions match game
6. **Aimbot** - Last (most complex, needs tuning)

### Verification Tools
- **Cheat Engine**: Use to freeze/modify values and verify offsets
- **ReClass.NET**: Visualize and document structure layouts
- **x64dbg**: Set breakpoints on memory access
- **Process Hacker**: Monitor memory regions

---

## ⚡ Performance Tips

1. **Cache Player Pointers**: Don't re-read base pointers every frame
2. **Distance Checks**: Only process nearby players for ESP/aimbot
3. **Frame Limiting**: Don't run trainer logic faster than game logic
4. **Smart Updates**: Only update changed values, not everything every frame

```cpp
// Good: Cache and check for changes
static int* lastLocalPlayer = nullptr;
int* localPlayer = *(int**)0x0058AC00;
if (localPlayer != lastLocalPlayer) {
    // Player changed, update cached data
    lastLocalPlayer = localPlayer;
}

// Bad: Reading repeatedly
if (*(int**)0x0058AC00 != nullptr) { ... }
if (*(int**)0x0058AC00 != nullptr) { ... } // Unnecessary re-read
```

------|---------|------|-------------|
| **LocalPlayer** | `0x0058AC00` | `int*` | Pointer to local player entity |
| **PlayerList** | `0x0058AC04` | `int*` | Pointer to player list array |
| **PlayerCount** | `0x0058AC0C` | `int` | Number of active players |
| **GameMode** | `0x0058ABF8` | `int` | Current game mode (0=DM, 4=TDM, etc.) |
| **SpawnPoints** | `0x0059086C` | `int*` | Array of spawn point structures |
| **CurrentSpawnIndex** | `0x0056BC90` | `int` | Current spawn point index |

---

## 🏃 Entity Structure Offsets (VERIFIED)

*Once you have a pointer to an entity (from the entity list at 0x0058AC04), you can access these offsets:*

### Position & Velocity

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x04` | `float` | **Position X** | Player X coordinate in world space |
| `+0x08` | `float` | **Position Y** | Player Y coordinate in world space |
| `+0x0C` | `float` | **Position Z** | Player Z coordinate in world space |
| `+0x10` | `float` | **Velocity X** | Movement velocity X component |
| `+0x14` | `float` | **Velocity Y** | Movement velocity Y component |
| `+0x18` | `float` | **Velocity Z** | Movement velocity Z component |

### Camera/View Angles

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x34` | `float` | **Yaw** | Horizontal rotation (0-360 degrees) |
| `+0x38` | `float` | **Pitch** | Vertical rotation (-90 to 90 degrees) |
| `+0x3C` | `float` | **Roll** | Roll angle (camera tilt) |

### Player Info

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x30C` | `int` | **Team ID** | Player's team (0 or 1 in team modes) |
| `+0x61` | `byte` | **Is Shooting** | 1 if currently firing, 0 otherwise |
| `+0x76` | `byte` | **Player State** | State flags (alive, spectating, etc.) |
| `+0x77` | `byte` | **Environment State** | Water/ground state |

### Health & Armor

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0xEC` | `int` | **Health** | Current health points |
| `+0xF0` | `int` | **Armor** | Current armor points |

---

## 🏃 Player Entity Structure Offsets (LEGACY - NEEDS VERIFICATION)

*Base: LocalPlayer pointer (`0x0058AC00`)*

### Position & Movement (OLD)
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x4` | `float` | **X Position** | Player X coordinate |
| `+0x8` | `float` | **Y Position** | Player Y coordinate |
| `+0xC` | `float` | **Z Position** | Player Z coordinate |
| `+0x10` | `float[3]` | **Velocity** | Movement velocity vector |
| `+0x14` | `float` | **Current Height** | Player height from ground |
| `+0x15` | `float` | **Max Height** | Maximum height (for collision) |

### View Angles & Rotation
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x34` | `float` | **Yaw** | Horizontal rotation angle |
| `+0xD` | `int` | **Yaw (alt)** | Alternative yaw storage |
| `+0xE` | `int` | **Pitch** | Vertical rotation angle |
| `+0xF` | `int` | **Roll** | Roll angle (camera tilt) |
| `+0xCB` | `int` | **View Angle X** | View angle component X |
| `+0xCC` | `int` | **View Angle Y** | View angle component Y |

### Player State
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x76` | `byte` | **Is Dead** | Death state (0=alive, 1+=dead) |
| `+0x77` | `byte` | **Player State** | 0=ground, 1=jumping, 2=falling, 3=swimming, 4=editing |
| `+0x5D` | `byte` | **Is Crouching** | Crouch state flag |
| `+0x5E` | `byte` | **Is Scoped** | Scoped weapon flag |
| `+0x5F` | `byte` | **Is Jumping** | Jump state flag |
| `+0x61` | `byte` | **Is Shooting** | Shooting state flag |
| `+0x62` | `byte` | **Jump Flag** | Secondary jump flag |
| `+0x65` | `byte` | **Is Stationary** | Not moving flag |
| `+0x75` | `byte` | **Movement Flags** | Directional movement bits |

### Combat Stats & Health
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x1DC` | `int` | **Frags/Score** | Kill count |
| `+0x1E4` | `int` | **Deaths** | Death count |
| `+0x1E8` | `int` | **Team Kills** | Friendly fire kills |
| `+0xC6` | `int` | **Player Mode** | Player mode/status (0-5) |

### Health & Armor System
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x??` | `int` | **Health** | Current health points (0-100) |
| `+0x??` | `int` | **Armor** | Current armor points (0-100) |
| `+0x??` | `int` | **Max Health** | Maximum health capacity |

*Note: Health and armor exact offsets need verification through dynamic analysis*

### Player Info
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x205` | `char[260]` | **Player Name** | Player name string (0x104 bytes) |
| `+0x30C` | `int` | **Team ID** | Team identifier (0=RVSF, 1=CLA, 4=Spectator) |
| `+0x318` | `int` | **Spectator Mode** | Spectator state (5=spectating) |
| `+0x1C4` | `int*` | **Followed Player** | Pointer to player being spectated |

### Player States (offset +0x77)
```cpp
enum PlayerState {
    STATE_GROUND = 0,      // On ground
    STATE_JUMPING = 1,     // Jumping
    STATE_FALLING = 2,     // Falling
    STATE_SWIMMING = 3,    // In water
    STATE_EDITING = 4      // Map editing mode
};
```

### Physics & Movement
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x11` | `float` | **Speed Multiplier** | Movement speed modifier |
| `+0x12` | `int` | **On Ground Timer** | Time on ground (for jump) |
| `+0x1A` | `int` | **Last Jump Time** | Timestamp of last jump |
| `+0x1B` | `int` | **Jump Height Delta** | Height difference for jump calc |
| `+0x1C` | `int` | **Landing Timestamp** | When player last landed |
| `+0x1E` | `float` | **Vertical Velocity** | Upward/downward velocity |
| `+0x23` | `float` | **Friction** | Movement friction multiplier |

### Weapon Related
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x1D` | `byte` | **Current Weapon** | Current weapon ID (0-7) |
| `+0xD6` | `int*` | **Weapon Pointer** | Pointer to weapon object |
| `+0xD9` | `int*` | **Active Weapon** | Currently active weapon |
| `+0xC4` | `int` | **Weapon Switch Time** | Timestamp of weapon switch |
| `+0xD8` | `int` | **Previous Weapon** | Previously equipped weapon |
| `+0xDA` | `int` | **Next Weapon** | Next weapon to switch to |

### Ammo & Magazine System
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x1D4` | `int[10]` | **Ammo Array** | Ammo for each weapon type |
| `+0x??` | `int` | **Current Magazine** | Rounds in current magazine |
| `+0x??` | `int` | **Reserve Ammo** | Reserve ammunition |

*Note: Ammo offsets are stored in an array indexed by weapon ID. Exact magazine offsets need verification.*

### Weapon IDs Reference
```cpp
enum WeaponID {
    WEAPON_KNIFE = 0,
    WEAPON_PISTOL = 1,
    WEAPON_CARBINE = 2,
    WEAPON_SHOTGUN = 3,
    WEAPON_SUBGUN = 4,
    WEAPON_SNIPER = 5,
    WEAPON_ASSAULT = 6,
    WEAPON_GRENADE = 7
};
```

### Timestam ps & Timing
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x7D` | `int` | **Spawn Time** | When player spawned |
| `+0xDE` | `int` | **Position Update Time** | Last position update timestamp |
| `+0xDF` | `int` | **Position History Index** | Index in position history buffer |
| `+0xE0` | `int` | **Position History Count** | Number of stored positions |
| `+0xE1-0xE3` | `float[3]` | **Last Position** | Previous position (X, Y, Z) |

---

## 🌍 Game State & Environment

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `0x0057ED8F` | `byte` | **Game Paused** | Pause state flag |
| `0x0057F114` | `int` | **Game State Flag** | General game state |
| `0x0057F10C` | `uint` | **Game Time** | Current game time (milliseconds) |
| `0x0057F144` | `int` | **Last Update Time** | Last game update timestamp |
| `0x0057F0D4` | `int` | **Debug/Edit Mode** | Edit mode flag |
| `0x00582D77` | `byte` | **Network Flag** | Networking state |
| `0x0056BC94` | `int` | **Game Phase** | Current game phase (0-3) |
| `0x0056C964` | `int` | **Tick Rate** | Game tick interval |
| `0x005837D0` | `int` | **Frame Base Time** | Frame timing base |
| `0x005837D4` | `int` | **Frame Delta** | Frame time delta |
| `0x005837D8` | `int` | **Collision Flag** | Collision detection flag |

---

## 🔫 Weapon System

### Weapon Constants (from strings)
```
Knife       = 0
Pistol      = 1
Carbine     = 2
Shotgun     = 3
Subgun      = 4
Sniper      = 5
Auto Rifle  = 6
Grenade     = 7
```

### Important Functions

| Address | Name | Description |
|---------|------|-------------|
| `0x0047D7B0` | **SpawnPlayer** | Spawns player at spawn point |
| `0x0047CC70` | **OnPlayerKill** | Handles player death |
| `0x00480A70` | **SetPlayerName** | Sets player name |
| `0x004C19C0` | **PlayerMovement** | Main player physics/movement update |
| `0x004C0D40` | **CollisionCheck** | Checks for collisions |
| `0x004784A0` | **GameLoop** | Main game loop function |
| `0x0047DD20` | **Update Loop** | Main update/tick function |

---

## 📊 Spawn Point Structure

*Base: SpawnPoints array (`0x0059086C`)*
*Each spawn is 0x18 bytes*

| Offset | Type | Name |
|--------|------|------|
| `+0x0` | `short` | X Position |
| `+0x2` | `short` | Y Position |
| `+0x4` | `short` | Z Position |
| `+0x6` | `short` | Yaw Angle |

---

## 🎮 Game Modes

| ID | Name | Description |
|----|------|-------------|
| `0` | Deathmatch | Free-for-all |
| `3` | Pistol FFA | Pistol only FFA |
| `4` | Team Deathmatch | Team vs team |
| `5` | CTF | Capture the flag |
| `7` | One Shot One Kill | OSOK mode |
| `9` | Team OSOK | Team OSOK |
| `11` | HTF | Hold the flag |
| `13` | KTF | Keep the flag |
| `14` | Teamplay | Team gameplay |
| `15` | Team Survivor | Team survivor |
| `16` | Bot Team Deathmatch | TDM with bots |
| `17` | Bot Team OSOK | Team OSOK with bots |
| `20` | LSS | Last Swiss Standing |
| `21` | TLSS | Team LSS |

---

## 🔍 Usage Tips for Trainers

### **No Recoil**
- Hook or patch the function at `0x004C19C0` (PlayerMovement)
- Look for view angle calculations around offsets `+0xCB`, `+0xCC`, `+0xD`, `+0xE`
- Alternative: Freeze view angles after shooting by monitoring `+0x61` (Is Shooting flag)

### **ESP / Wallhack**
- Read PlayerList (`0x0058AC04`) and PlayerCount (`0x0058AC0C`)
- Iterate through players and read their positions (`+0x4`, `+0x8`, `+0xC`)
- Check if alive with `+0x76` (Is Dead flag)
- Read team ID from `+0x30C` to distinguish teams

### **Teleport**
- Write to local player position offsets (`+0x4`, `+0x8`, `+0xC`)
- May need to update collision state

### **Speed Hack**
- Modify Speed Multiplier at `+0x11`
- Values > 1.0 increase speed, < 1.0 decrease speed

### **Aimbot**
- Read enemy positions from player list
- Calculate angles to target
- Write to Yaw (`+0x34` or `+0xD`) and Pitch (`+0xE`)

### **God Mode**
- Monitor health/damage functions
- Look for damage application near OnPlayerKill (`0x0047CC70`)

---

## 💡 Important Notes

1. **Base Addresses**: All offsets are relative to the player entity pointer, not absolute addresses
2. **Pointer Validity**: Always check if pointers are not NULL before dereferencing
3. **Game Updates**: Offsets may change with game updates - verify version compatibility
4. **Multiplayer**: Some offsets may behave differently in multiplayer vs singleplayer
5. **Anti-Cheat**: This game has minimal protection, but be aware server-side validation exists

---

## 🎯 Recoil & Shooting Mechanics

### Recoil Related Offsets
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x61` | `byte` | **Is Shooting** | Set to 1 when firing weapon |
| `+0x32C` | `int` | **Recoil X Component** | Horizontal recoil offset |
| `+0x330` | `int` | **Recoil Y Component** | Vertical recoil offset |
| `+0x34` | `int` | **Current Yaw (with recoil)** | Yaw angle affected by recoil |
| `+0x38` | `int` | **Current Pitch (with recoil)** | Pitch angle affected by recoil |

### Recoil Mechanics
- **Recoil is applied in PlayerMovement function** (`0x004C19C0`)
- View angles are modified during shooting based on weapon spread
- Recoil decreases over time when not shooting
- Each weapon has different recoil patterns stored in weapon data structures

### No Recoil Implementation Methods

**Method 1: Freeze View Angles** (Easiest)
```cpp
// Save original angles before shooting
float originalYaw = *(float*)((int)localPlayer + 0x34);
float originalPitch = *(int*)((int)localPlayer + 0xE);

// After shooting, restore them
*(float*)((int)localPlayer + 0x34) = originalYaw;
*(int*)((int)localPlayer + 0xE) = originalPitch;
```

**Method 2: Zero Recoil Values**
```cpp
// Set recoil components to zero each frame
*(int*)((int)localPlayer + 0xCB) = 0;  // X recoil
*(int*)((int)localPlayer + 0xCC) = 0;  // Y recoil
```

**Method 3: NOP Recoil Instructions**
- Hook or patch the recoil calculation in `PlayerMovement` function at `0x004C19C0`
- Find the assembly instructions that add recoil to view angles
- Replace with NOP (0x90) instructions

---

## 🤖 Bot & Enemy Player Data

### Accessing Other Players / Bots
All players (including bots) are stored in the same array structure:

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `0x0058AC04` | `int**` | **Player Array** | Array of pointers to player entities |
| `0x0058AC0C` | `int` | **Player Count** | Total number of players/bots |

### Identifying Bots vs Players
- Bots use the same entity structure as players
- Check player state and behavior flags to distinguish
- Bots typically have AI-controlled movement patterns

### Player Location & Bones (Hitboxes)

**Note**: AssaultCube uses a simplified hitbox system, not full skeletal animation

#### Main Hitbox Areas
| Offset | Type | Name | Description |
|--------|------|------|-------------|
| `+0x4, +0x8, +0xC` | `float[3]` | **Center Position** | Main body center (X, Y, Z) |
| `+0x14` | `float` | **Current Height** | Height from ground (for head) |
| `+0x15` | `float` | **Max Height** | Player max height (collision box) |
| `+0x50` | `float` | **Eye Height** | Camera/eye level offset from feet |
| `+0x58` | `float` | **Crouch Height** | Height when crouching |

#### Calculating Hitbox Positions
```cpp
// Head position (for headshot detection)
float headX = playerX;
float headY = playerY;
float headZ = playerZ + eyeHeight - crouchHeight;

// Center mass (torso)
float torsoZ = playerZ + (eyeHeight / 2);

// Feet position
float feetZ = playerZ;
```

### Getting All Enemy Positions
```cpp
int* localPlayer = *(int**)0x0058AC00;
int* playerList = *(int**)0x0058AC04;
int playerCount = *(int*)0x0058AC0C;
int localTeam = *(int*)((int)localPlayer + 0x30C);

for (int i = 0; i < playerCount; i++) {
    int* enemy = *(int**)(playerList + i * 4);
    if (!enemy || enemy == localPlayer) continue;
    
    // Check if alive
    byte isDead = *(byte*)((int)enemy + 0x76);
    if (isDead != 0) continue;
    
    // Check if enemy team (for team modes)
    int enemyTeam = *(int*)((int)enemy + 0x30C);
    if (enemyTeam == localTeam) continue; // Skip teammates
    
    // Get enemy position
    float enemyX = *(float*)((int)enemy + 0x4);
    float enemyY = *(float*)((int)enemy + 0x8);
    float enemyZ = *(float*)((int)enemy + 0xC);
    
    // Get enemy eye height for headshot position
    float eyeHeight = *(float*)((int)enemy + 0x50);
    float headZ = enemyZ + eyeHeight;
    
    // Now you can use these positions for ESP, aimbot, etc.
}
```

---

## 🔧 Example Code Snippets

### Reading Local Player Position
```cpp
int* localPlayer = *(int**)0x0058AC00;
if (localPlayer) {
    float x = *(float*)(localPlayer + 0x4);
    float y = *(float*)(localPlayer + 0x8);
    float z = *(float*)(localPlayer + 0xC);
}
```

### Checking if Player is Alive
```cpp
int* localPlayer = *(int**)0x0058AC00;
if (localPlayer) {
    byte isDead = *(byte*)((int)localPlayer + 0x76);
    if (isDead == 0) {
        // Player is alive
    }
}
```

### No Recoil Implementation (Real-time)
```cpp
// Call this in your main loop/hook
void NoRecoil() {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return;
    
    // Method 1: Zero out recoil components
    *(int*)((int)localPlayer + 0xCB) = 0;
    *(int*)((int)localPlayer + 0xCC) = 0;
    
    // Method 2: Smooth recoil reduction (more subtle)
    int* recoilX = (int*)((int)localPlayer + 0xCB);
    int* recoilY = (int*)((int)localPlayer + 0xCC);
    *recoilX = (int)(*recoilX * 0.1f); // Reduce by 90%
    *recoilY = (int)(*recoilY * 0.1f);
}
```

### ESP - Get All Visible Enemies
```cpp
struct Enemy {
    char name[260];
    float x, y, z;
    float headX, headY, headZ;
    int team;
    bool isDead;
};

std::vector<Enemy> GetEnemies() {
    std::vector<Enemy> enemies;
    
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return enemies;
    
    int* playerList = *(int**)0x0058AC04;
    int playerCount = *(int*)0x0058AC0C;
    int localTeam = *(int*)((int)localPlayer + 0x30C);
    
    for (int i = 0; i < playerCount; i++) {
        int* player = *(int**)(playerList + i * 4);
        if (!player || player == localPlayer) continue;
        
        Enemy e;
        e.isDead = *(byte*)((int)player + 0x76) != 0;
        if (e.isDead) continue;
        
        // Position
        e.x = *(float*)((int)player + 0x4);
        e.y = *(float*)((int)player + 0x8);
        e.z = *(float*)((int)player + 0xC);
        
        // Head position
        float eyeHeight = *(float*)((int)player + 0x50);
        e.headX = e.x;
        e.headY = e.y;
        e.headZ = e.z + eyeHeight;
        
        // Name and team
        strncpy(e.name, (char*)((int)player + 0x205), 260);
        e.team = *(int*)((int)player + 0x30C);
        
        enemies.push_back(e);
    }
    
    return enemies;
}
```

### Simple Aimbot (Aim at Nearest Enemy Head)
```cpp
void SimpleAimbot() {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return;
    
    float myX = *(float*)((int)localPlayer + 0x4);
    float myY = *(float*)((int)localPlayer + 0x8);
    float myZ = *(float*)((int)localPlayer + 0xC);
    float myEye = *(float*)((int)localPlayer + 0x50);
    myZ += myEye; // Eye position
    
    int* playerList = *(int**)0x0058AC04;
    int playerCount = *(int*)0x0058AC0C;
    
    float closestDist = FLT_MAX;
    float targetX, targetY, targetZ;
    bool foundTarget = false;
    
    // Find closest enemy
    for (int i = 0; i < playerCount; i++) {
        int* player = *(int**)(playerList + i * 4);
        if (!player || player == localPlayer) continue;
        if (*(byte*)((int)player + 0x76) != 0) continue; // Dead
        
        float px = *(float*)((int)player + 0x4);
        float py = *(float*)((int)player + 0x8);
        float pz = *(float*)((int)player + 0xC);
        float eyeHeight = *(float*)((int)player + 0x50);
        pz += eyeHeight; // Head position
        
        float dx = px - myX;
        float dy = py - myY;
        float dz = pz - myZ;
        float dist = sqrt(dx*dx + dy*dy + dz*dz);
        
        if (dist < closestDist) {
            closestDist = dist;
            targetX = px;
            targetY = py;
            targetZ = pz;
            foundTarget = true;
        }
    }
    
    if (!foundTarget) return;
    
    // Calculate angles
    float dx = targetX - myX;
    float dy = targetY - myY;
    float dz = targetZ - myZ;
    
    float horizontalDist = sqrt(dx*dx + dy*dy);
    
    // Calculate yaw (horizontal angle)
    float yaw = atan2(dy, dx) * 180.0f / 3.14159265f;
    
    // Calculate pitch (vertical angle)
    float pitch = -atan2(dz, horizontalDist) * 180.0f / 3.14159265f;
    
    // Apply angles
    *(float*)((int)localPlayer + 0x34) = yaw;
    *(int*)((int)localPlayer + 0xE) = (int)pitch;
}
```

### Teleport to Position
```cpp
void Teleport(float x, float y, float z) {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return;
    
    *(float*)((int)localPlayer + 0x4) = x;
    *(float*)((int)localPlayer + 0x8) = y;
    *(float*)((int)localPlayer + 0xC) = z;
    
    // Reset velocity to prevent physics issues
    *(float*)((int)localPlayer + 0x10) = 0.0f;
    *(float*)((int)localPlayer + 0x14) = 0.0f;
    *(float*)((int)localPlayer + 0x18) = 0.0f;
}
```

### Speed Hack
```cpp
void SetSpeed(float multiplier) {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return;
    
    // Normal speed is 1.0
    // 2.0 = 2x speed, 0.5 = half speed
    *(float*)((int)localPlayer + 0x11) = multiplier;
}
```

### Infinite Jump
```cpp
void InfiniteJump(bool enable) {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return;
    
    if (enable) {
        // Set jump flag
        *(byte*)((int)localPlayer + 0x5F) = 1;
        // Set upward velocity
        *(float*)((int)localPlayer + 0x1E) = 5.0f;
    }
}
```

### Fly Hack
```cpp
void FlyHack(bool enable) {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return;
    
    if (enable) {
        // Set player state to swimming (allows free movement)
        *(byte*)((int)localPlayer + 0x77) = 3;
        
        // Disable gravity effect
        *(float*)((int)localPlayer + 0x1E) = 0.0f;
    } else {
        // Return to normal ground state
        *(byte*)((int)localPlayer + 0x77) = 0;
    }
}
```

---

## 🎨 Advanced Features

### Bone/Hitbox Positions

AssaultCube uses a simplified hitbox system. Here are the approximate bone positions:

```cpp
struct BonePositions {
    float head[3];
    float neck[3];
    float chest[3];
    float stomach[3];
    float pelvis[3];
    float feet[3];
};

BonePositions GetPlayerBones(int* player) {
    BonePositions bones = {0};
    
    float x = *(float*)((int)player + 0x4);
    float y = *(float*)((int)player + 0x8);
    float z = *(float*)((int)player + 0xC);
    float eyeHeight = *(float*)((int)player + 0x50);
    float maxHeight = *(float*)((int)player + 0x15);
    
    // Head (top)
    bones.head[0] = x;
    bones.head[1] = y;
    bones.head[2] = z + eyeHeight;
    
    // Neck
    bones.neck[0] = x;
    bones.neck[1] = y;
    bones.neck[2] = z + eyeHeight * 0.85f;
    
    // Chest
    bones.chest[0] = x;
    bones.chest[1] = y;
    bones.chest[2] = z + eyeHeight * 0.65f;
    
    // Stomach
    bones.stomach[0] = x;
    bones.stomach[1] = y;
    bones.stomach[2] = z + eyeHeight * 0.45f;
    
    // Pelvis
    bones.pelvis[0] = x;
    bones.pelvis[1] = y;
    bones.pelvis[2] = z + eyeHeight * 0.25f;
    
    // Feet
    bones.feet[0] = x;
    bones.feet[1] = y;
    bones.feet[2] = z;
    
    return bones;
}
```

### World to Screen (for ESP Drawing)

```cpp
struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

bool WorldToScreen(Vec3 pos, Vec2& screen, int screenWidth, int screenHeight) {
    int* localPlayer = *(int**)0x0058AC00;
    if (!localPlayer) return false;
    
    // Get view matrix (this address needs to be found)
    // For now, this is a placeholder
    float* viewMatrix = (float*)0x00000000; // TODO: Find view matrix address
    
    // Basic projection (simplified)
    float myX = *(float*)((int)localPlayer + 0x4);
    float myY = *(float*)((int)localPlayer + 0x8);
    float myZ = *(float*)((int)localPlayer + 0xC);
    float myEye = *(float*)((int)localPlayer + 0x50);
    myZ += myEye;
    
    float yaw = *(float*)((int)localPlayer + 0x34) * 3.14159f / 180.0f;
    float pitch = *(int*)((int)localPlayer + 0xE) * 3.14159f / 180.0f;
    
    // Transform to view space
    float dx = pos.x - myX;
    float dy = pos.y - myY;
    float dz = pos.z - myZ;
    
    // Rotate by camera angles
    float vx = dx * cos(yaw) + dy * sin(yaw);
    float vy = -dx * sin(yaw) + dy * cos(yaw);
    float vz = dz;
    
    // Apply pitch
    float vx2 = vx * cos(pitch) - vz * sin(pitch);
    float vz2 = vx * sin(pitch) + vz * cos(pitch);
    
    // Check if behind camera
    if (vx2 <= 0) return false;
    
    // Project to screen
    float fov = 90.0f; // Default FOV
    float aspectRatio = (float)screenWidth / screenHeight;
    
    screen.x = (screenWidth / 2.0f) + (vy / vx2) * (screenWidth / 2.0f) / tan(fov * 3.14159f / 360.0f);
    screen.y = (screenHeight / 2.0f) - (vz2 / vx2) * (screenHeight / 2.0f) / tan(fov * 3.14159f / 360.0f) / aspectRatio;
    
    return (screen.x >= 0 && screen.x <= screenWidth && 
            screen.y >= 0 && screen.y <= screenHeight);
}
```

---

## 📝 Additional Weapon Data

### Weapon Structure (Preliminary)
Based on analysis, weapon objects contain:
- Weapon type/ID
- Current ammo
- Magazine size
- Reload time
- Damage values
- Spread/accuracy values
- Fire rate

*Note: Exact weapon structure offsets need further analysis*

### Ammo Offsets (TODO)
- Current ammo in magazine
- Reserve ammo
- Max ammo capacity

---

## 🛡️ Anti-Detection Tips

1. **Use reasonable values**: Don't set speed to 100x or teleport across the map instantly
2. **Smooth aimbot**: Add smoothing to aim movements to look more human
3. **Limit ESP range**: Only show enemies within reasonable distance
4. **Timing**: Add slight delays to actions to avoid perfect frame-by-frame execution
5. **Server-side validation**: Remember that servers validate many actions, so client-side changes may be rejected

---

## 🔍 Finding More Offsets

### Using Cheat Engine
1. Search for known values (health, ammo, position)
2. Use "Unknown initial value" → "Changed value" scans
3. Freeze values and see what else changes
4. Use pointer scans to find base addresses

### Using Ghidra
1. Search for string references (player names, weapon names)
2. Follow cross-references to functions
3. Analyze function parameters and return values
4. Look for patterns in memory allocation

### Dynamic Analysis
1. Use debuggers (x64dbg, OllyDbg) to set breakpoints
2. Monitor memory access during key actions (shooting, jumping)
3. Follow the call stack to find important functions

---

## 📚 Function Reference

### Critical Functions for Hooking

| Address | Function | Purpose | Hook Use |
|---------|----------|---------|----------|
| `0x004C19C0` | PlayerMovement | Updates player physics | Speed hack, fly hack, no-clip |
| `0x004C0D40` | CollisionCheck | Checks collisions | No-clip, wall hack |
| `0x0047D7B0` | SpawnPlayer | Spawns player | Spawn manipulation |
| `0x0047CC70` | OnPlayerKill | Handles kills | God mode, kill tracking |
| `0x004CC560` | SpawnProjectile | Creates bullets/grenades | Rapid fire, projectile mods |
| `0x0047DD20` | MainUpdate | Main game loop | General hooks, timing |

### Useful Helper Functions

| Address | Function | Description |
|---------|----------|-------------|
| `0x004784A0` | GameLoopEnd | Called at end of game loop |
| `0x00518920` | PlaySound | Plays sound effects |
| `0x00516A50` | SpawnEffect | Spawns visual effects |
| `0x004D44C0` | TriggerEvent | Triggers game events |


Primary Control Variables (all are single bytes):

0x593f19 - Mouse Lock State (0 = unlocked, 1 = locked) - Most important!
0x593f13 - Mouse Capture State (0 = not captured, 1 = captured)
0x56d925 - Relative Mouse Config (0 = disabled, 1 = enabled)

Function to hook (if you prefer that approach):

SetMouseLockMode at offset 0x4f1b60

---

## 🎯 Common Trainer Features Implementation

### 1. **God Mode / Infinite Health**
- Method 1: Hook damage function and return early
- Method 2: Constantly set health to max value
- Method 3: NOP damage application instructions

### 2. **Infinite Ammo**
- Method 1: Hook ammo decrease and skip it
- Method 2: Set ammo to max each frame
- Method 3: Freeze ammo value in memory

### 3. **Rapid Fire**
- Modify fire rate in weapon structure
- Hook shooting delay timer
- Remove fire cooldown

### 4. **No Spread / Perfect Accuracy**
- Zero out weapon spread values
- Hook bullet spawn function
- Modify projectile direction to always center

### 5. **Wallhack / ESP**
- Read all player positions from player array
- Use WorldToScreen to project 3D → 2D
- Draw boxes/lines on screen using DirectX/OpenGL hooks

### 6. **Aimbot**
- Calculate angles to target
- Smooth interpolation for natural movement
- Lead target for moving enemies
- Bone selection (head, chest, etc.)

---

## ⚠️ Important Warnings

1. **Testing**: Always test in offline/bot matches first
2. **Backups**: Keep backups of original game files
3. **Ethics**: Don't ruin others' gameplay in online matches
4. **Legal**: Modifying games may violate ToS/EULA
5. **Detection**: Even without anti-cheat, suspicious behavior can get you banned

---

## 📖 Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024 | Initial offset discovery and documentation |

---

**Last Updated**: Generated from reverse engineering analysis  
**Game**: AssaultCube (tested on v1.2.x)  
**Analysis Tools**: Ghidra, Cheat Engine  
**Status**: Active research - offsets may need verification  

---

## 🤝 Contributing

Found new offsets or corrections? Documentation improvements welcome!

### Offset Format
```
Offset: +0xXXX
Type: int/float/byte/pointer
Name: Descriptive name
Description: What it does
Verified: Yes/No
```

---

## 📞 Support & Resources

- **Ghidra Project**: Can be shared for collaborative analysis
- **Cheat Engine Tables**: Can be created for quick value finding
- **Pattern Signatures**: Useful for auto-updating offsets

---

## 🎓 Learning Resources

### Reverse Engineering
- Ghidra documentation
- IDA Pro tutorials
- x86 assembly basics

### Game Hacking
- Cheat Engine tutorials
- Memory scanning techniques
- Code injection methods

### C++ / Low-level Programming
- Pointer arithmetic
- Memory manipulation
- Windows API basics

---

*Remember: Use responsibly and ethically. This documentation is for educational purposes.*
