# Assault Cube Trainer

A C++ DLL trainer/hook for Assault Cube with memory manipulation features.

## 🎮 Features

- **God Mode** (F1) - Infinite health
- **Infinite Ammo** (F2) - Never run out of ammo
- **No Recoil** (F3) - Remove weapon recoil
- **Add Health** (F4) - Add 1000 health
- **END** - Unload trainer

## 📁 Project Structure

```
trainer/
├── src/
│   ├── dllmain.cpp      # DLL entry point and main thread
│   ├── trainer.cpp      # Trainer implementation
│   ├── pch.h/cpp        # Precompiled headers
├── include/
│   ├── trainer.h        # Trainer class definition
│   ├── memory.h         # Memory manipulation utilities
├── build/               # Build output directory
├── CMakeLists.txt       # CMake configuration
├── build.ps1            # Build script for Windows
└── README.md            # This file
```

## 🔧 Building

### Prerequisites

- CMake (3.15 or higher)
- Visual Studio 2019/2022 with C++ tools
- Windows SDK

### Build Steps

1. Open PowerShell in the `trainer` directory
2. Run the build script:
   ```powershell
   .\build.ps1
   ```

### Manual Build

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

The compiled DLL will be in `build/lib/Release/actrainer.dll`

## 🎯 Usage

1. Build the DLL using the steps above
2. Use the Go loader to inject the DLL into Assault Cube
3. A console window will appear showing the trainer status
4. Use hotkeys to activate features

## 🔍 Finding Addresses

**IMPORTANT:** The current implementation uses placeholder addresses. You need to find the real addresses using Cheat Engine or similar tools.

### Steps to Find Addresses:

1. Open Assault Cube
2. Open Cheat Engine and attach to the game process
3. Search for your health value (e.g., 100)
4. Take damage and search for the new value
5. Repeat until you find the correct address
6. Use "Find out what writes to this address" to locate the code
7. Update `trainer.cpp` with the real addresses

### Common Address Offsets (verify these!):

```cpp
// Player base: Need to find via pattern scan
// Health: Player + 0xF8 (example)
// Armor: Player + 0xFC (example)
// Ammo: Player + 0x150 (example)
```

## 📝 Memory Class API

The `Memory` class in `memory.h` provides utilities for:

- **Read/Write** - Basic memory read/write
- **WriteProtected** - Write with automatic protection change
- **Patch** - Patch bytes at an address
- **Nop** - NOP out instructions
- **FindPattern** - Pattern scanning (signature scanning)
- **ReadPointerChain** - Follow pointer chains

### Example Usage:

```cpp
// Read value
int health = Memory::Read<int>(healthAddress);

// Write value
Memory::Write<int>(healthAddress, 999);

// Patch bytes (e.g., NOP out ammo decrease)
Memory::Nop(ammoDecreaseAddress, 6);

// Pattern scan
uintptr_t found = Memory::FindPattern(moduleBase, size, pattern, mask);
```

## 🛡️ Anti-Cheat Considerations

This is a basic trainer for educational purposes. For games with anti-cheat:

- Remove debug console (set `#define DEBUG_CONSOLE false`)
- Use internal patterns instead of hardcoded offsets
- Implement more stealthy injection methods
- Avoid obvious pattern scans
- Use code caves for hooks

## ⚠️ Disclaimer

This trainer is for **educational purposes only**. Use it:

- On single-player or private servers only
- Where you have permission
- Not to ruin others' gaming experience

## 🔨 TODO

- [ ] Find real player base address using pattern scanning
- [ ] Verify and update health/ammo/armor offsets
- [ ] Implement proper no-recoil hook
- [ ] Add ESP (wallhack) feature
- [ ] Add aimbot feature
- [ ] Implement proper DLL unloading
- [ ] Add GUI overlay (ImGui)
- [ ] Make it more stealthy (remove console, use internal hooks)

## 📚 Learning Resources

- [Guided Hacking](https://guidedhacking.com/) - Game hacking tutorials
- [Cheat Engine](https://www.cheatengine.org/) - Memory scanner
- [x64dbg](https://x64dbg.com/) - Debugger
- [IDA Pro/Ghidra](https://ghidra-sre.org/) - Reverse engineering

## 🤝 Contributing

Feel free to improve the trainer:
- Find and add real addresses
- Add new features
- Improve stealth
- Fix bugs
