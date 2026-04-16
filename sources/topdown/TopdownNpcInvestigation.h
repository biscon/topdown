#pragma once

#include "data/GameState.h"

void TopdownResetNpcInvestigationState(TopdownNpcRuntime& npc);

bool TopdownBeginNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc);

void TopdownLeaveNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc);

void TopdownUpdateNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt);

void TopdownCleanupNpcInvestigationContexts(GameState& state);
