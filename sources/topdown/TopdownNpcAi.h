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
        Vector2 targetPos);

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

void TopdownUpdateInvestigationContexts(GameState& state, float dtMs);

void TopdownAssignNpcInvestigationSlot(
        GameState& state,
        TopdownNpcRuntime& npc);

void TopdownClearNpcInvestigationSlot(
        GameState& state,
        TopdownNpcRuntime& npc);

bool TopdownGetNpcInvestigationDestination(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        Vector2& outDestination);
