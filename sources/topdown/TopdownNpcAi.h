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

void TopdownAlertNpcsByGunshot(
        GameState& state,
        Vector2 shotOrigin);

bool TopdownHasNpcReachedPoint(
        const TopdownNpcRuntime& npc,
        Vector2 point,
        float radius);

void TopdownResetNpcSearchTimers(TopdownNpcRuntime& npc);
void TopdownResetNpcLostTargetProgress(TopdownNpcRuntime& npc);
void TopdownResetNpcChaseStuckWatchdog(TopdownNpcRuntime& npc);

bool TopdownHasNpcReachedLastKnownTarget(
        const TopdownNpcRuntime& npc,
        float arriveRadius = 300.0f);

void TopdownFinishNpcSearchAndForgetTarget(TopdownNpcRuntime& npc);
void TopdownBeginNpcSearchState(
        TopdownNpcRuntime& npc,
        float durationMs = 3600.0f,
        float sweepDegrees = 260.0f);

bool TopdownUpdateNpcChaseStuckWatchdog(
        TopdownNpcRuntime& npc,
        float dtMs,
        float probePeriodMs = 800.0f,
        float minDistancePerProbe = 20.0f);

bool TopdownTryBuildNpcChaseTarget(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer,
        Vector2& outChaseTarget);

void TopdownUpdateNpcSearchState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt);

void TopdownUpdateNpcPerception(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dtMs);

void TopdownUpdateNpcPersistentChaseState(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer);
