#pragma once

#include "data/GameState.h"

void TopdownNpcAiHoldAndFire_UpdateInvestigating(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt);

void TopdownNpcAiHoldAndFire_UpdateEngaged(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt);