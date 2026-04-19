#pragma once

#include "raymath.h"
#include "TopdownData.h"

enum class TopdownShotHitType
{
    None,
    Npc,
    Door,
    Window,
    Wall,
    Player
};

struct TopdownShotHitResult
{
    TopdownShotHitType type = TopdownShotHitType::None;
    TopdownNpcRuntime* npc = nullptr;
    TopdownRuntimeDoor* door = nullptr;
    TopdownRuntimeWindow* window = nullptr;
    Vector2 point{};
    Vector2 normal{};
    float distance = 0.0f;
};

struct TopdownNpcDamageResult
{
    bool validTarget = false;
    bool killed = false;
    float damageApplied = 0.0f;
};
