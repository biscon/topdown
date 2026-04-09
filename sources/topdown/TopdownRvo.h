#pragma once

#include "data/GameState.h"

void TopdownRvoInit(GameState& state);
void TopdownRvoShutdown(GameState& state);

void TopdownRvoRequestRebuild(GameState& state);
void TopdownRvoEnsureReady(GameState& state);

void TopdownRvoSync(GameState& state);
void TopdownRvoStep(GameState& state, float dt);

bool TopdownRvoHasAgent(GameState& state, int npcHandle);
Vector2 TopdownRvoGetVelocity(GameState& state, int npcHandle);