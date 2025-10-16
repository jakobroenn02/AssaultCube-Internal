# Phase 2 Testing Guide

## Build Status: ✅ READY

Both components have been built successfully:
- ✅ Go Loader (32-bit): `go-project-x86.exe` (6.43 MB)
- ✅ C++ Trainer DLL: `trainer/actrainer.dll` (249.5 KB)

## What's Been Implemented

### Security Improvements ✅
- [x] No MessageBox in release builds (only in DEBUG mode)
- [x] No `system()` calls - replaced with Windows API in both Go and C++
- [x] Smart process launch with `WaitForInputIdle` instead of hardcoded sleep

### IPC Infrastructure ✅
- [x] Named pipe protocol (JSON messages)
- [x] C++ pipe server in trainer
- [x] Go pipe client in loader
- [x] Real-time status updates
- [x] Log message forwarding
- [x] Error reporting

### TUI Enhancements ✅
- [x] Real-time stats display (Health, Armor, Ammo)
- [x] Feature status indicators (God Mode, Infinite Ammo, No Recoil)
- [x] Recent activity log (last 5 messages)
- [x] Connection status indicator

## Testing Procedure

### Step 1: Launch the Loader
```powershell
.\go-project-x86.exe
```

**Expected**: 
- Console clears using Windows API (no cmd.exe spawned)
- TUI appears with login screen
- No errors

### Step 2: Login/Register
1. Create an account or login
2. Navigate to "Load Assault Cube"

**Expected**:
- Smooth navigation
- No crashes

### Step 3: Search for Game
1. Press `Enter` to search for Assault Cube
2. Wait for validation

**Expected**:
- Game path found
- Version detected
- DLL path confirmed

### Step 4: Launch Game with Trainer
1. Press `S` to start game with trainer
2. Wait for game to launch

**Expected**:
- Game launches from correct working directory
- No texture errors
- `WaitForInputIdle` completes quickly (~1-2 seconds)
- Injection succeeds

### Step 5: Check IPC Connection

**In the TUI**, you should see:
- ✅ "Connected to Trainer" message appears
- ✅ "Trainer v1.0.0 initialized" in logs
- ✅ Real-time stats start updating:
  - Health: (current value)
  - Armor: (current value)
  - Ammo: (current value)
- ✅ All feature states show "OFF" initially

**In the Game Console Window**:
- Console window appears (debug mode)
- Shows trainer initialization
- Shows memory addresses
- Shows current stat values

### Step 6: Test Feature Toggles

**Press F1 in-game** (God Mode):

**Expected in TUI**:
- "God Mode enabled" appears in Recent Activity
- "God Mode: ON" shows in Features section
- Health/Armor values should lock at 100

**Expected in Console**:
- "God Mode: ON" message
- Before/after values printed

**Press F2 in-game** (Infinite Ammo):

**Expected in TUI**:
- "Infinite Ammo enabled" appears in logs
- "Infinite Ammo: ON" shows in Features section
- Ammo should stay at 100

**Expected in Console**:
- "Infinite Ammo: ON" message

### Step 7: Monitor Real-Time Updates

**While playing**:
- Stats should update every ~1 second in TUI
- Logs should appear in real-time when you toggle features
- Connection status should remain "Connected"

### Step 8: Graceful Shutdown

**Press END in-game**:

**Expected**:
- "Trainer shutting down..." appears in logs
- Game continues running
- Console window closes
- Trainer unloads

## Known Issues to Test For

### ❌ If TUI shows "Waiting for trainer connection..."
**Problem**: Pipe not connecting  
**Check**:
1. Is the console window visible? (DLL loaded)
2. Check trainer console for "Named pipe created" message
3. Look for "Failed to create named pipe" errors

**Debug**:
- Antivirus might be blocking named pipes
- Windows firewall might need exception
- Run as Administrator

### ❌ If Stats Don't Update
**Problem**: Pipe connected but no messages  
**Check**:
1. Are you in an actual match? (not menu)
2. Is playerBase address valid? (check console)
3. Are the feature toggles working? (check logs appear)

**Debug**:
- Check if addresses.md offsets match your AC version
- Use Cheat Engine to verify memory addresses
- Check trainer console for error messages

### ❌ If Game Crashes on Injection
**Problem**: DLL compatibility issue  
**Check**:
1. Is game 32-bit? (should be for AC 1.3.0.2)
2. Is loader 32-bit? (check go-project-x86.exe)
3. Is DLL 32-bit? (check build output)

**Debug**:
- Rebuild with `.\build.ps1 -Clean`
- Check for MSVC runtime dependencies
- Verify `/MT` flag is used (static linking)

### ❌ If Injection Fails
**Problem**: Architecture mismatch or permissions  
**Check**:
1. Error message in TUI
2. GetExitCodeThread shows 0? (LoadLibrary failed)
3. Running as Administrator?

**Debug**:
- Check if antivirus quarantined the DLL
- Verify DLL path is correct
- Try running both as Admin

## Success Criteria

- [ ] Loader starts and clears console properly
- [ ] Game launches without texture errors
- [ ] DLL injects successfully
- [ ] TUI shows "Connected to Trainer"
- [ ] Real-time stats appear and update
- [ ] F1 toggle appears in logs
- [ ] F2 toggle appears in logs
- [ ] Stats reflect feature states (god mode locks at 100)
- [ ] No crashes or freezes
- [ ] Graceful shutdown works

## Performance Metrics

Track these for optimization:

| Metric | Target | Current |
|--------|--------|---------|
| Loader startup | < 2s | ? |
| Game launch | < 10s | ? |
| Injection time | < 5s | ? |
| Pipe connection | < 2s | ? |
| Status update rate | ~1s | 1s |
| Memory usage (loader) | < 50MB | ? |
| Memory usage (DLL) | < 10MB | ? |

## Next Steps After Testing

### If Everything Works ✅
1. Document any findings
2. Optimize status update rate if needed
3. Add more features (command support from TUI)
4. Implement process monitoring (detect game close)
5. Add more hotkeys/features

### If Issues Found ❌
1. Document exact error messages
2. Check console logs
3. Review addresses.md offsets
4. Test with Cheat Engine
5. Debug with MessageBox (add `/D_DEBUG` to build)

## Debug Mode

To enable debug MessageBox:

**Edit `trainer/build_msvc.ps1`**:
```powershell
# Change this line:
cl.exe /LD /MT /std:c++17 /EHsc /I..\include ...

# To this:
cl.exe /LD /MT /D_DEBUG /std:c++17 /EHsc /I..\include ...
```

Then rebuild:
```powershell
.\build.ps1
```

You'll see a MessageBox when DLL loads (useful for confirming injection).

## Logs Location

Check these files for debugging:
- **Trainer Log**: `%TEMP%\trainer_debug.txt`
- **Go Loader**: Terminal output
- **Windows Event Log**: Application logs

## Contact

If you encounter issues not covered here:
1. Check `PHASE2_IMPLEMENTATION.md` for architecture details
2. Check `SECURITY_IMPROVEMENTS.md` for security specifics
3. Review console output carefully
4. Enable debug mode and test again

---

**Status**: Ready for Testing!  
**Build Date**: 2025-10-16  
**Version**: Phase 2 (IPC Bridge)  
**Components**: All Built ✅
