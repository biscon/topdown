#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "topdown/TopdownCoreData.h"

enum class TopdownTriggerAffects {
    Player,
    Npc,
    All
};

struct TopdownAuthoredTrigger {
    int tiledObjectId = -1;
    std::string id;

    bool usePolygon = false;
    std::vector<Vector2> polygon;
    Rectangle worldRect{};

    bool visible = true;
    std::string script;
    TopdownTriggerAffects affects = TopdownTriggerAffects::Player;
    bool repeat = false;
    float delayMs = 0.0f;

    bool interact = false;
    std::string displayName;
};

using TopdownTriggerHandle = int;

struct TopdownRuntimeTriggerPendingCall {
    bool active = false;
    int authoredIndex = -1;
    TopdownCharacterHandle instigatorHandle = -1;
    bool instigatorIsPlayer = false;
    float remainingMs = 0.0f;
};

struct TopdownRuntimeTrigger {
    TopdownTriggerHandle handle = -1;
    int authoredIndex = -1;

    bool enabled = true;
    bool repeat = false;
    bool fired = false;

    bool playerInside = false;
    std::vector<TopdownCharacterHandle> npcHandlesInside;

    std::vector<TopdownRuntimeTriggerPendingCall> pendingCalls;
};
