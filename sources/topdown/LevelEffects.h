#pragma once

#include "data/GameState.h"

void AppendTracerEffectAnchoredToPlayer(
        GameState& state,
        Vector2 start,
        Vector2 end,
        TopdownTracerStyle style);
void AppendTracerEffectAnchoredToNpc(
        GameState& state,
        TopdownCharacterHandle npcHandle,
        Vector2 start,
        Vector2 end,
        TopdownTracerStyle style);

void SpawnWallImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 hitNormal,
        const TopdownBallisticImpactEffectConfig& fxConfig);

void SpawnMuzzleFlashEffectAnchoredToPlayer(
        GameState& state,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownMuzzleEffectConfig& fxConfig);
void SpawnMuzzleFlashEffectAnchoredToNpc(
        GameState& state,
        TopdownCharacterHandle npcHandle,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownMuzzleEffectConfig& fxConfig);

void SpawnMuzzleSmokeParticles(
        GameState& state,
        Vector2 muzzleWorld,
        Vector2 shotDir,
        const TopdownMuzzleEffectConfig& fxConfig);

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
void QueueBloodSpatterDecals(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownNpcAttackEffectsConfig& fxConfig);

void SpawnBloodPoolEmitter(
        GameState& state,
        Vector2 position,
        float maxRadius,
        float durationMs);

void SpawnBloodImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownPlayerWeaponConfig& weaponConfig);
void SpawnBloodImpactParticles(
        GameState& state,
        Vector2 hitPoint,
        Vector2 incomingShotDir,
        const TopdownNpcAttackEffectsConfig& fxConfig);



void TopdownUpdateLevelEffects(GameState& state, float dt);
