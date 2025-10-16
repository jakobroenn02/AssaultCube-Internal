# Testing Notes - Trainer Debugging

## Current Status ✅
- Game launches successfully
- DLL injects successfully  
- Working directory set correctly (no texture errors)
- Console window appears with trainer

## Issue 1: Features Not Working

### What to Test:
1. Launch the game with trainer
2. Look at the **console window** that appears
3. Check the initialization output:
   ```
   Current values:
     Health: [should show current health, e.g., 100]
     Armor: [should show current armor, e.g., 0]
     Ammo: [should show current ammo, e.g., 20]
   ```

4. Press **F1** for God Mode
   - Console should show:
     ```
     God Mode: ON
       Current Health: X, Armor: Y
       Set Health and Armor to 100
       New Health: 100, Armor: 100
     ```
   - If "New Health" and "New Armor" show 100, the write worked!
   - If they show the old values, the addresses might be wrong

5. Press **F2** for Infinite Ammo
   - Console should show:
     ```
     Infinite Ammo: ON
       Current Ammo: X
       Set Ammo to 100, New Ammo: 100
     ```

### Possible Problems:

#### Problem A: Values Read as 0 or Garbage
**Symptoms**: Console shows `Current Health: 0` or weird numbers like `Current Health: 274627282`

**Cause**: Addresses are wrong for this version of Assault Cube

**Solution**: 
1. Open Cheat Engine
2. Attach to ac_client.exe
3. Search for your current health (start at 100)
4. Damage yourself
5. Search for decreased value
6. Find the real address
7. Calculate offset from player base: `RealAddress - PlayerBase = Offset`
8. Update `addresses.md` with correct offset

#### Problem B: Values Read Correctly But Don't Change In-Game
**Symptoms**: Console shows correct read values, writes show 100, but in-game nothing changes

**Cause**: Writing too fast, or need to write every frame

**Solution**: The `UpdatePlayerData()` function runs every 10ms and should keep values locked. Make sure features stay toggled ON.

#### Problem C: Game Crashes When Pressing Hotkeys
**Symptoms**: Game closes immediately when pressing F1/F2

**Cause**: Writing to wrong memory address or bad pointer

**Solution**: Verify playerBase is valid and not NULL (0x00000000)

## Issue 2: Detect Game Process Close

### Current Behavior:
- Game can close
- Loader still shows "✓ Trainer Active!"
- No way to know game closed

### Solution for Phase 2 (Hybrid):
When we implement the IPC/pipe communication, we can:

**Option A: Polling from Go**
```go
// In dashboard, check if process is still alive
func IsProcessRunning(pid uint32) bool {
    handle, err := syscall.OpenProcess(PROCESS_QUERY_INFORMATION, false, pid)
    if err != nil {
        return false // Process doesn't exist
    }
    defer syscall.CloseHandle(handle)
    
    var exitCode uint32
    syscall.GetExitCodeProcess(handle, &exitCode)
    return exitCode == STILL_ACTIVE // 259
}

// Poll every second
ticker := time.NewTicker(1 * time.Second)
if !IsProcessRunning(gamePID) {
    m.Injected = false
    m.Status = "Game closed"
}
```

**Option B: DLL Notification**
```cpp
// In DllMain DLL_PROCESS_DETACH:
// Send message through pipe before unloading
SendToPipe("GAME_CLOSING");
```

**Option C: Wait for Process Handle**
```go
// Store process handle when launching
hProcess, _ := syscall.OpenProcess(SYNCHRONIZE, false, pid)
go func() {
    syscall.WaitForSingleObject(hProcess, syscall.INFINITE)
    // Process exited - update UI
    sendMessage(ProcessClosedMsg{})
}()
```

### For Now (Phase 1):
- User manually presses ESC to go back
- Simple and works for testing
- We'll add automatic detection in Phase 2

## Testing Checklist

### Basic Functionality:
- [ ] Game launches without texture error
- [ ] Console window appears
- [ ] Console shows valid player base address (not 0x00000000)
- [ ] Console shows reasonable health/armor/ammo values
- [ ] F1 toggles God Mode on
- [ ] F1 again toggles God Mode off
- [ ] F2 toggles Infinite Ammo on
- [ ] END key closes trainer

### Feature Testing (In-Game):
- [ ] Start a match (multiplayer bot or singleplayer)
- [ ] Enable God Mode (F1)
- [ ] Take damage - does health stay at 100?
- [ ] Enable Infinite Ammo (F2)  
- [ ] Fire weapon - does ammo stay full?
- [ ] Press F4 to add health - does it increase?

### Console Output to Check:
```
Initializing trainer...
Player base found at: 0x[SOME_ADDRESS]  <- Not 0x00000000!
Health address: 0x[ADDRESS]
Armor address: 0x[ADDRESS]
Ammo address: 0x[ADDRESS]

Current values:
  Health: [realistic number, 1-100]
  Armor: [realistic number, 0-100]
  Ammo: [realistic number, 0-100]

Trainer initialized successfully!
```

If addresses or values look wrong, that's our clue! 🔍
