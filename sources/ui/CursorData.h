#pragma once

#include "raylib.h"

enum class CursorType {
    Default,
    Interact,
    Aim
};

struct CursorData {
    CursorType type = CursorType::Default;

    Texture2D defaultTexture{};
    Texture2D interactTexture{};

    Vector2 defaultHotspot{0, 0};
    Vector2 interactHotspot{0, 0};

    float aimPulseTimeSeconds = 0.0f;

    bool initialized = false;
};
