#pragma once

#include "data/GameState.h"

void TopdownNpcAiSeekAndDestroy_UpdateInvestigating(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt);

void TopdownNpcAiSeekAndDestroy_UpdateEngaged(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt);