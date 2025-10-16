# Assault Cube Trainer

A complete game trainer for Assault Cube with TUI (Terminal User Interface) loader and C++ DLL injection.

## 🚀 Quick Start

### Run the Trainer
```powershell
.\run.ps1
```
This will automatically build if needed and launch the trainer.

### Build Manually
```powershell
# Build everything
.\build.ps1

# Build only Go loader
.\build.ps1 -SkipCpp

# Build only C++ trainer
.\build.ps1 -SkipGo

# Clean and rebuild
.\build.ps1 -Clean
```

## 🎮 Features

### Current Features (Phase 1)
- ✅ User authentication (MySQL database)
- ✅ TUI-based loader with Bubble Tea framework
- ✅ Automatic game detection and validation
- ✅ DLL injection into game process
- ✅ In-game hotkeys:
  - **F1** - God Mode (lock health/armor at 100)
  - **F2** - Infinite Ammo
  - **F3** - No Recoil (placeholder)
  - **F4** - Add Health
  - **END** - Unload Trainer

### Coming Soon (Phase 2)
- 🔨 Named pipe communication (Go ↔ C++)
- 🔨 Real-time stats in dashboard (health, ammo, armor)
- 🔨 Control features from TUI (not just hotkeys)
- 🔨 Auto-detect when game closes
- 🔨 Silent injection (no message box)

## 📁 Project Structure

```
tuiapp/
├── main.go                 # Entry point
├── config.go               # Configuration management
├── db.go                   # Database connection
├── build.ps1               # Build script (builds everything)
├── run.ps1                 # Quick run script
├── go-project-x86.exe      # 32-bit loader (generated)
├── appsettings.json        # Configuration file (gitignored)
│
├── views/                  # TUI views
│   ├── menu.go
│   ├── login.go
│   ├── register.go
│   ├── dashboard.go
│   ├── loadassaultcube.go
│   ├── resetpassword.go
│   ├── styles.go
│   └── types.go
│
├── injection/              # DLL injection system
│   ├── injection.go        # Core injection logic
│   └── launcher.go         # Game launcher
│
└── trainer/                # C++ Trainer DLL
    ├── src/
    │   ├── dllmain.cpp     # DLL entry point
    │   ├── trainer.cpp     # Trainer implementation
    │   └── pch.cpp
    ├── include/
    │   ├── memory.h        # Memory manipulation utilities
    │   ├── trainer.h       # Trainer class
    │   └── pch.h
    ├── addresses.md        # Memory addresses from Cheat Engine
    ├── build_msvc.ps1      # C++ build script
    └── actrainer.dll       # Trainer DLL (generated)
```

## 🔧 Requirements

- **Go 1.25+** (for building the loader)
- **Visual Studio 2019/2022** with C++ support (for building the trainer DLL)
- **MySQL 8.0+** (for user authentication)
- **Assault Cube 1.3.0.2** (the game)

## ⚙️ Configuration

Create `appsettings.json` in the root directory:

```json
{
  "database": {
    "host": "192.168.0.2",
    "port": 3306,
    "user": "your_user",
    "password": "your_password",
    "database": "your_database"
  }
}
```

## 🎯 Usage

1. **Build the project:**
   ```powershell
   .\build.ps1
   ```

2. **Run the loader:**
   ```powershell
   .\go-project-x86.exe
   ```

3. **Login or Register** an account

4. **Select "Load Assault Cube"** from the dashboard

5. **Press Enter** to search for the game

6. **Press S** to start the game with trainer

7. **In-game**, use the hotkeys to enable features

## 📝 Notes

- **Architecture:** The loader MUST be 32-bit (`go-project-x86.exe`) to inject into the 32-bit Assault Cube game
- **DLL Dependencies:** The trainer DLL is statically linked (`/MT`) to avoid runtime dependencies
- **Database:** User passwords are hashed with bcrypt for security
- **Addresses:** Memory addresses are from Assault Cube 1.3.0.2 - update `trainer/addresses.md` if using a different version

## 🐛 Troubleshooting

### "Injection failed" error
- Make sure you're using `go-project-x86.exe` (32-bit), not `go-project.exe` (64-bit)
- Try running as Administrator
- Check antivirus isn't blocking the DLL

### "Game not found" error
- Verify Assault Cube is installed at one of the common locations
- Check `views/loadassaultcube.go` and add your installation path to `searchPaths`

### Features don't work in-game
- Open the trainer console window (appears when DLL loads)
- Check if addresses are reading realistic values (health 1-100, not garbage)
- Use Cheat Engine to verify addresses match `trainer/addresses.md`

### Console window doesn't appear
- DLL might not be injecting - check for error messages in the TUI
- Verify `trainer\actrainer.dll` exists and was built successfully

## 🤝 Development Workflow

```powershell
# Make changes to Go code or C++ code

# Rebuild and run in one command
.\run.ps1

# Or build separately
.\build.ps1

# Test
.\go-project-x86.exe
```

## 📄 License

This project is for educational purposes only.

---

**Built with:** Go, Bubble Tea, Lipgloss, C++17, Windows API
- `ViewTransition` for navigation between views
- Helper functions for view transitions

#### `menu.go`
- Main menu with navigation
- Handles selection between Login and Register
- Clean, focused responsibility

#### `register.go`
- Complete registration form with validation
- Password hashing with bcrypt
- Database insertion
- Error handling and success states

#### `login.go`
- Login form (TODO: implementation)
- Will handle authentication

## Benefits of This Structure

1. **Separation of Concerns**: Each file has a clear, single responsibility
2. **Testability**: Views can be tested independently
3. **Maintainability**: Easy to find and modify specific features
4. **Extensibility**: Adding new views is straightforward
5. **Reusability**: Views can be reused or composed
6. **Clean Main**: `main.go` focuses on initialization and orchestration

## Database Schema

```sql
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Configuration

The application uses a JSON configuration file for settings including database credentials.

### Setup Configuration

1. Copy the example configuration file:
   ```bash
   cp appsettings.example.json appsettings.json
   ```

2. Edit `appsettings.json` with your database credentials:
   ```json
   {
     "database": {
       "host": "localhost",
       "port": 3306,
       "user": "root",
       "password": "your_password_here",
       "database": "tuiapp"
     },
     "app": {
       "name": "TUI App",
       "version": "1.0.0"
     }
   }
   ```

**Note**: The `appsettings.json` file is excluded from version control to protect sensitive credentials. Always use `appsettings.example.json` as a template.

## Running the Application

```bash
# Build
go build

# Run
./go-project
```

## Navigation

- **Arrow Keys / j/k**: Navigate menu items and form fields
- **Tab**: Move between form fields
- **Enter**: Select menu item or submit form
- **Esc**: Return to main menu
- **q / Ctrl+C**: Quit application

## Adding New Views

To add a new view:

1. Create a new file in `views/` (e.g., `views/profile.go`)
2. Define your model struct with state
3. Implement `Update()`, `View()`, and `Reset()` methods
4. Add your view type to `types.go`
5. Update `main.go` to handle the new view in:
   - `model` struct (add your view's model)
   - `initialModel()` (initialize your view)
   - `Update()` (route to your view's update)
   - `View()` (route to your view's render)

## Dependencies

- [Bubble Tea](https://github.com/charmbracelet/bubbletea) - TUI framework
- [Lipgloss](https://github.com/charmbracelet/lipgloss) - Terminal styling
- [Bubbles](https://github.com/charmbracelet/bubbles) - TUI components (text input, etc.)
- [go-sql-driver/mysql](https://github.com/go-sql-driver/mysql) - MySQL driver
- [bcrypt](https://pkg.go.dev/golang.org/x/crypto/bcrypt) - Password hashing
