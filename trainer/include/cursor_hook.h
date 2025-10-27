#ifndef CURSOR_HOOK_H
#define CURSOR_HOOK_H

// Install a hook on ClipCursor to prevent the game from locking the cursor
bool InstallCursorHook();

// Remove the ClipCursor hook
void RemoveCursorHook();

// Set whether cursor clipping should be blocked
// When true, any attempt by the game to clip the cursor will be ignored
void SetCursorClipBlocked(bool blocked);

#endif // CURSOR_HOOK_H
