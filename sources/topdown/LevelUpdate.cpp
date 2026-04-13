#include "topdown/LevelUpdate.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/LevelCamera.h"
#include "topdown/PlayerUpdate.h"
#include "raylib.h"
#include "NpcRegistry.h"
#include "topdown/PlayerLoad.h"
#include "resources/AsepriteAsset.h"
#include "audio/Audio.h"
#include "LevelCollision.h"
#include "NpcUpdate.h"
#include "LevelEffects.h"
#include "menu/Menu.h"
#include "LevelLoad.h"
#include "TopdownNpcAi.h"
#include "TopdownRvo.h"
#include "LevelDoors.h"
#include "utils/ScopeTimer.h"

enum class TopdownShotHitType
{
    None,
    Npc,
    Door,
    Wall
};

struct TopdownShotHitResult
{
    TopdownShotHitType type = TopdownShotHitType::None;
    TopdownNpcRuntime* npc = nullptr;
    TopdownRuntimeDoor* door = nullptr;
    Vector2 point{};
    Vector2 normal{};
    float distance = 0.0f;
};

struct PendingNpcShotResult
{
    TopdownCharacterHandle handle = -1;
    TopdownNpcRuntime* npc = nullptr;
    float totalDamage = 0.0f;
    Vector2 hitPoint{};
    Vector2 hitDir{};
    bool hasHit = false;
};

struct PendingDoorShotResult
{
    TopdownRuntimeDoor* door = nullptr;
    int hitCount = 0;
    Vector2 hitPoint{};
    Vector2 hitNormal{};
    Vector2 hitDir{};
    bool hasHit = false;
};

struct TopdownNpcDamageResult
{
    bool validTarget = false;
    bool killed = false;
    float damageApplied = 0.0f;
};

static Vector2 ClampMouseWorldToPlayerShootingDeadzone(
        const GameState& state,
        Vector2 mouseWorld,
        Vector2 muzzleWorld)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    const Vector2 center = player.position;

    // Deadzone based on:
    // 1) a multiple of player radius
    // 2) circle centered on player that reaches the muzzle, plus padding
    const float muzzleDistanceFromCenter =
            TopdownLength(TopdownSub(muzzleWorld, center));
    const float deadzoneRadius = std::max(
            player.radius * 2.5f,
            muzzleDistanceFromCenter + 20.0f);

    Vector2 delta = TopdownSub(mouseWorld, center);
    float dist = TopdownLength(delta);

    if (dist >= deadzoneRadius) {
        return mouseWorld;
    }

    Vector2 dir = TopdownNormalizeOrZero(delta);

    if (TopdownLengthSqr(dir) <= 0.000001f) {
        dir = Vector2{
                std::cos(character.upperRotationRadians),
                std::sin(character.upperRotationRadians)
        };
        dir = TopdownNormalizeOrZero(dir);
    }

    if (TopdownLengthSqr(dir) <= 0.000001f) {
        dir = Vector2{1.0f, 0.0f};
    }

    return TopdownAdd(center, TopdownMul(dir, deadzoneRadius));
}

static Vector2 ComputePlayerWeaponMuzzleWorldPosition(
        const GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    const float radians = character.upperRotationRadians;
    const Vector2 forward{
            std::cos(radians),
            std::sin(radians)
    };
    const Vector2 right{
            -forward.y,
            forward.x
    };

    return TopdownAdd(
            player.position,
            TopdownAdd(
                    TopdownMul(forward, weaponConfig.muzzleOrigin.x),
                    TopdownMul(right, weaponConfig.muzzleOrigin.y)));
}

static bool TryComputePlayerAttackAnimationMuzzleWorldPosition(
        const GameState& state,
        const std::string& animationId,
        Vector2& outWorldPos)
{
    const TopdownPlayerAnimationEntry* animationEntry =
            FindTopdownPlayerAnimationEntry(state, animationId);

    if (animationEntry == nullptr || !animationEntry->hasMuzzle) {
        return false;
    }

    const SpriteAssetResource* sprite =
            FindSpriteAssetResource(state.resources, animationEntry->spriteHandle);

    if (sprite == nullptr || !sprite->loaded || !sprite->hasExplicitOrigin) {
        return false;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    const float drawScale = sprite->baseDrawScale;

    const float localX = (animationEntry->muzzle.x - sprite->origin.x) * drawScale;
    const float localY = (animationEntry->muzzle.y - sprite->origin.y) * drawScale;

    const float radians = character.upperRotationRadians;
    const Vector2 forward{
            std::cos(radians),
            std::sin(radians)
    };
    const Vector2 right{
            -forward.y,
            forward.x
    };

    outWorldPos = TopdownAdd(
            player.position,
            TopdownAdd(
                    TopdownMul(forward, localX),
                    TopdownMul(right, localY)));

    return true;
}

static Vector2 ComputeShotDirectionWithSpread(
        Vector2 baseDir,
        float spreadDegrees)
{
    if (spreadDegrees <= 0.0f) {
        return TopdownNormalizeOrZero(baseDir);
    }

    const float halfSpreadRadians = spreadDegrees * 0.5f * DEG2RAD;
    const float randomAngle = GetRandomValue(-10000, 10000) / 10000.0f * halfSpreadRadians;

    const float c = std::cos(randomAngle);
    const float s = std::sin(randomAngle);

    Vector2 dir{
            baseDir.x * c - baseDir.y * s,
            baseDir.x * s + baseDir.y * c
    };

    return TopdownNormalizeOrZero(dir);
}

static TopdownShotHitResult FindFirstHitscanHit(
        GameState& state,
        Vector2 origin,
        Vector2 dir,
        float maxRange)
{
    TopdownShotHitResult result;
    result.point = TopdownAdd(origin, TopdownMul(dir, maxRange));
    result.distance = maxRange;
    result.npc = nullptr;
    result.door = nullptr;

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible || npc.dead) {
            continue;
        }

        float hitDistance = 0.0f;
        Vector2 hitPoint{};
        Vector2 hitNormal{};

        if (RaycastCircleDetailed(
                origin,
                dir,
                npc.position,
                npc.collisionRadius,
                result.distance,
                hitDistance,
                hitPoint,
                hitNormal)) {
            result.type = TopdownShotHitType::Npc;
            result.npc = &npc;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    TopdownRuntimeDoor* hitDoor = nullptr;
    Vector2 doorPoint{};
    Vector2 doorNormal{};
    float doorDistance = result.distance;

    if (RaycastClosestDoor(
            state,
            origin,
            dir,
            result.distance,
            hitDoor,
            doorPoint,
            doorNormal,
            doorDistance)) {
        result.type = TopdownShotHitType::Door;
        result.npc = nullptr;
        result.door = hitDoor;
        result.point = doorPoint;
        result.normal = doorNormal;
        result.distance = doorDistance;
    }

    Vector2 wallPoint{};
    Vector2 wallNormal{};
    float wallDistance = result.distance;

    if (RaycastClosestSegmentWithNormal(
            origin,
            dir,
            state.topdown.runtime.collision.visionSegments,
            result.distance,
            wallPoint,
            wallNormal,
            wallDistance)) {
        result.type = TopdownShotHitType::Wall;
        result.npc = nullptr;
        result.point = wallPoint;
        result.normal = wallNormal;
        result.distance = wallDistance;
    }

    wallDistance = result.distance;
    if (RaycastClosestSegmentWithNormal(
            origin,
            dir,
            state.topdown.runtime.collision.boundarySegments,
            result.distance,
            wallPoint,
            wallNormal,
            wallDistance)) {
        result.type = TopdownShotHitType::Wall;
        result.npc = nullptr;
        result.point = wallPoint;
        result.normal = wallNormal;
        result.distance = wallDistance;
    }

    return result;
}

static bool IsShotBlockedFromPlayerOrigin(
        const TopdownShotHitResult& muzzleHit,
        const TopdownShotHitResult& playerOriginHit)
{
    if (muzzleHit.type != TopdownShotHitType::Npc || muzzleHit.npc == nullptr) {
        return false;
    }

    if (playerOriginHit.type != TopdownShotHitType::Door &&
        playerOriginHit.type != TopdownShotHitType::Wall) {
        return false;
    }

    static constexpr float kDistanceEpsilon = 0.01f;
    return playerOriginHit.distance + kDistanceEpsilon < muzzleHit.distance;
}

static TopdownNpcDamageResult ApplyDamageToNpc(
        GameState& state,
        TopdownNpcRuntime& npc,
        float damage)
{
    TopdownNpcDamageResult result;

    if (npc.dead) {
        return result;
    }

    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    if (asset == nullptr || !asset->loaded) {
        return result;
    }

    result.validTarget = true;

    if (damage <= 0.0f) {
        return result;
    }

    const float oldHealth = npc.health;
    npc.health -= damage;

    if (npc.health < 0.0f) {
        npc.health = 0.0f;
    }

    result.damageApplied = std::max(0.0f, oldHealth - npc.health);
    result.killed = (oldHealth > 0.0f && npc.health <= 0.0f);
    return result;
}

static void PlayNpcHitReactionSound(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    if (npc.painSoundCooldownMs > 0.0f) {
        return;
    }

    if (!npc.hitReactionSoundIds.empty()) {
        const int idx = GetRandomValue(0, static_cast<int>(npc.hitReactionSoundIds.size()) - 1);
        PlaySoundById(
                state,
                npc.hitReactionSoundIds[idx],
                RandomRangeFloat(0.92f, 1.08f));
        npc.painSoundCooldownMs = 750.0f;
        return;
    }

    static const char* fallbackPainSounds[] = {
            "human_pain_01",
            "human_pain_02",
            "human_pain_03",
            "human_pain_04",
            "human_pain_05",
            "human_pain_06"
    };

    const int idx = GetRandomValue(0, 5);
    PlaySoundById(state, fallbackPainSounds[idx], RandomRangeFloat(0.92f, 1.08f));
    npc.painSoundCooldownMs = 750.0f;
}

static void ApplyNpcHitReaction(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 hitDir,
        float knockbackDistance)
{
    if (npc.dead) {
        return;
    }

    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    if (asset == nullptr || !asset->loaded) {
        return;
    }

    npc.hurtStunRemainingMs = std::max(npc.hurtStunRemainingMs, asset->hurtStunMs);

    npc.move = {};
    npc.moving = false;
    npc.running = false;

    StartNpcKnockback(npc, hitDir, knockbackDistance);

    if (TopdownNpcClipRefIsValid(asset->hurtClip)) {
        TopdownPlayNpcOneShotAnimation(npc, asset->hurtClip);
    }

    PlayNpcHitReactionSound(state, npc);
}

static void BeginNpcDeath(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 hitDir,
        float knockbackDistance)
{
    if (npc.dead) {
        return;
    }

    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    if (asset == nullptr || !asset->loaded) {
        return;
    }

    npc.health = 0.0f;
    npc.dead = true;
    npc.corpse = false;

    npc.hasPlayerTarget = false;
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
    npc.combatState = TopdownNpcCombatState::None;
    npc.loseTargetTimerMs = 0.0f;
    npc.repathTimerMs = 0.0f;

    npc.attackHitPending = false;
    npc.attackHitApplied = false;
    npc.attackStateTimeMs = 0.0f;
    npc.attackAnimationDurationMs = 0.0f;
    npc.attackCooldownRemainingMs = 0.0f;

    TopdownRvoRequestRebuild(state);

    npc.move = {};
    npc.moving = false;
    npc.running = false;

    npc.hurtStunRemainingMs = 0.0f;
    npc.painSoundCooldownMs = 0.0f;

    StartNpcKnockback(npc, hitDir, knockbackDistance);

    if (TopdownNpcClipRefIsValid(asset->deathClip)) {
        TopdownPlayNpcOneShotAnimation(npc, asset->deathClip);
    } else if (TopdownNpcClipRefIsValid(asset->hurtClip)) {
        TopdownPlayNpcOneShotAnimation(npc, asset->hurtClip);
    } else {
        npc.corpse = true;
        TopdownClearNpcOneShotAnimation(npc);
        npc.knockbackVelocity = Vector2{};
    }
}

static bool IsNpcInsidePlayerMeleeArc(
        const TopdownPlayerRuntime& player,
        const TopdownCharacterRuntime& character,
        const TopdownNpcRuntime& npc,
        float meleeRange,
        float meleeArcDegrees)
{
    if (meleeRange <= 0.0f || meleeArcDegrees <= 0.0f) {
        return false;
    }

    Vector2 toCenter = TopdownSub(npc.position, player.position);
    float centerDist = TopdownLength(toCenter);

    if (centerDist <= 0.000001f) {
        return true;
    }

    Vector2 dirToCenter = TopdownMul(toCenter, 1.0f / centerDist);

    // Closest point on NPC circle toward the player.
    Vector2 closestPointOnNpc = TopdownSub(
            npc.position,
            TopdownMul(dirToCenter, npc.collisionRadius));

    Vector2 toHitPoint = TopdownSub(closestPointOnNpc, player.position);
    float hitPointDist = TopdownLength(toHitPoint);

    if (hitPointDist > meleeRange) {
        return false;
    }

    Vector2 attackForward{
            std::cos(character.upperRotationRadians),
            std::sin(character.upperRotationRadians)
    };
    attackForward = TopdownNormalizeOrZero(attackForward);

    if (TopdownLengthSqr(attackForward) <= 0.000001f) {
        attackForward = player.facing;
    }

    if (TopdownLengthSqr(attackForward) <= 0.000001f) {
        attackForward = Vector2{1.0f, 0.0f};
    }

    Vector2 toHitDir = TopdownNormalizeOrZero(toHitPoint);
    if (TopdownLengthSqr(toHitDir) <= 0.000001f) {
        return true;
    }

    const float cosThreshold =
            std::cos((meleeArcDegrees * 0.5f) * DEG2RAD);

    return TopdownDot(attackForward, toHitDir) >= cosThreshold;
}

static TopdownNpcRuntime* FindBestPlayerMeleeTarget(
        GameState& state,
        float meleeRange,
        float meleeArcDegrees)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    TopdownNpcRuntime* bestNpc = nullptr;
    float bestDistSqr = 0.0f;

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible || npc.dead) {
            continue;
        }

        if (!IsNpcInsidePlayerMeleeArc(player, character, npc, meleeRange, meleeArcDegrees)) {
            continue;
        }

        const float distSqr = TopdownLengthSqr(TopdownSub(npc.position, player.position));

        if (bestNpc == nullptr || distSqr < bestDistSqr) {
            bestNpc = &npc;
            bestDistSqr = distSqr;
        }
    }

    return bestNpc;
}

static float GetPlayerAttackAnimationDurationMs(
        const GameState& state,
        const std::string& equipmentSetId,
        TopdownAttackType attackType)
{
    const SpriteAssetHandle handle =
            FindTopdownPlayerEquipmentAttackAnimationHandle(
                    state,
                    equipmentSetId,
                    attackType);

    if (handle < 0) {
        return 0.0f;
    }

    const SpriteAssetResource* sprite =
            FindSpriteAssetResource(state.resources, handle);

    if (sprite == nullptr || !sprite->loaded || sprite->clips.empty()) {
        return 0.0f;
    }

    return GetOneShotClipDurationMs(*sprite, sprite->clips[0]);
}

static void TriggerPlayerWeaponScreenShake(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    // Shotgun:
    // short, punchy, unsmoothed kick on every blast
    if (weaponConfig.equipmentSetId == "shotgun") {
        TopdownShakeScreen(state, 110.0f, 18.0f, 55.0f, false);
        return;
    }

    // Rifle full auto:
    // light, smooth vibration while spraying, but gated so it does not
    // restart every single bullet.
    if (weaponConfig.equipmentSetId == "rifle" &&
        attack.currentFireMode == TopdownFireMode::FullAuto) {
        if (attack.fullAutoShakeCooldownMs <= 0.0f) {
            TopdownShakeScreen(state, 80.0f, 5.0f, 35.0f, true);
            attack.fullAutoShakeCooldownMs = 70.0f;
        }
        return;
    }
}

struct PlayerRangedAttackContext
{
    Vector2 muzzleWorld{};
    Vector2 baseDir{1.0f, 0.0f};
};

static bool BeginPlayerAttackRuntime(
        GameState& state,
        TopdownAttackInput input,
        const TopdownPlayerWeaponConfig*& outWeaponConfig,
        TopdownAttackType& outAttackType)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    if (attack.active) {
        return false;
    }

    const TopdownPlayerWeaponConfig* weaponConfig =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, character.equippedSetId);

    if (weaponConfig == nullptr) {
        return false;
    }

    const TopdownAttackType attackType =
            (input == TopdownAttackInput::Primary)
            ? weaponConfig->primaryAttackType
            : weaponConfig->secondaryAttackType;

    if (attackType == TopdownAttackType::None) {
        return false;
    }

    if (attack.cooldownRemainingMs > 0.0f) {
        return false;
    }

    attack.active = true;
    attack.state = TopdownPlayerAttackState::Recover;
    attack.input = input;
    attack.attackType = attackType;
    attack.stateTimeMs = 0.0f;
    attack.equipmentSetId = weaponConfig->equipmentSetId;
    attack.animationDurationMs =
            GetPlayerAttackAnimationDurationMs(
                    state,
                    character.equippedSetId,
                    attackType);

    const SpriteAssetHandle attackAnimationHandle =
            FindTopdownPlayerEquipmentAttackAnimationHandle(
                    state,
                    character.equippedSetId,
                    attackType);

    if (attackAnimationHandle < 0) {
        TraceLog(LOG_WARNING,
                 "Missing player attack animation for equipment '%s' attackType=%d",
                 character.equippedSetId.c_str(),
                 static_cast<int>(attackType));
    }

    attack.cooldownRemainingMs =
            (input == TopdownAttackInput::Primary)
            ? weaponConfig->primaryCooldownMs
            : weaponConfig->secondaryCooldownMs;

    outWeaponConfig = weaponConfig;
    outAttackType = attackType;
    return true;
}

static Vector2 BuildPlayerAttackAimDirection(const GameState& state)
{
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    Vector2 dir{
            std::cos(character.upperRotationRadians),
            std::sin(character.upperRotationRadians)
    };
    dir = TopdownNormalizeOrZero(dir);

    if (TopdownLengthSqr(dir) <= 0.000001f) {
        dir = state.topdown.runtime.player.facing;
    }

    if (TopdownLengthSqr(dir) <= 0.000001f) {
        dir = Vector2{1.0f, 0.0f};
    }

    return dir;
}

static PlayerRangedAttackContext BuildPlayerRangedAttackContext(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    PlayerRangedAttackContext ctx;

    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    const std::string attackAnimationId =
            FindTopdownPlayerEquipmentAttackAnimationId(
                    state,
                    character.equippedSetId,
                    TopdownAttackType::Ranged);

    if (!attackAnimationId.empty() &&
        TryComputePlayerAttackAnimationMuzzleWorldPosition(state, attackAnimationId, ctx.muzzleWorld)) {
        // authored muzzle point used
    } else {
        ctx.muzzleWorld = ComputePlayerWeaponMuzzleWorldPosition(state, weaponConfig);
    }

    Vector2 mouseWorld = GetMouseWorldPosition(state);
    mouseWorld = ClampMouseWorldToPlayerShootingDeadzone(state, mouseWorld, ctx.muzzleWorld);
    (void)mouseWorld;

    ctx.baseDir = BuildPlayerAttackAimDirection(state);
    return ctx;
}

static void PlayPlayerRangedAttackAudioAndFx(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        const PlayerRangedAttackContext& ctx)
{
    SpawnMuzzleFlashEffect(
            state,
            ctx.muzzleWorld,
            ctx.baseDir,
            weaponConfig);

    SpawnMuzzleSmokeParticles(
            state,
            ctx.muzzleWorld,
            ctx.baseDir,
            weaponConfig);

    TriggerPlayerWeaponScreenShake(state, weaponConfig);

    if (weaponConfig.tracerStyle == TopdownTracerStyle::Handgun) {
        PlaySoundById(state, "pistol_shot", RandomRangeFloat(0.96f, 1.04f));
    } else if (weaponConfig.tracerStyle == TopdownTracerStyle::Shotgun) {
        PlaySoundById(state, "shotgun_shot", RandomRangeFloat(0.97f, 1.03f));
    }
}

static void AccumulatePendingDoorShot(
        std::vector<PendingDoorShotResult>& pendingDoorHits,
        TopdownRuntimeDoor* door,
        Vector2 hitPoint,
        Vector2 hitNormal,
        Vector2 hitDir)
{
    PendingDoorShotResult* existing = nullptr;

    for (PendingDoorShotResult& pending : pendingDoorHits) {
        if (pending.door == door) {
            existing = &pending;
            break;
        }
    }

    if (existing == nullptr) {
        PendingDoorShotResult pending;
        pending.door = door;
        pending.hitCount = 1;
        pending.hitPoint = hitPoint;
        pending.hitNormal = hitNormal;
        pending.hitDir = hitDir;
        pending.hasHit = true;
        pendingDoorHits.push_back(pending);
    } else {
        existing->hitCount++;
        existing->hitPoint = hitPoint;
        existing->hitNormal = hitNormal;
        existing->hitDir = hitDir;
    }
}

static void AccumulatePendingNpcShot(
        std::vector<PendingNpcShotResult>& pendingNpcHits,
        TopdownNpcRuntime* npc,
        float damage,
        Vector2 hitPoint,
        Vector2 hitDir)
{
    PendingNpcShotResult* existing = nullptr;

    for (PendingNpcShotResult& pending : pendingNpcHits) {
        if (pending.handle == npc->handle) {
            existing = &pending;
            break;
        }
    }

    if (existing == nullptr) {
        PendingNpcShotResult pending;
        pending.handle = npc->handle;
        pending.npc = npc;
        pending.totalDamage = damage;
        pending.hitPoint = hitPoint;
        pending.hitDir = hitDir;
        pending.hasHit = true;
        pendingNpcHits.push_back(pending);
    } else {
        existing->totalDamage += damage;
        existing->hitDir = hitDir;
    }
}

static void CollectPlayerRangedHits(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        const PlayerRangedAttackContext& ctx,
        std::vector<PendingDoorShotResult>& outDoorHits,
        std::vector<PendingNpcShotResult>& outNpcHits)
{
    const int shotCount =
            (weaponConfig.tracerStyle == TopdownTracerStyle::Shotgun)
            ? std::max(1, weaponConfig.pelletCount)
            : 1;

    outNpcHits.clear();
    outNpcHits.reserve(shotCount);

    outDoorHits.clear();
    outDoorHits.reserve(shotCount);

    static constexpr float kTracerForwardOffset = 0.0f;

    for (int i = 0; i < shotCount; ++i) {
        const Vector2 shotDir =
                ComputeShotDirectionWithSpread(ctx.baseDir, weaponConfig.spreadDegrees);

        const Vector2 tracerStart =
                TopdownAdd(ctx.muzzleWorld, TopdownMul(shotDir, kTracerForwardOffset));

        const TopdownShotHitResult hit =
                FindFirstHitscanHit(state, ctx.muzzleWorld, shotDir, weaponConfig.maxRange);

        const TopdownShotHitResult playerOriginHit =
                FindFirstHitscanHit(
                        state,
                        state.topdown.runtime.player.position,
                        shotDir,
                        weaponConfig.maxRange);

        const TopdownShotHitResult effectiveHit =
                IsShotBlockedFromPlayerOrigin(hit, playerOriginHit)
                ? playerOriginHit
                : hit;

        AppendPlayerTracerEffect(
                state,
                tracerStart,
                effectiveHit.point,
                weaponConfig.tracerStyle);

        if (effectiveHit.type == TopdownShotHitType::Wall) {
            SpawnWallImpactParticles(
                    state,
                    effectiveHit.point,
                    effectiveHit.normal,
                    weaponConfig);
            continue;
        }

        if (effectiveHit.type == TopdownShotHitType::Door && effectiveHit.door != nullptr) {
            AccumulatePendingDoorShot(
                    outDoorHits,
                    effectiveHit.door,
                    effectiveHit.point,
                    effectiveHit.normal,
                    shotDir);
            continue;
        }

        if (effectiveHit.type == TopdownShotHitType::Npc && effectiveHit.npc != nullptr) {
            AccumulatePendingNpcShot(
                    outNpcHits,
                    effectiveHit.npc,
                    weaponConfig.rangedDamage,
                    effectiveHit.point,
                    shotDir);
        }
    }
}

static void ApplyPendingDoorShots(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        const std::vector<PendingDoorShotResult>& pendingDoorHits)
{
    for (const PendingDoorShotResult& pending : pendingDoorHits) {
        if (!pending.hasHit || pending.door == nullptr) {
            continue;
        }

        SpawnWallImpactParticles(
                state,
                pending.hitPoint,
                pending.hitNormal,
                weaponConfig);

        ApplyDoorBallisticImpulse(
                *pending.door,
                pending.hitPoint,
                pending.hitDir,
                weaponConfig.rangedDoorImpulse * static_cast<float>(pending.hitCount));
    }
}

static void ApplyPendingNpcShots(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        const std::vector<PendingNpcShotResult>& pendingNpcHits)
{
    for (const PendingNpcShotResult& pending : pendingNpcHits) {
        if (!pending.hasHit || pending.npc == nullptr) {
            continue;
        }

        const TopdownNpcDamageResult damageResult =
                ApplyDamageToNpc(
                        state,
                        *pending.npc,
                        pending.totalDamage);

        if (!damageResult.validTarget || damageResult.damageApplied <= 0.0f) {
            continue;
        }

        TopdownAlertNpcToPlayer(state, *pending.npc);

        {
            const float nearbyAlertRadius =
                    std::max(180.0f, pending.npc->hearingRange);
            TopdownAlertNearbyNpcs(state, *pending.npc, nearbyAlertRadius);
        }

        if (damageResult.killed) {
            BeginNpcDeath(
                    state,
                    *pending.npc,
                    pending.hitDir,
                    weaponConfig.rangedKnockback);
        } else {
            ApplyNpcHitReaction(
                    state,
                    *pending.npc,
                    pending.hitDir,
                    weaponConfig.rangedKnockback);
        }

        SpawnBloodImpactParticles(
                state,
                pending.hitPoint,
                pending.hitDir,
                weaponConfig);

        QueueBloodSpatterDecals(
                state,
                pending.hitPoint,
                pending.hitDir,
                weaponConfig);
    }
}

static void StartPlayerMeleeAttack(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    attack.meleeHitPending = true;
    attack.meleeHitApplied = false;

    if (weaponConfig.equipmentSetId == "knife") {
        PlaySoundById(state, "knife_swing", RandomRangeFloat(0.95f, 1.05f));
    } else {
        PlaySoundById(state, "melee_hit", RandomRangeFloat(0.95f, 1.05f));
    }
}

static void ResolvePlayerMeleeNpcHit(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        TopdownNpcRuntime& hitNpc,
        Vector2 hitDir)
{
    const TopdownNpcDamageResult damageResult =
            ApplyDamageToNpc(
                    state,
                    hitNpc,
                    weaponConfig.meleeDamage);

    if (damageResult.validTarget && damageResult.damageApplied > 0.0f) {
        TopdownAlertNpcToPlayer(state, hitNpc);

        {
            const float nearbyAlertRadius =
                    std::max(180.0f, hitNpc.hearingRange);
            TopdownAlertNearbyNpcs(state, hitNpc, nearbyAlertRadius);
        }

        if (damageResult.killed) {
            BeginNpcDeath(
                    state,
                    hitNpc,
                    hitDir,
                    weaponConfig.meleeKnockback);
        } else {
            ApplyNpcHitReaction(
                    state,
                    hitNpc,
                    hitDir,
                    weaponConfig.meleeKnockback);
        }
    }

    if (weaponConfig.equipmentSetId == "knife" &&
        damageResult.validTarget &&
        damageResult.damageApplied > 0.0f) {
        Vector2 bloodOrigin{};
        bool hasAuthoredBloodOrigin = false;

        const std::string attackAnimationId =
                FindTopdownPlayerEquipmentAttackAnimationId(
                        state,
                        weaponConfig.equipmentSetId,
                        TopdownAttackType::Melee);

        if (!attackAnimationId.empty()) {
            hasAuthoredBloodOrigin =
                    TryComputePlayerAttackAnimationMuzzleWorldPosition(
                            state,
                            attackAnimationId,
                            bloodOrigin);
        }

        if (!hasAuthoredBloodOrigin) {
            Vector2 toNpc = TopdownNormalizeOrZero(
                    TopdownSub(hitNpc.position, state.topdown.runtime.player.position));

            if (TopdownLengthSqr(toNpc) <= 0.000001f) {
                toNpc = hitDir;
            }

            bloodOrigin = TopdownSub(
                    hitNpc.position,
                    TopdownMul(toNpc, hitNpc.collisionRadius * 0.65f));
        }

        SpawnBloodImpactParticles(
                state,
                bloodOrigin,
                hitDir,
                weaponConfig);

        QueueBloodSpatterDecals(
                state,
                bloodOrigin,
                hitDir,
                weaponConfig);
    }
}

static void ResolvePlayerMeleeDoorHit(
        GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        Vector2 attackDir)
{
    if (weaponConfig.meleeDoorImpulse <= 0.0f) {
        return;
    }

    TopdownRuntimeDoor* hitDoor = nullptr;
    Vector2 hitPoint{};
    Vector2 hitNormal{};
    float hitDistance = weaponConfig.meleeRange;

    if (!RaycastClosestDoor(
            state,
            state.topdown.runtime.player.position,
            attackDir,
            weaponConfig.meleeRange,
            hitDoor,
            hitPoint,
            hitNormal,
            hitDistance)) {
        return;
    }

    SpawnWallImpactParticles(
            state,
            hitPoint,
            hitNormal,
            weaponConfig);

    ApplyDoorBallisticImpulse(
            *hitDoor,
            hitPoint,
            attackDir,
            weaponConfig.meleeDoorImpulse);
}

static void ResolvePendingPlayerMeleeImpact(GameState& state)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    const TopdownPlayerWeaponConfig* weaponConfig =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(
                    state,
                    attack.equipmentSetId);

    if (weaponConfig == nullptr) {
        attack.meleeHitApplied = true;
        attack.meleeHitPending = false;
        return;
    }

    TopdownNpcRuntime* hitNpc =
            FindBestPlayerMeleeTarget(
                    state,
                    weaponConfig->meleeRange,
                    weaponConfig->meleeArcDegrees);

    const Vector2 attackDir = BuildPlayerAttackAimDirection(state);

    if (hitNpc != nullptr) {
        Vector2 hitDir = TopdownNormalizeOrZero(
                TopdownSub(hitNpc->position, state.topdown.runtime.player.position));

        if (TopdownLengthSqr(hitDir) <= 0.000001f) {
            hitDir = attackDir;
        }

        if (weaponConfig->equipmentSetId == "knife") {
            PlaySoundById(state, "knife_hit", RandomRangeFloat(0.95f, 1.05f));
        } else if (weaponConfig->equipmentSetId == "rifle" ||
                   weaponConfig->equipmentSetId == "shotgun") {
            PlaySoundById(state, "melee_hit", RandomRangeFloat(0.95f, 1.05f));
        }

        ResolvePlayerMeleeNpcHit(
                state,
                *weaponConfig,
                *hitNpc,
                hitDir);
    }

    ResolvePlayerMeleeDoorHit(
            state,
            *weaponConfig,
            attackDir);

    attack.meleeHitApplied = true;
    attack.meleeHitPending = false;
}

static void UpdatePlayerAttackTimers(TopdownPlayerAttackRuntime& attack, float dt)
{
    if (attack.cooldownRemainingMs > 0.0f) {
        attack.cooldownRemainingMs -= dt * 1000.0f;
        if (attack.cooldownRemainingMs < 0.0f) {
            attack.cooldownRemainingMs = 0.0f;
        }
    }

    if (attack.fullAutoShakeCooldownMs > 0.0f) {
        attack.fullAutoShakeCooldownMs -= dt * 1000.0f;
        if (attack.fullAutoShakeCooldownMs < 0.0f) {
            attack.fullAutoShakeCooldownMs = 0.0f;
        }
    }

    if (!attack.triggerHeld || attack.currentFireMode != TopdownFireMode::FullAuto) {
        attack.fullAutoShakeCooldownMs = 0.0f;
    }
}

static void UpdateActivePlayerAttack(GameState& state, float dt)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    if (!attack.active) {
        attack.rifleLoopPlaying = false;
        return;
    }

    attack.stateTimeMs += dt * 1000.0f;

    if (attack.meleeHitPending && !attack.meleeHitApplied) {
        const float durationMs = attack.animationDurationMs;

        if (durationMs > 0.0f) {
            const float triggerTime = durationMs * 0.75f;

            if (attack.stateTimeMs >= triggerTime) {
                ResolvePendingPlayerMeleeImpact(state);
            }
        }
    }

    const float durationMs = attack.animationDurationMs;

    if (durationMs <= 0.0f || attack.stateTimeMs >= durationMs) {
        attack.active = false;
        attack.state = TopdownPlayerAttackState::Idle;
        attack.attackType = TopdownAttackType::None;
        attack.stateTimeMs = 0.0f;
        attack.animationDurationMs = 0.0f;
    }
}

static bool TryStartPlayerAttack(
        GameState& state,
        TopdownAttackInput input)
{
    const TopdownPlayerWeaponConfig* weaponConfig = nullptr;
    TopdownAttackType attackType = TopdownAttackType::None;

    if (!BeginPlayerAttackRuntime(state, input, weaponConfig, attackType)) {
        return false;
    }

    if (attackType == TopdownAttackType::Ranged) {
        const PlayerRangedAttackContext ctx =
                BuildPlayerRangedAttackContext(state, *weaponConfig);

        PlayPlayerRangedAttackAudioAndFx(state, *weaponConfig, ctx);
        TopdownAlertNpcsByGunshot(state, ctx.muzzleWorld);

        std::vector<PendingNpcShotResult> pendingNpcHits;
        std::vector<PendingDoorShotResult> pendingDoorHits;

        CollectPlayerRangedHits(
                state,
                *weaponConfig,
                ctx,
                pendingDoorHits,
                pendingNpcHits);

        ApplyPendingDoorShots(
                state,
                *weaponConfig,
                pendingDoorHits);

        ApplyPendingNpcShots(
                state,
                *weaponConfig,
                pendingNpcHits);
    }

    if (attackType == TopdownAttackType::Melee) {
        StartPlayerMeleeAttack(state, *weaponConfig);
    }

    return true;
}

static void ConsumeQueuedPlayerAttackInputs(GameState& state)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    if (attack.pendingSecondaryAttack) {
        attack.pendingSecondaryAttack = false;
        TryStartPlayerAttack(state, TopdownAttackInput::Secondary);
    }

    if (attack.pendingPrimaryAttack) {
        attack.pendingPrimaryAttack = false;
        TryStartPlayerAttack(state, TopdownAttackInput::Primary);
    }

    if (attack.triggerHeld &&
        attack.currentFireMode == TopdownFireMode::FullAuto &&
        attack.cooldownRemainingMs <= 0.0f) {
        TryStartPlayerAttack(state, TopdownAttackInput::Primary);
    }
}

static void UpdatePlayerRifleLoopAudio(GameState& state)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    const TopdownPlayerWeaponConfig* weaponConfig =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, character.equippedSetId);

    if (weaponConfig != nullptr &&
        weaponConfig->tracerStyle == TopdownTracerStyle::Rifle &&
        attack.currentFireMode == TopdownFireMode::FullAuto) {
        if (attack.triggerHeld) {
            if (!attack.rifleLoopPlaying) {
                PlaySoundById(state, "rifle_full_auto_start", RandomRangeFloat(0.98f, 1.02f));
                PlaySoundById(state, "rifle_full_auto_loop", RandomRangeFloat(0.98f, 1.02f));
                attack.rifleLoopPlaying = true;
            }
        } else {
            if (attack.rifleLoopPlaying) {
                StopSoundById(state, "rifle_full_auto_loop");
                PlaySoundById(state, "rifle_full_auto_end");
                attack.rifleLoopPlaying = false;
            }
        }
    }
}

static void UpdatePlayerAttackRuntime(GameState& state, float dt)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    UpdatePlayerAttackTimers(attack, dt);
    UpdateActivePlayerAttack(state, dt);
    ConsumeQueuedPlayerAttackInputs(state);
    UpdatePlayerRifleLoopAudio(state);
}

void TopdownUpdate(GameState& state, float dt)
{
    if (!state.topdown.runtime.levelActive) {
        return;
    }

    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    TopdownRuntimeData& runtime = state.topdown.runtime;

    // --- trigger game over ---
    if (!runtime.gameOverActive &&
        player.lifeState == TopdownPlayerLifeState::Dead &&
        player.health <= 0.0f)
    {
        runtime.gameOverActive = true;
        runtime.gameOverElapsedMs = 0.0f;
        runtime.returnToMenuRequested = false;

        runtime.controlsEnabled = false;

        TraceLog(LOG_INFO, "Game Over triggered");
    }

    TopdownUpdateDoors(state, dt);

    TopdownUpdatePlayerLogic(state, dt);
    TopdownUpdateNpcLogic(state, dt);

    ApplyDoorMotionPushToPlayer(state, dt);
    ApplyDoorMotionPushToNpcs(state, dt);

    UpdatePlayerAttackRuntime(state, dt);

    TopdownUpdateLevelEffects(state, dt);

    TopdownUpdatePlayerAnimation(state, dt);
    TopdownUpdateNpcAnimation(state, dt);
    TopdownUpdateCamera(state, dt);
    {
        //ScopeTimer t("RebuildWallOcclusionPolygons");
        TopdownRebuildWallOcclusionPolygons(state.topdown, false);
    }

    // --- update game over ---
    if (runtime.gameOverActive) {
        runtime.gameOverElapsedMs += dt * 1000.0f;

        if (runtime.returnToMenuRequested) {
            TraceLog(LOG_INFO, "Returning to menu from game over");

            TopdownUnloadLevel(state);

            runtime = {}; // wipe runtime clean (important)

            state.topdown.currentLevelId.clear();
            state.topdown.currentLevelSaveName.clear();

            state.mode = GameMode::Menu;

            MenuInit(&state); // resets to "Start New Game" state

            return;
        }
    }
}
