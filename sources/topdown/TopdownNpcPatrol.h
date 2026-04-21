#pragma once

#include <vector>
#include <string>

#include "data/GameState.h"

struct TopdownNpcPatrolRouteOptions {
    bool loop = true;
    bool running = false;
    float waitMs = 0.0f;
};

bool TopdownAssignNpcPatrolRoute(
        GameState& state,
        TopdownNpcRuntime& npc,
        const std::vector<std::string>& spawnIds,
        const TopdownNpcPatrolRouteOptions& options);

void TopdownClearNpcPatrol(TopdownNpcRuntime& npc);
bool TopdownPauseNpcPatrol(TopdownNpcRuntime& npc);
bool TopdownResumeNpcPatrol(TopdownNpcRuntime& npc);

void TopdownUpdateNpcPatrol(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt);

