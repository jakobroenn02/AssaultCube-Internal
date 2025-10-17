# Trainer Overlay Implementation Plan

## Objectives & Constraints
- Replace the current F1–F4 hotkey workflow in `Trainer::Run` with an in-game overlay that lets players toggle features directly, while preserving the existing feature state flags (`godMode`, `infiniteAmmo`, `noRecoil`).【F:trainer/src/trainer.cpp†L62-L157】
- Ensure the overlay does not invoke external console commands (e.g., the current `system("cls")` call) and keep all trainer UI interactions inside the game client.【F:trainer/src/trainer.cpp†L233-L245】
- Close the Bubble Tea launcher once the game is running and the DLL has been injected successfully, so only the game and overlay remain active.【F:views/loadassaultcube.go†L292-L321】【F:main.go†L68-L171】
- Improve weak or broken features (repair no-recoil logic and replace the one-shot Add Health action with a more useful mechanic) while keeping God Mode and Infinite Ammo stable.【F:trainer/src/trainer.cpp†L129-L230】

## High-level Architecture
1. **Overlay lifecycle**
   - Initialize an expanded `UIRenderer` that can render interactive controls and respond to cursor events after the DLL is injected. The overlay continues to refresh while `Trainer::Run` loops, rendering the current feature states and player stats.【F:trainer/src/trainer.cpp†L62-L91】【F:trainer/src/ui.cpp†L26-L196】
   - Register a custom window procedure (via `SetWindowLongPtr` or `SetWindowsHookEx`) from `MainThread` in `dllmain.cpp` to capture mouse/keyboard events destined for the overlay, without relying on external executables.【F:trainer/src/dllmain.cpp†L1-L64】

2. **Event flow**
   - Convert overlay clicks into trainer actions by adding an input-processing step inside `Trainer::Run` (e.g., `ProcessOverlayInput()`), which updates feature flags and triggers any one-time patches or memory writes. This replaces the `GetAsyncKeyState` polling.【F:trainer/src/trainer.cpp†L62-L91】
   - Maintain a thread-safe queue or atomic flags (e.g., `std::atomic<bool> godModeRequested`) for UI-triggered state changes so the render and logic threads remain decoupled.

3. **Rendering updates**
   - Extend `UIRenderer::RenderFeatureStatus` to draw toggle widgets (checkboxes or buttons) and capture their screen rectangles for hit-testing. Introduce helper structures (e.g., `struct FeatureToggle { RECT bounds; std::function<void()> onToggle; };`) managed by the trainer loop.【F:trainer/src/ui.cpp†L97-L171】
   - Add a small settings footer to replace the hotkey legend, showcasing overlay usage and an "Unload Trainer" control that mirrors the `VK_END` behavior.【F:trainer/src/ui.cpp†L152-L171】

## Detailed Task Breakdown

### 1. Overlay input handling
- **Expose overlay controls**: Update `UIRenderer` to return the bounding rectangles for rendered toggles, enabling the trainer to determine which control was clicked.【F:trainer/src/ui.cpp†L97-L149】
- **Hook mouse input**: In `MainThread` (`dllmain.cpp`), register a window procedure wrapper that intercepts `WM_LBUTTONDOWN` events, translates cursor coordinates into overlay space, and stores pending toggle actions in a thread-safe structure that `Trainer::Run` can consume.【F:trainer/src/dllmain.cpp†L1-L64】
- **Process actions**: Replace the `GetAsyncKeyState` block in `Trainer::Run` with logic that drains queued overlay actions, calling `ToggleGodMode`, `ToggleInfiniteAmmo`, the new recoil patch routine, or a revised health mechanic.【F:trainer/src/trainer.cpp†L62-L157】

### 2. Feature state management improvements
- **Persistent recoil patch**: Implement `ToggleNoRecoil` by locating the recoil instruction sequence once during `Initialize` (use `Memory::FindPattern` to identify the recoil code) and storing the original bytes for restoration. On activation, apply a NOP patch with `Memory::Nop`, and on deactivation restore the cached bytes using `Memory::Patch`.【F:trainer/src/trainer.cpp†L148-L157】【F:trainer/include/memory.h†L21-L78】
- **Replace Add Health**: Retire `AddHealth` as a direct UI action and introduce a "Regenerative Health" toggle that periodically enforces minimum health/armor values (e.g., 75 HP) within `UpdatePlayerData`. Let players trigger an instant refill via an overlay button if desired, but keep it optional since God Mode already pins values at 100.【F:trainer/src/trainer.cpp†L159-L230】
- **Modular feature descriptors**: Create a `std::vector<FeatureDescriptor>` holding metadata (name, description, toggle function, status provider) so the overlay renderer and logic can iterate over features generically. This removes the need for hard-coded sections in `RenderFeatureStatus` and simplifies adding new cheats later.【F:trainer/src/trainer.cpp†L62-L91】【F:trainer/src/ui.cpp†L97-L149】

### 3. Player stat refresh pipeline
- **Shared data snapshot**: Introduce a lightweight struct (e.g., `TrainerStateSnapshot`) updated once per loop to hand the latest health/armor/ammo values to `UIRenderer::Render`, reducing redundant memory reads inside the renderer.【F:trainer/src/trainer.cpp†L37-L91】【F:trainer/src/ui.cpp†L173-L196】
- **Frame scheduling**: Control the overlay refresh rate using a dedicated timer (e.g., `std::chrono::steady_clock`) to avoid tying render speed strictly to the trainer logic sleep interval. This keeps the panel responsive even if future features introduce longer operations.【F:trainer/src/trainer.cpp†L84-L91】

### 4. Launcher/TUI shutdown flow
- **Emit quit command**: Modify `LaunchAndInject` to return a Bubble Tea command when injection succeeds (e.g., wrap `tea.Quit` in the `ViewTransition`) so the main model can exit the program gracefully instead of continuing to render the launcher. Update the `LoadAssaultCubeModel.Update` switch to forward that command.【F:views/loadassaultcube.go†L60-L321】【F:main.go†L68-L171】
- **Signal injection status**: After calling `injection.LaunchAndInject`, set a flag on the model (already `Injected`) and trigger the quit command only when both `Launching` is false and `Injected` is true, ensuring the launcher window disappears once the overlay is active.【F:views/loadassaultcube.go†L292-L321】
- **Remove redundant console clears**: Since the launcher will terminate automatically, eliminate `clearScreen()` and any `system("cls")` usage tied to the trainer console to comply with the "no external cmd" constraint.【F:main.go†L15-L35】【F:trainer/src/trainer.cpp†L233-L245】

### 5. Additional enhancements & suggestions
- **Feature presets per user**: Persist preferred feature states in the database so the overlay can auto-enable favorites on launch, using the existing authenticated context (`currentUser`) from the Bubble Tea model.【F:main.go†L37-L171】
- **Version-aware patches**: Store recoil patch metadata and other offsets alongside the checksum map, ensuring the overlay only exposes compatible features for the detected AssaultCube build.【F:views/loadassaultcube.go†L18-L253】
- **Overlay theming**: Add configuration-driven color schemes to `UIRenderer` so the panel can adapt to night/day modes without recompiling, leveraging the existing brush creation logic.【F:trainer/src/ui.cpp†L38-L171】

## Testing Checklist
1. **Overlay interaction**: Validate that toggling features through the overlay updates trainer state immediately and persists across level loads.
2. **Memory patch safety**: Confirm recoil patches restore original bytes on feature disable and upon trainer shutdown to prevent crash-on-exit scenarios.
3. **Launcher shutdown**: Ensure the Bubble Tea process terminates automatically after successful injection and remains running if injection fails, allowing retries.
4. **Regression guard**: Verify God Mode and Infinite Ammo still operate correctly and that the regenerative health toggle does not conflict with God Mode when both are enabled.
