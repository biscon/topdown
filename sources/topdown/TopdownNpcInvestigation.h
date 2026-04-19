#pragma once

#include "data/GameState.h"

enum class TopdownNpcInvestigationUpdateResult {
    Running,
    Arrived,
    Failed
};

void TopdownResetNpcInvestigationState(TopdownNpcRuntime& npc);

bool TopdownBeginNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc);

void TopdownLeaveNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc);

TopdownNpcInvestigationUpdateResult TopdownUpdateNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt);

void TopdownPruneNpcInvestigationContexts(GameState& state);