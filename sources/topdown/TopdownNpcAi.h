#pragma once

#include "data/GameState.h"

const char* TopdownNpcAwarenessStateToString(TopdownNpcAwarenessState state);
const char* TopdownNpcCombatStateToString(TopdownNpcCombatState state);

void TopdownStopNpcMovement(TopdownNpcRuntime& npc);
bool TopdownIsPlayerAlive(const GameState& state);

bool TopdownNpcCanSeePlayer(
        GameState& state,
        const TopdownNpcRuntime& npc);

bool TopdownNpcCanHearPlayer(
        GameState& state,
        const TopdownNpcRuntime& npc);

bool TopdownIsPlayerWithinNpcAttackRange(
        const TopdownNpcRuntime& npc,
        const TopdownPlayerRuntime& player);

bool TopdownNpcHasLineOfSightToNpc(
        GameState& state,
        const TopdownNpcRuntime& fromNpc,
        const TopdownNpcRuntime& toNpc);

float TopdownGetNpcClipDurationMs(
        const GameState& state,
        const TopdownNpcClipRef& clipRef);

void TopdownBuildNpcPathToTarget(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 targetPos,
        TopdownNpcMoveOwner owner = TopdownNpcMoveOwner::Ai);

void TopdownApplyDamageToPlayer(
        GameState& state,
        float damage,
        Vector2 attackerPos);

void TopdownUpdateNpcAiSeekAndDestroy(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt);

void TopdownUpdateNpcAi(GameState& state, float dt);

void TopdownAlertNpcToPlayer(
        GameState& state,
        TopdownNpcRuntime& npc);

void TopdownAlertNearbyNpcs(
        GameState& state,
        const TopdownNpcRuntime& sourceNpc,
        float radius);

Vector2 TopdownBuildNpcInvestigationTargetAroundPoint(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        Vector2 center);

bool TopdownHasNpcReachedPoint(
        const TopdownNpcRuntime& npc,
        Vector2 point,
        float radius);