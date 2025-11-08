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


