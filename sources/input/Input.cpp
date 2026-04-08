//
// Created by bison on 18-11-25.
//

#include <algorithm>
#include "Input.h"

void InitInput(InputData& input) {
    input.events.clear();
    input.lastClickTime = -1.0f;
    input.doubleClickThreshold = 0.3f;
    input.keyRepeatStates = {};
    input.keyRepeatInitialDelay = 0.45f;
    input.keyRepeatInterval = 0.04f;
}

static void AddEvent(InputData& input, const InputEvent& evt) {
    input.events.push_back(evt);
}

static void AddKeyEvent(InputData& input, InputEventType type, int key)
{
    InputEvent evt;
    evt.type = type;
    evt.handled = false;
    evt.key.key = key;
    AddEvent(input, evt);
}

static void UpdateKeyRepeat(InputData& input, float dt)
{
    for (auto it = input.keyRepeatStates.begin(); it != input.keyRepeatStates.end(); )
    {
        const int key = it->first;
        KeyRepeatState& state = it->second;

        if (!IsKeyDown(key)) {
            AddKeyEvent(input, InputEventType::KeyReleased, key);
            it = input.keyRepeatStates.erase(it);
            continue;
        }

        state.heldTime += dt;

        while (state.heldTime >= state.nextRepeatTime) {
            AddKeyEvent(input, InputEventType::KeyRepeated, key);
            state.nextRepeatTime += input.keyRepeatInterval;
        }

        ++it;
    }
}

void UpdateInput(InputData& input)
{
    input.events.clear();

    Vector2 mouseScreen = GetMousePosition();
    const float now = (float)GetTime();
    const float dt = GetFrameTime();

    // ---- MOUSE BUTTONS / CLICKS ------------------------------------------------------

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        InputEvent evt;
        evt.type = InputEventType::MouseButtonPressed;
        evt.handled = false;
        evt.mouse.pos = mouseScreen;
        evt.mouse.button = MOUSE_LEFT_BUTTON;
        evt.mouse.doubleClick = false;
        AddEvent(input, evt);
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        InputEvent evt;
        evt.type = InputEventType::MouseButtonPressed;
        evt.handled = false;
        evt.mouse.pos = mouseScreen;
        evt.mouse.button = MOUSE_RIGHT_BUTTON;
        evt.mouse.doubleClick = false;
        AddEvent(input, evt);
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        const bool dbl = (now - input.lastClickTime <= input.doubleClickThreshold);
        input.lastClickTime = now;

        InputEvent releasedEvt;
        releasedEvt.type = InputEventType::MouseButtonReleased;
        releasedEvt.handled = false;
        releasedEvt.mouse.pos = mouseScreen;
        releasedEvt.mouse.button = MOUSE_LEFT_BUTTON;
        releasedEvt.mouse.doubleClick = dbl;
        AddEvent(input, releasedEvt);

        InputEvent clickEvt;
        clickEvt.type = InputEventType::MouseClick;
        clickEvt.handled = false;
        clickEvt.mouse.pos = mouseScreen;
        clickEvt.mouse.button = MOUSE_LEFT_BUTTON;
        clickEvt.mouse.doubleClick = dbl;
        AddEvent(input, clickEvt);
    }

    if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
        const bool dbl = (now - input.lastClickTime <= input.doubleClickThreshold);
        input.lastClickTime = now;

        InputEvent releasedEvt;
        releasedEvt.type = InputEventType::MouseButtonReleased;
        releasedEvt.handled = false;
        releasedEvt.mouse.pos = mouseScreen;
        releasedEvt.mouse.button = MOUSE_RIGHT_BUTTON;
        releasedEvt.mouse.doubleClick = dbl;
        AddEvent(input, releasedEvt);

        InputEvent clickEvt;
        clickEvt.type = InputEventType::MouseClick;
        clickEvt.handled = false;
        clickEvt.mouse.pos = mouseScreen;
        clickEvt.mouse.button = MOUSE_RIGHT_BUTTON;
        clickEvt.mouse.doubleClick = dbl;
        AddEvent(input, clickEvt);
    }

    // ---- KEYBOARD -----------------------------------------------------

    for (;;) {
        const int key = GetKeyPressed();
        if (key == 0) {
            break;
        }

        AddKeyEvent(input, InputEventType::KeyPressed, key);

        KeyRepeatState state;
        state.down = true;
        state.heldTime = 0.0f;
        state.nextRepeatTime = input.keyRepeatInitialDelay;

        input.keyRepeatStates[key] = state;
    }

    UpdateKeyRepeat(input, dt);

    // ---- TEXT INPUT ---------------------------------------------------

    for (;;) {
        const int codepoint = GetCharPressed();
        if (codepoint == 0) {
            break;
        }

        InputEvent evt;
        evt.type = InputEventType::TextInput;
        evt.handled = false;
        evt.text.codepoint = static_cast<unsigned int>(codepoint);
        AddEvent(input, evt);
    }

    // KeyReleased is synthesized in UpdateKeyRepeat().
}
