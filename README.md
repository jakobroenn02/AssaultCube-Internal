# TUI App - Refactored Structure

A Terminal User Interface (TUI) application built with [Bubble Tea](https://github.com/charmbracelet/bubbletea) and MySQL.

## Project Structure

```
go_project/
├── main.go           # Application entry point and orchestration
├── db.go             # Database connection and initialization
├── views/            # View components package
│   ├── types.go      # Common types and interfaces
│   ├── menu.go       # Main menu view
│   ├── login.go      # Login view
│   └── register.go   # Registration view
├── go.mod            # Go module dependencies
└── go.sum            # Go module checksums
```

## Architecture Overview

### Main Application (`main.go`)
- **Purpose**: High-level orchestration and initialization
- **Responsibilities**:
  - Database initialization
  - Terminal setup
  - View state management
  - Routing between different views
  - Global keyboard shortcuts (quit, back to menu)

### Database Layer (`db.go`)
- **Purpose**: Database connection management
- **Responsibilities**:
  - Initialize MySQL connection
  - Expose global `DB` instance
  - Handle connection cleanup

### Views Package (`views/`)
Each view is a self-contained component with its own:
- **Model**: State management for the view
- **Update**: Input handling and business logic
- **View**: Rendering logic
- **Reset**: State cleanup when switching views

#### `types.go`
- Defines `ViewType` enum (Menu, Login, Register)
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

- **Database**: tuiapp
- **User**: root
- **Password**: 7155
- **Host**: 127.0.0.1:3306

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
- [go-sql-driver/mysql](https://github.com/go-sql-driver/mysql) - MySQL driver
- [bcrypt](https://pkg.go.dev/golang.org/x/crypto/bcrypt) - Password hashing
