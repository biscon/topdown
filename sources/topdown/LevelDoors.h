#pragma once

#include "data/GameState.h"

void TopdownUpdateDoors(GameState& state, float dt);

void ResolvePlayerVsDoors(GameState& state);
void ResolveNpcVsDoors(GameState& state, TopdownNpcRuntime& npc);
void ResolveNpcKnockbackVsDoors(GameState& state, TopdownNpcRuntime& npc);

void ApplyDoorMotionPushToPlayer(GameState& state, float dt);
void ApplyDoorMotionPushToNpcs(GameState& state, float dt);

bool RaycastClosestDoor(
        GameState& state,
        Vector2 origin,
        Vector2 dirNormalized,
        float maxDistance,
        TopdownRuntimeDoor*& outDoor,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance);

void ApplyDoorBallisticImpulse(
        TopdownRuntimeDoor& door,
        Vector2 hitPoint,
        Vector2 shotDir,
        float impulseMagnitude);