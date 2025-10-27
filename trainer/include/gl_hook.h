#pragma once

#include <Windows.h>

class Trainer;
class UIRenderer;

bool InstallHooks(HWND gameWindow, Trainer* trainer, UIRenderer* renderer);
void RemoveHooks();

// Get the captured view matrix from glLoadMatrixf hook
bool GetCapturedViewMatrix(float* outMatrix);  // Returns true if matrix is valid
