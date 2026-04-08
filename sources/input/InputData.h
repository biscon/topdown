//
// Created by bison on 18-11-25.
//

#pragma once
#include "raylib.h"
#include <array>
#include <vector>
#include <unordered_map>

enum class InputEventType {
    MouseButtonPressed,
    MouseButtonReleased,
    MouseClick,
    KeyPressed,
    KeyReleased,
    KeyRepeated,
    TextInput,
    Any
};

struct MouseClickEvent {
    Vector2 pos;
    int button;       // MOUSE_LEFT_BUTTON, etc
    bool doubleClick;
};

struct KeyEvent {
    int key;          // KEY_A, KEY_SPACE, etc
};

struct TextInputEvent {
    unsigned int codepoint = 0;
};

struct InputEvent {
    InputEventType type;
    bool handled = false;  // ← consumer sets this

    union {
        MouseClickEvent mouse{};
        KeyEvent key;
        TextInputEvent text;
    };
};

static constexpr int INPUT_MAX_TRACKED_KEYS = 512;

struct KeyRepeatState {
    bool down = false;
    float heldTime = 0.0f;
    float nextRepeatTime = 0.0f;
};

struct InputData {
    std::vector<InputEvent> events;

    // double-click logic
    float lastClickTime = -1.0f;
    float doubleClickThreshold = 0.3f;

    // key repeat
    std::unordered_map<int, KeyRepeatState> keyRepeatStates;
    float keyRepeatInitialDelay = 0.45f;
    float keyRepeatInterval = 0.04f;
};
