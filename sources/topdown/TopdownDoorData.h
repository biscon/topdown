#pragma once

#include <string>

#include "raylib.h"

enum class TopdownDoorHingeSide {
    Left,
    Right,
    Top,
    Bottom
};

struct TopdownAuthoredDoor {
    int tiledObjectId = -1;
    std::string id;
    bool visible = true;

    Vector2 rectPosition{};
    Vector2 rectSize{};

    TopdownDoorHingeSide hingeSide = TopdownDoorHingeSide::Left;

    bool locked = false;

    bool autoClose = false;
    float autoCloseStrength = 6.0f;
    float damping = 5.0f;

    float swingMinDegrees = -90.0f;
    float swingMaxDegrees = 90.0f;

    std::string openSoundId;
    std::string closeSoundId;

    Color color = Color{92, 58, 34, 255};
    Color outlineColor = BLACK;
};

struct TopdownRuntimeDoor {
    int tiledObjectId = -1;
    std::string id;
    bool visible = true;

    Vector2 hinge{};
    float length = 0.0f;
    float thickness = 0.0f;

    float closedAngleRadians = 0.0f;
    float angleRadians = 0.0f;
    float angularVelocity = 0.0f;

    float swingMinRadians = -90.0f * DEG2RAD;
    float swingMaxRadians = 90.0f * DEG2RAD;

    bool locked = false;

    bool autoClose = false;
    float autoCloseStrength = 6.0f;
    float damping = 5.0f;

    std::string openSoundId;
    std::string closeSoundId;
    bool wasNearClosed = true;
    bool openSoundPlayedThisSwing = false;
    Color color = Color{92, 58, 34, 255};
    Color outlineColor = BLACK;
};
