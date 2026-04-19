#include "TopdownCombatHelpers.h"
#include "LevelCollision.h"
#include "LevelWindows.h"
#include "LevelDoors.h"
#include "NpcRegistry.h"
#include "TopdownRvo.h"
#include "NpcUpdate.h"
#include "audio/Audio.h"
#include "TopdownNpcAiCommon.h"

Vector2 ComputeShotDirectionWithSpread(
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

TopdownShotHitResult FindFirstHitscanHit(
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
    result.window = nullptr;

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

    TopdownRuntimeWindow* hitWindow = nullptr;
    Vector2 windowPoint{};
    Vector2 windowNormal{};
    float windowDistance = result.distance;

    if (RaycastClosestWindow(
            state,
            origin,
            dir,
            result.distance,
            hitWindow,
            windowPoint,
            windowNormal,
            windowDistance)) {
        result.type = TopdownShotHitType::Window;
        result.npc = nullptr;
        result.door = nullptr;
        result.window = hitWindow;
        result.point = windowPoint;
        result.normal = windowNormal;
        result.distance = windowDistance;
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
        result.door = nullptr;
        result.window = nullptr;
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
        result.door = nullptr;
        result.window = nullptr;
        result.point = wallPoint;
        result.normal = wallNormal;
        result.distance = wallDistance;
    }

    return result;
}

TopdownNpcDamageResult ApplyDamageToNpc(
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

void BeginNpcDeath(
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

void ApplyNpcHitReaction(
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


TopdownShotHitResult FindFirstNpcHitscanHit(
        GameState& state,
        const TopdownNpcRuntime& shooter,
        Vector2 origin,
        Vector2 dir,
        float maxRange)
{
    TopdownShotHitResult result;
    result.type = TopdownShotHitType::None;
    result.npc = nullptr;
    result.door = nullptr;
    result.window = nullptr;
    result.point = TopdownAdd(origin, TopdownMul(dir, maxRange));
    result.normal = Vector2{};
    result.distance = maxRange;

    // -------------------------------------------------
    // Player
    // -------------------------------------------------
    {
        float hitDistance = 0.0f;
        Vector2 hitPoint{};
        Vector2 hitNormal{};

        if (RaycastPlayerDetailed(
                state,
                origin,
                dir,
                result.distance,
                hitDistance,
                hitPoint,
                hitNormal)) {
            result.type = TopdownShotHitType::Player;
            result.npc = nullptr;
            result.door = nullptr;
            result.window = nullptr;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    // -------------------------------------------------
    // Other NPCs
    // -------------------------------------------------
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible || npc.dead) {
            continue;
        }

        if (npc.handle == shooter.handle) {
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
            result.door = nullptr;
            result.window = nullptr;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    // -------------------------------------------------
    // Doors
    // -------------------------------------------------
    {
        TopdownRuntimeDoor* hitDoor = nullptr;
        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = result.distance;

        if (RaycastClosestDoor(
                state,
                origin,
                dir,
                result.distance,
                hitDoor,
                hitPoint,
                hitNormal,
                hitDistance)) {
            result.type = TopdownShotHitType::Door;
            result.npc = nullptr;
            result.door = hitDoor;
            result.window = nullptr;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    // -------------------------------------------------
    // Windows
    // -------------------------------------------------
    {
        TopdownRuntimeWindow* hitWindow = nullptr;
        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = result.distance;

        if (RaycastClosestWindow(
                state,
                origin,
                dir,
                result.distance,
                hitWindow,
                hitPoint,
                hitNormal,
                hitDistance)) {
            result.type = TopdownShotHitType::Window;
            result.npc = nullptr;
            result.door = nullptr;
            result.window = hitWindow;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    // -------------------------------------------------
    // Walls / blockers
    // -------------------------------------------------
    {
        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = result.distance;

        if (RaycastClosestSegmentWithNormal(
                origin,
                dir,
                state.topdown.runtime.collision.visionSegments,
                result.distance,
                hitPoint,
                hitNormal,
                hitDistance)) {
            result.type = TopdownShotHitType::Wall;
            result.npc = nullptr;
            result.door = nullptr;
            result.window = nullptr;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    {
        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = result.distance;

        if (RaycastClosestSegmentWithNormal(
                origin,
                dir,
                state.topdown.runtime.collision.boundarySegments,
                result.distance,
                hitPoint,
                hitNormal,
                hitDistance)) {
            result.type = TopdownShotHitType::Wall;
            result.npc = nullptr;
            result.door = nullptr;
            result.window = nullptr;
            result.point = hitPoint;
            result.normal = hitNormal;
            result.distance = hitDistance;
        }
    }

    return result;
}
