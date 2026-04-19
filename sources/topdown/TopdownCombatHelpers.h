#pragma once

#include "data/GameState.h"
#include "raymath.h"
#include "TopdownCombatData.h"


Vector2 ComputeShotDirectionWithSpread(
        Vector2 baseDir,
        float spreadDegrees);

TopdownShotHitResult FindFirstHitscanHit(
        GameState& state,
        Vector2 origin,
        Vector2 dir,
        float maxRange);

TopdownShotHitResult FindFirstNpcHitscanHit(
        GameState& state,
        const TopdownNpcRuntime& shooter,
        Vector2 origin,
        Vector2 dir,
        float maxRange);

TopdownNpcDamageResult ApplyDamageToNpc(
        GameState& state,
        TopdownNpcRuntime& npc,
        float damage);

void BeginNpcDeath(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 hitDir,
        float knockbackDistance);

void ApplyNpcHitReaction(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 hitDir,
        float knockbackDistance);

bool TopdownIsNpcShotBlockedByOtherNpc(
        GameState& state,
        const TopdownNpcRuntime& shooter,
        Vector2 origin,
        Vector2 targetPoint,
        float maxRange);

bool TopdownComputeNpcMuzzleWorldPosition(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        const TopdownNpcAssetRuntime& asset,
        Vector2& outWorldPos);