#pragma once

#include "data/GameState.h"

void AppendPlayerTracerEffect(
        GameState& state,
        Vector2 start,
        Vector2 end,
        TopdownTracerStyle style);

void SpawnWallImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 hitNormal,
        const TopdownPlayerWeaponConfig& weaponConfig);

void SpawnMuzzleFlashEffect(
        GameState& state,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownPlayerWeaponConfig& weaponConfig);

void SpawnMuzzleSmokeParticles(
        GameState& state,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownPlayerWeaponConfig& weaponConfig);

bool TopdownShakeScreen(GameState& state,
                        float durationMs,
                        float strengthPx,
                        float frequencyHz,
                        bool smooth);

void SpawnBloodSpatterDecals(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig);

void QueueBloodSpatterDecals(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig);

void SpawnBloodImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig);



void TopdownUpdateLevelEffects(GameState& state, float dt);

