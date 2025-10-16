# Trainer Implementation Readiness Analysis

This document summarizes the current state of the project with respect to building a functional AssaultCube trainer and outlines the gaps that must be closed to ship a working feature.

## Existing Capabilities

- **Authentication Workflow** – User registration, login, logout, and password reset flows are already wired up against the `users` table, including password hashing and validation logic. These screens provide a secure gating mechanism around the trainer features once implemented.【F:views/register.go†L1-L120】【F:views/login.go†L1-L139】【F:views/resetpassword.go†L1-L153】
- **Post-login Navigation** – The dashboard view exposes entry points for "Load Assault Cube", "Reset Password", and "Logout", which confirms the trainer will live behind the authenticated dashboard menu.【F:views/dashboard.go†L1-L76】
- **Game Discovery UI** – The "Load Assault Cube" view already contains the UX for locating the `ac_client.exe` binary, displaying status, and gating the launch step behind checksum validation. The view can render success, error, and helper states, and supports refreshing the search on demand.【F:views/loadassaultcube.go†L1-L196】

## Critical Gaps to Address

1. **Reliable Version Fingerprinting**  
   The placeholder `knownVersions` map uses dummy checksums. Populate it with verified SHA-256 hashes per supported AssaultCube build, and consider shipping tooling (or developer docs) for capturing new checksums so the map stays current.【F:views/loadassaultcube.go†L14-L31】

2. **Executable Discovery Beyond Hard-coded Paths**  
   The search routine only scans a short, Windows-only path list. Supporting common Steam directories, configurable install paths, and non-Windows environments will make the trainer viable for a broader audience. Reading candidate locations from configuration or environment variables would improve flexibility.【F:views/loadassaultcube.go†L118-L144】

3. **Executable Integrity & Version Reporting**  
   When a checksum mismatch occurs, the UI currently marks the version "Unverified" but still leaks a truncated hash. Replace the placeholder messaging with actionable guidance (e.g., "download the supported installer"), and decide whether to allow unverified binaries in development vs. production builds.【F:views/loadassaultcube.go†L167-L188】

4. **Game Launch & Trainer Injection Pipeline**  
   Launching the game and injecting trainer code is currently represented by a TODO. Implement a robust pipeline that:
   - Spawns the validated `ac_client.exe` process.
   - Injects the trainer payload (likely a DLL) using a reliable technique such as CreateRemoteThread or manual mapping.
   - Verifies the payload loads successfully and surfaces status or error information back to the TUI.【F:views/loadassaultcube.go†L66-L86】

5. **Trainer Payload Delivery**  
   Decide how the trainer payload is packaged and versioned. Options include bundling a compiled DLL within the binary, downloading on demand from a secure endpoint, or building it alongside the Go application. Alignment with the checksum logic is important so the loader knows which payload matches each game version.

6. **Error Handling & Telemetry**  
   The loader does not yet record errors beyond in-memory strings. Adding structured logging (and optionally telemetry tied to the authenticated user) will simplify debugging installation issues and failed injections. Consider persisting attempt history in the database for auditability.

7. **Security Hardening**  
   Because the trainer manipulates another process, ensure minimal privilege execution, validate inputs from the database/UI, and consider shipping code-signing or anti-tamper checks to reduce the risk of distributing malicious payloads.

## Recommended Next Steps

1. Finalize checksum data and augment the search routine so players on supported platforms can be detected automatically.
2. Prototype the launch/injection workflow outside of the TUI, then integrate it with clear success/error states in `LoadAssaultCubeModel.Update`.
3. Define configuration knobs for install locations, payload directories, and optional developer overrides, leveraging the existing JSON configuration loader.【F:config.go†L1-L52】
4. Extend the database schema if you need to track per-user trainer settings (e.g., preferred cheats, last launch time) or usage metrics.
5. Add automated tests around version validation and configuration parsing to guard future refactors.

Delivering on the above will convert the current UI skeleton into a functional trainer experience while maintaining the secure workflow that already exists in the app.
