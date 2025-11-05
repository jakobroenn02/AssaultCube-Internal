# Assault Cube Checksum Tool

This tool calculates the SHA256 and MD5 checksums of the Assault Cube executable to verify game versions.

## Building the Tool

```powershell
cd tools
go build -o checksumtool.exe checksumtool.go
```

## Usage

```powershell
checksumtool.exe "<path_to_ac_client.exe>"
```

### Example

```powershell
checksumtool.exe "C:\Program Files\AssaultCube\bin_win32\ac_client.exe"
```

## Output

The tool will display:
- File path and size
- SHA256 checksum (used for verification)
- MD5 checksum (for reference)
- Pre-formatted code to add to `loadassaultcube.go`

## Adding a New Version

1. Run the tool on your `ac_client.exe`
2. Copy the SHA256 checksum
3. Open `views/loadassaultcube.go`
4. Add the checksum to the `knownVersions` map:

```go
var knownVersions = map[string]string{
    "1.3.0.2": "your_sha256_checksum_here",
    // Add more versions
}
```

## Purpose

This ensures the loader only works with verified, unmodified versions of Assault Cube, preventing:
- Loading into wrong game versions
- Working with corrupted executables  
- Compatibility issues with modified game files



it still does not work.
The problem is that in FFA mode, team values are meaningless for enemy detection. The game doesn't use teams to identify enemies in FFA - it just considers everyone an enemy!
Use team offset 0x30c instead of whatever you were using
In FFA modes, completely ignore team values and just return true for any valid player that isn't you.,
The team offset is 0x30c (780 in decimal)!
But wait, there's MORE! Look at line 0x47d853
ecx = *(ebx_1 + 0x30c) & 1
In certain game modes, they're using team & 1 - this means they're checking only the least significant bit of the team value!