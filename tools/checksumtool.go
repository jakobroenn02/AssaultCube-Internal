package main

import (
	"crypto/md5"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"os"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("=== Assault Cube Checksum Tool ===")
		fmt.Println("\nUsage: checksumtool.exe <path_to_ac_client.exe>")
		fmt.Println("\nExample:")
		fmt.Println(`  checksumtool.exe "C:\Program Files\AssaultCube\bin_win32\ac_client.exe"`)
		os.Exit(1)
	}

	filePath := os.Args[1]

	// Check if file exists
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		fmt.Printf("Error: File not found: %s\n", filePath)
		os.Exit(1)
	}

	// Calculate SHA256
	sha256Sum, err := calculateSHA256(filePath)
	if err != nil {
		fmt.Printf("Error calculating SHA256: %v\n", err)
		os.Exit(1)
	}

	// Calculate MD5
	md5Sum, err := calculateMD5(filePath)
	if err != nil {
		fmt.Printf("Error calculating MD5: %v\n", err)
		os.Exit(1)
	}

	// Get file size
	fileInfo, err := os.Stat(filePath)
	if err != nil {
		fmt.Printf("Error getting file info: %v\n", err)
		os.Exit(1)
	}

	fmt.Println("\n=== Assault Cube Executable Checksum ===")
	fmt.Printf("File: %s\n", filePath)
	fmt.Printf("Size: %d bytes (%.2f MB)\n", fileInfo.Size(), float64(fileInfo.Size())/(1024*1024))
	fmt.Printf("\nSHA256: %s\n", sha256Sum)
	fmt.Printf("MD5:    %s\n", md5Sum)
	fmt.Println("\n=== Add to loadassaultcube.go ===")
	fmt.Println("Update the knownVersions map:")
	fmt.Printf(`"X.X.X.X": "%s",  // Replace X.X.X.X with actual version`+"\n", sha256Sum)
}

func calculateSHA256(filepath string) (string, error) {
	file, err := os.Open(filepath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	hash := sha256.New()
	if _, err := io.Copy(hash, file); err != nil {
		return "", err
	}

	return hex.EncodeToString(hash.Sum(nil)), nil
}

func calculateMD5(filepath string) (string, error) {
	file, err := os.Open(filepath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	hash := md5.New()
	if _, err := io.Copy(hash, file); err != nil {
		return "", err
	}

	return hex.EncodeToString(hash.Sum(nil)), nil
}
