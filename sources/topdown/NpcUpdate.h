#pragma once

#include "data/GameState.h"

void StartNpcKnockback(
        TopdownNpcRuntime& npc,
        Vector2 dir,
        float knockbackDistance);

void TopdownUpdateNpcAnimation(GameState& state, float dt);
void TopdownUpdateNpcLogic(GameState& state, float dt);
