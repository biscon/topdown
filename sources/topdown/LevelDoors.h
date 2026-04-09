#pragma once

#include "data/GameState.h"

void TopdownUpdateDoors(GameState& state, float dt);

void ResolvePlayerVsDoors(GameState& state);
void ResolveNpcVsDoors(GameState& state, TopdownNpcRuntime& npc);
void ResolveNpcKnockbackVsDoors(GameState& state, TopdownNpcRuntime& npc);

void ApplyDoorMotionPushToPlayer(GameState& state, float dt);
void ApplyDoorMotionPushToNpcs(GameState& state, float dt);
