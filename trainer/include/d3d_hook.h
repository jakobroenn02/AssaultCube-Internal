#pragma once

#include <Windows.h>

class Trainer;
class UIRenderer;

bool InstallHooks(HWND gameWindow, Trainer* trainer, UIRenderer* renderer);
void RemoveHooks();
