# Assault Cube Trainer - Build Status

## ✅ COMPLETED

### Addresses Integration
All addresses from `addresses.md` have been integrated into the trainer:

**Static Addresses:**
- LocalPlayer: `[ac_client.exe + 0x0017E0A8]`
- Entity List: `[ac_client.exe + 0x18AC04]`
- FOV: `[ac_client.exe + 0x18A7CC]`
- PlayerCount: `[ac_client.exe + 0x18AC0C]`

**Player Offsets (from LocalPlayer):**
- Health: `[LocalPlayer + 0xEC]` ✅ **ACTIVE**
- Armor: `[LocalPlayer + 0xF0]` ✅ **ACTIVE**
- Position X/Y/Z: `0x2C, 0x30, 0x28`
- Head Position X/Y/Z: `0x4, 0xC, 0x8`
- Camera X/Y: `0x34, 0x38`

**Weapon Offsets:**
- Assault Rifle Ammo: `0x140` ✅ **ACTIVE**
- SMG Ammo: `0x138`
- Sniper Ammo: `0x13C`
- Shotgun Ammo: `0x134`
- Pistol Ammo: `0x12C`
- Grenade Ammo: `0x144`

**Feature Offsets:**
- Fast Fire AR: `0x164`
- Fast Fire Sniper: `0x160`
- Fast Fire Shotgun: `0x158`
- Auto Shoot: `0x204`
- Player Name: `0x205`

### Build System
- ✅ Compiled successfully with MSVC (Visual Studio 2022)
- ✅ DLL output: `trainer\actrainer.dll` (42KB)
- ✅ 32-bit (x86) build for Assault Cube compatibility

### Implemented Features
1. **God Mode (F1)**: Sets health and armor to 100, maintains them automatically
2. **Infinite Ammo (F2)**: Keeps assault rifle ammo at 100
3. **No Recoil (F3)**: Placeholder - needs implementation
4. **Add Health (F4)**: Adds health to current value
5. **Unload Trainer (END)**: Cleanly exits the trainer

### Code Structure
```
trainer/
├── src/
│   ├── dllmain.cpp     - Entry point, console, hotkey loop
│   ├── trainer.cpp     - Feature implementations with REAL addresses
│   └── pch.cpp         - Precompiled headers
├── include/
│   ├── memory.h        - Memory read/write utilities
│   ├── trainer.h       - Trainer class with all offsets as constants
│   └── pch.h           - Precompiled headers
├── build/              - Compiled output
├── actrainer.dll       - Final DLL ✅
├── addresses.md        - Your Cheat Engine findings
├── build_msvc.ps1      - MSVC build script ✅
└── STATUS.md           - This file
```

## 🔨 NEXT STEPS

### 1. Implement DLL Injection in Go Loader
The Go application needs to inject `actrainer.dll` into `ac_client.exe`:

```go
// In views/loadassaultcube.go
func InjectDLL(processID uint32, dllPath string) error {
    // Use Windows API:
    // - OpenProcess()
    // - VirtualAllocEx()
    // - WriteProcessMemory()
    // - CreateRemoteThread()
}
```

### 2. Test Basic Features
1. Run Assault Cube
2. Inject the DLL
3. Verify console window appears
4. Test hotkeys:
   - **F1**: God mode - health/armor should stay at 100
   - **F2**: Infinite ammo - AR ammo should stay at 100
   - **F4**: Add health
   - **END**: Unload trainer

### 3. Additional Features to Implement

#### A. No Recoil (F3)
Currently just a placeholder. Options:
- Patch recoil calculation code
- Set recoil multiplier to 0
- NOP out recoil instructions

#### B. Rapid Fire
Use the fast fire offsets:
```cpp
Write<bool>(playerBase + 0x164, true); // AR fast fire
Write<bool>(playerBase + 0x160, true); // Sniper fast fire
Write<bool>(playerBase + 0x158, true); // Shotgun fast fire
```

#### C. Infinite Ammo for All Weapons
Expand to include:
```cpp
Write<int>(playerBase + 0x138, 100); // SMG
Write<int>(playerBase + 0x13C, 100); // Sniper
Write<int>(playerBase + 0x134, 100); // Shotgun
Write<int>(playerBase + 0x12C, 100); // Pistol
Write<int>(playerBase + 0x144, 100); // Grenades
```

#### D. ESP (Wallhack)
Would require:
- Reading Entity List at `moduleBase + 0x18AC04`
- Reading PlayerCount at `moduleBase + 0x18AC0C`
- Iterating through entities
- Reading positions and rendering overlays

#### E. Teleport
Using position offsets:
```cpp
Write<float>(playerBase + 0x2C, newX); // Position X
Write<float>(playerBase + 0x30, newY); // Position Y
Write<float>(playerBase + 0x28, newZ); // Position Z
```

### 4. Testing Checklist
- [ ] DLL injection works
- [ ] Console appears with trainer info
- [ ] F1 (God mode) keeps health/armor at 100
- [ ] F2 (Infinite ammo) keeps AR ammo at 100
- [ ] F4 (Add health) increases health
- [ ] END key unloads trainer cleanly
- [ ] No crashes or freezes
- [ ] Addresses remain valid after game restart

## 📝 NOTES

### Building
Use the MSVC build script:
```powershell
cd trainer
.\build_msvc.ps1
```

### Debugging
The trainer creates a debug console window showing:
- Module base address
- Player base address
- Health/Armor/Ammo addresses
- Feature toggle messages
- Current values

### Address Validation
If addresses don't work after a game update:
1. Open Assault Cube in Cheat Engine
2. Search for health value (starts at 100)
3. Damage yourself and search for decreased value
4. Find the address offset from player base
5. Update `addresses.md` and rebuild

## 🚀 READY FOR TESTING!

The trainer is compiled and ready. Next step is to:
1. Implement injection in the Go loader
2. Test the trainer in-game
3. Expand features as needed
