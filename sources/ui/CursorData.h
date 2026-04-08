#pragma once

#include "raylib.h"

enum class CursorType {
    Default,
    Interact
};

struct CursorData {
    CursorType type = CursorType::Default;

    Texture2D defaultTexture{};
    Texture2D interactTexture{};

    Vector2 defaultHotspot{0, 0};
    Vector2 interactHotspot{0, 0};

    bool initialized = false;
};
