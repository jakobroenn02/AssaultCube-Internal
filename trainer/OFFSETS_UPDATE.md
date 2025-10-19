# Offsets Update - October 19, 2025

## Updated Offsets

The following offsets have been updated to their new values:

### Base/Static Offsets (from ac_client.exe base)

| Offset Name | Old Value | New Value | Location |
|-------------|-----------|-----------|----------|
| `OFFSET_LOCALPLAYER` | `0x0017E0A8` | `0x17B0B8` | trainer.h + addresses.md |
| `OFFSET_ENTITY_LIST` | `0x18AC04` | `0x187C10` | trainer.h + addresses.md |
| `OFFSET_PLAYER_COUNT` | `0x18AC0C` | `0x18EFE4` | trainer.h + addresses.md |

### New Offsets Added

| Offset Name | Value | Location |
|-------------|-------|----------|
| `OFFSET_VIEW_MATRIX` | `0x17AFE0` | trainer.h + addresses.md |
| `OFFSET_VEC3_ORIGIN` | `0x28` | trainer.h + addresses.md |
| `OFFSET_VEC3_HEAD` | `0x4` | trainer.h + addresses.md |
| `OFFSET_TEAM_ID` | `0x30C` | trainer.h + addresses.md |

### Offsets That Remained Unchanged

These offsets were already correct and didn't need updating:

- `OFFSET_HEALTH` = `0xEC` ✓
- `OFFSET_PLAYER_NAME` = `0x205` ✓
- All ammo offsets (AR, SMG, Sniper, etc.)
- All position offsets (X, Y, Z)
- All camera offsets

## Files Modified

1. **`trainer/include/trainer.h`**
   - Updated static offset constants
   - Added new offset definitions for view matrix and vec3 positions
   - Added team ID offset

2. **`trainer/addresses.md`**
   - Updated documentation with new offset values
   - Added new entries for view matrix, vec3 origin/head, and team ID

## Verification

All offsets have been updated in both:
- The C++ header file (for compilation)
- The markdown documentation (for reference)

## Next Steps

To apply these changes:

1. **Rebuild the trainer DLL:**
   ```bash
   cd trainer
   cmake -B build
   cmake --build build
   ```

2. **Test the updated offsets** by injecting the DLL into the game

3. **Verify each feature** works correctly with the new offsets

## Notes

- The FOV offset was removed as it wasn't in the new offset list
- Vec3 offsets were added for better 3D position handling
- View matrix offset enables better ESP/rendering features
- Team ID offset enables team-based features (e.g., enemy detection)

---

**Date Updated:** October 19, 2025  
**Source:** User-provided offset dump
