# Assault Cube Loader & Trainer Project

Complete game hacking solution with Go-based loader/authentication and C++ DLL trainer.

## 📁 Project Structure

```
tuiapp/
├── main.go                  # Go loader entry point
├── config.go                # Configuration management
├── db.go                    # Database connection
├── appsettings.json         # DB config (gitignored)
│
├── views/                   # Go TUI views
│   ├── menu.go              # Main menu
│   ├── login.go             # User login
│   ├── register.go          # User registration
│   ├── dashboard.go         # Post-login dashboard
│   ├── loadassaultcube.go   # Game detection & injection
│   ├── resetpassword.go     # Password reset
│   └── styles.go            # UI styling
│
├── trainer/                 # C++ DLL trainer
│   ├── src/
│   │   ├── dllmain.cpp      # DLL entry & main thread
│   │   ├── trainer.cpp      # Trainer implementation
│   │   └── pch.h/cpp        # Precompiled headers
│   ├── include/
│   │   ├── trainer.h        # Trainer class
│   │   └── memory.h         # Memory utilities
│   ├── build/               # Build output
│   ├── CMakeLists.txt       # CMake config
│   ├── build.ps1            # Build script
│   └── README.md            # Trainer docs
│
└── tools/                   # Utility tools
    ├── checksumtool.go      # Game checksum calculator
    └── README.md            # Tool docs
```

## 🎯 System Overview

### 1. **Go Loader (TUI Application)**

**Purpose:** User authentication, game detection, and DLL injection

**Features:**
- ✅ User registration with bcrypt password hashing
- ✅ User login with session management
- ✅ MySQL database for user storage
- ✅ Beautiful TUI with Bubble Tea + Lipgloss
- ✅ Game executable detection (searches common paths)
- ✅ SHA256 checksum validation
- ✅ Cross-platform terminal clearing (Windows/Mac/Linux)
- 🔨 TODO: DLL injection into game process

**Tech Stack:**
- Go 1.25
- Bubble Tea (TUI framework)
- Lipgloss (styling)
- Bubbles (UI components)
- MySQL driver
- bcrypt (password hashing)

### 2. **C++ Trainer (DLL)**

**Purpose:** In-game memory manipulation and feature hooks

**Features:**
- ✅ DLL injection framework
- ✅ Memory read/write utilities
- ✅ Pattern scanning capabilities
- ✅ God Mode (F1)
- ✅ Infinite Ammo (F2)
- ✅ No Recoil (F3)
- ✅ Health manipulation (F4)
- ✅ Debug console for development
- 🔨 TODO: Find real game addresses
- 🔨 TODO: Implement hooks
- 🔨 TODO: Add ESP/Aimbot

**Tech Stack:**
- C++17
- CMake build system
- Windows API
- Pattern scanning

## 🚀 Getting Started

### Prerequisites

**For Go Loader:**
- Go 1.25+
- MySQL 8.0+
- Terminal with color support

**For C++ Trainer:**
- CMake 3.15+
- Visual Studio 2019/2022 with C++ tools
- Windows SDK

### Setup Instructions

#### 1. Database Setup

```sql
CREATE DATABASE tuiapp;
USE tuiapp;

CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

#### 2. Configure Loader

```bash
# Copy example config
cp appsettings.example.json appsettings.json

# Edit with your MySQL credentials
notepad appsettings.json
```

#### 3. Build Go Loader

```bash
go build -o tuiapp.exe
```

#### 4. Build C++ Trainer

```powershell
cd trainer
.\build.ps1
```

## 🎮 Usage Workflow

### Step 1: Login
```
1. Run: .\tuiapp.exe
2. Register a new account or login
3. Navigate to Dashboard
```

### Step 2: Load Game
```
1. Select "Load Assault Cube"
2. Press Enter to search for game
3. System validates game version
4. Press 'S' to inject and start
```

### Step 3: Use Trainer
```
In-game hotkeys:
- F1: Toggle God Mode
- F2: Toggle Infinite Ammo  
- F3: Toggle No Recoil
- F4: Add Health
- END: Unload trainer
```

## 🔧 Configuration

### Loader Config (`appsettings.json`)

```json
{
  "database": {
    "host": "192.168.0.2",
    "port": 3306,
    "user": "your_user",
    "password": "your_password",
    "database": "tuiapp"
  },
  "app": {
    "name": "TUI App",
    "version": "1.0.0"
  }
}
```

### Trainer Config (`trainer.cpp`)

Update these placeholder values with real addresses from Cheat Engine:

```cpp
// Find these with Cheat Engine
playerBase = moduleBase + 0xOFFSET;  // Find player pointer
healthAddress = playerBase + 0xF8;   // Health offset
ammoAddress = playerBase + 0x150;    // Ammo offset
```

## 🛠️ Development Tools

### Checksum Tool

Calculate game executable checksums:

```powershell
cd tools
go build -o checksumtool.exe checksumtool.go
checksumtool.exe "C:\path\to\ac_client.exe"
```

### Memory Scanner

Use [Cheat Engine](https://www.cheatengine.org/) to find:
- Player base address
- Health/armor offsets
- Ammo offsets
- Weapon data structures

## 📝 TODO List

### Loader (Go)
- [ ] Implement DLL injection
- [ ] Add process creation with suspended state
- [ ] Handle multiple game versions
- [ ] Add automatic updates
- [ ] Implement HWID authentication
- [ ] Add subscription management

### Trainer (C++)
- [ ] Find real player addresses via pattern scanning
- [ ] Implement proper hooks (ammo, recoil)
- [ ] Add ESP (wallhack)
- [ ] Add aimbot
- [ ] Add GUI overlay (ImGui)
- [ ] Make stealthier (remove console)
- [ ] Add proper DLL unloading

## 🔒 Security Considerations

- Passwords are hashed with bcrypt
- Database credentials stored separately (gitignored)
- Game validation via checksums
- Anti-cheat considerations documented

## ⚠️ Legal Disclaimer

This project is for **educational purposes only**:
- Use only on games you own
- Use in single-player or private servers
- Do not use to harm others' experience
- Respect game developers and communities

## 📚 Resources

- [Guided Hacking](https://guidedhacking.com/)
- [Cheat Engine](https://www.cheatengine.org/)
- [x64dbg](https://x64dbg.com/)
- [Bubble Tea Docs](https://github.com/charmbracelet/bubbletea)
- [CMake Documentation](https://cmake.org/documentation/)

## 🤝 Contributing

Contributions welcome! Areas to help:
- Finding real game addresses
- Improving injection methods
- Adding new trainer features
- UI/UX improvements
- Documentation
