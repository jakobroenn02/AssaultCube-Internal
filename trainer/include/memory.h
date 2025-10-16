#ifndef MEMORY_H
#define MEMORY_H

#include <Windows.h>
#include <cstdint>
#include <vector>

class Memory {
public:
    // Read memory
    template<typename T>
    static T Read(uintptr_t address) {
        return *(T*)address;
    }
    
    // Write memory
    template<typename T>
    static void Write(uintptr_t address, T value) {
        *(T*)address = value;
    }
    
    // Write memory with protection change
    template<typename T>
    static bool WriteProtected(uintptr_t address, T value) {
        DWORD oldProtect;
        if (VirtualProtect((LPVOID)address, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *(T*)address = value;
            VirtualProtect((LPVOID)address, sizeof(T), oldProtect, &oldProtect);
            return true;
        }
        return false;
    }
    
    // Patch bytes
    static bool Patch(uintptr_t address, const char* bytes, size_t size) {
        DWORD oldProtect;
        if (VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy((void*)address, bytes, size);
            VirtualProtect((LPVOID)address, size, oldProtect, &oldProtect);
            return true;
        }
        return false;
    }
    
    // NOP (No Operation) instruction patch
    static bool Nop(uintptr_t address, size_t size) {
        DWORD oldProtect;
        if (VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memset((void*)address, 0x90, size); // 0x90 = NOP
            VirtualProtect((LPVOID)address, size, oldProtect, &oldProtect);
            return true;
        }
        return false;
    }
    
    // Pattern scanning
    static uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern, const char* mask) {
        size_t patternLength = strlen(mask);
        
        for (size_t i = 0; i < size - patternLength; i++) {
            bool found = true;
            for (size_t j = 0; j < patternLength; j++) {
                if (mask[j] != '?' && pattern[j] != *(char*)(start + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return start + i;
            }
        }
        return 0;
    }
    
    // Get module base and size
    static bool GetModuleInfo(const char* moduleName, uintptr_t& baseAddress, size_t& moduleSize) {
        HMODULE hModule = GetModuleHandleA(moduleName);
        if (!hModule) return false;
        
        MODULEINFO modInfo;
        if (GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) {
            baseAddress = (uintptr_t)modInfo.lpBaseOfDll;
            moduleSize = modInfo.SizeOfImage;
            return true;
        }
        return false;
    }
    
    // Read pointer chain
    static uintptr_t ReadPointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets) {
        uintptr_t address = Read<uintptr_t>(base);
        for (size_t i = 0; i < offsets.size() - 1; i++) {
            address = Read<uintptr_t>(address + offsets[i]);
            if (!address) return 0;
        }
        return address + offsets.back();
    }
};

#endif // MEMORY_H
