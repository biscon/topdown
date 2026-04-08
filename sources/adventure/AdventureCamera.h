#pragma once
#include "data/GameState.h"

Vector2 GetActorCameraCenterWorldPos(const GameState& state, const ActorInstance& actor);
Vector2 GetImmediateCenteredCameraPosition(const GameState& state, const ActorInstance& actor);
void UpdateCamera(GameState& state, float dt);