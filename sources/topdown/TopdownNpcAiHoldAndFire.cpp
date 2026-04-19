#include "topdown/TopdownNpcAiHoldAndFire.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcInvestigation.h"
#include "topdown/NpcRegistry.h"
#include "topdown/LevelEffects.h"
#include "resources/AsepriteAsset.h"
#include "audio/Audio.h"
#include "TopdownNpcAiCommon.h"
#include "TopdownCombatData.h"
#include "TopdownCombatHelpers.h"
#include "LevelWindows.h"

// Give up hard chase and enter search if the player becomes implausibly far away.
constexpr const float kHardChaseCutoffDistance = 3500.0f;

static void FireNpcHitscanWeapon(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    if (asset == nullptr || !asset->loaded) {
        return;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    Vector2 toPlayer = TopdownSub(player.position, npc.position);
    Vector2 baseDir = TopdownNormalizeOrZero(toPlayer);

    if (TopdownLengthSqr(baseDir) <= 0.000001f) {
        baseDir = npc.facing;
    }

    if (TopdownLengthSqr(baseDir) <= 0.000001f) {
        baseDir = Vector2{1.0f, 0.0f};
    }

    const int pelletCount = std::max(1, asset->rangedPelletCount);

    for (int i = 0; i < pelletCount; ++i) {
        const float dist = TopdownLength(toPlayer);
        const float t = Clamp(dist / std::max(1.0f, asset->rangedMaxRange), 0.0f, 1.0f);

        const float spreadDegrees =
                Lerp(asset->aimInaccuracyMinDegrees,
                     asset->aimInaccuracyMaxDegrees,
                     t);

        const Vector2 shotDir =
                ComputeShotDirectionWithSpread(baseDir, spreadDegrees);

        const TopdownShotHitResult hit =
                FindFirstNpcHitscanHit(
                        state,
                        npc,
                        npc.position,
                        shotDir,
                        asset->rangedMaxRange);

        AppendPlayerTracerEffect(
                state,
                npc.position,
                hit.point,
                asset->rangedTracerStyle);

        /*
        // Make other npcs investigates this npcs gunshots
        TopdownPushWorldEvent(state,
                              TopdownWorldEventType::Gunshot,
                              npc.position,
                              1000,
                              TopdownWorldEventSourceType::Npc, -1);
                              */

        if (hit.type == TopdownShotHitType::Wall) {
            SpawnWallImpactParticles(
                    state,
                    hit.point,
                    hit.normal,
                    state.topdown.playerCharacterAsset.weaponConfigs[0]); // temporary reuse
            continue;
        }

        if (hit.type == TopdownShotHitType::Door && hit.door != nullptr) {
            SpawnWallImpactParticles(
                    state,
                    hit.point,
                    hit.normal,
                    state.topdown.playerCharacterAsset.weaponConfigs[0]); // temporary reuse
            continue;
        }

        if (hit.type == TopdownShotHitType::Window && hit.window != nullptr) {
            BreakWindow(
                    state,
                    *hit.window,
                    hit.point,
                    shotDir);
            continue;
        }

        if (hit.type == TopdownShotHitType::Player) {
            TopdownApplyDamageToPlayer(
                    state,
                    asset->attackDamage,
                    npc.position);

            if (!npc.attackConnectSoundId.empty()) {
                PlaySoundById(
                        state,
                        npc.attackConnectSoundId,
                        RandomRangeFloat(0.95f, 1.05f));
            }

            SpawnBloodImpactParticles(
                    state,
                    hit.point,
                    shotDir,
                    asset->attackEffects);

            QueueBloodSpatterDecals(
                    state,
                    hit.point,
                    shotDir,
                    asset->attackEffects);

            continue;
        }

        if (hit.type == TopdownShotHitType::Npc && hit.npc != nullptr) {
            // First pass: other NPCs block the shot.
            continue;
        }

        if (hit.type == TopdownShotHitType::None) {
            continue;
        }
    }
}

static void StartNpcRangedAttack(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    if (asset == nullptr || !asset->loaded) {
        return;
    }

    TopdownStopNpcMovement(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);

    npc.combatState = TopdownNpcCombatState::Attack;
    npc.attackStateTimeMs = 0.0f;

    FireNpcHitscanWeapon(state, npc);

    npc.attackCooldownRemainingMs =
            std::max(npc.attackCooldownRemainingMs, npc.attackCooldownMs);

    if (TopdownNpcClipRefIsValid(asset->rangedAttackClip)) {
        TopdownPlayNpcOneShotAnimation(npc, asset->rangedAttackClip);
        npc.attackAnimationDurationMs =
                TopdownGetNpcClipDurationMs(state, asset->rangedAttackClip);
    } else {
        npc.attackAnimationDurationMs = std::max(1.0f, npc.attackRecoverMs);
    }

    if (npc.attackAnimationDurationMs <= 0.0f) {
        npc.attackAnimationDurationMs = std::max(1.0f, npc.attackRecoverMs);
    }

    if (!npc.attackStartSoundId.empty()) {
        PlaySoundById(
                state,
                npc.attackStartSoundId,
                RandomRangeFloat(0.95f, 1.05f));
    }
}

static void UpdateNpcAttackCooldown(TopdownNpcRuntime& npc, float dtMs)
{
    if (npc.attackCooldownRemainingMs > 0.0f) {
        npc.attackCooldownRemainingMs -= dtMs;
        if (npc.attackCooldownRemainingMs < 0.0f) {
            npc.attackCooldownRemainingMs = 0.0f;
        }
    }
}

static void UpdateNpcRepathTimer(TopdownNpcRuntime& npc, float dtMs)
{
    if (npc.repathTimerMs > 0.0f) {
        npc.repathTimerMs -= dtMs;
        if (npc.repathTimerMs < 0.0f) {
            npc.repathTimerMs = 0.0f;
        }
    }
}

static bool HandleNpcMotionInterrupts(TopdownNpcRuntime& npc)
{
    if (npc.hurtStunRemainingMs > 0.0f) {
        TopdownResetNpcChaseStuckWatchdog(npc);
        npc.currentVelocity = Vector2{};
        return true;
    }

    if (TopdownLengthSqr(npc.knockbackVelocity) > 0.000001f) {
        TopdownResetNpcChaseStuckWatchdog(npc);
        return true;
    }

    return false;
}

static void UpdateNpcFacingTowardPlayer(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const Vector2 toPlayer = TopdownSub(player.position, npc.position);

    if (TopdownLengthSqr(toPlayer) <= 0.000001f) {
        return;
    }

    const Vector2 facing = TopdownNormalizeOrZero(toPlayer);
    npc.facing = facing;
    npc.rotationRadians = std::atan2(facing.y, facing.x);
}

static bool TryNpcAttackOrRecover(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlySeesPlayer)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const bool inAttackRange =
            TopdownIsPlayerWithinNpcAttackRange(npc, player);

    if (!(currentlySeesPlayer && inAttackRange)) {
        return false;
    }

    TopdownStopNpcMovement(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);

    if (npc.attackCooldownRemainingMs <= 0.0f) {
        StartNpcRangedAttack(state, npc);
    }

    return true;
}

static void UpdateNpcChasePathing(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 chaseTarget)
{
    if (!npc.move.active || npc.repathTimerMs <= 0.0f) {
        TopdownBuildNpcPathToTarget(
                state,
                npc,
                chaseTarget,
                TopdownNpcMoveOwner::Ai);

        npc.repathTimerMs = std::max(1.0f, npc.chaseRepathIntervalMs);
    }
}

static void StopNpcEngagedExecution(TopdownNpcRuntime& npc)
{
    npc.hasPlayerTarget = false;
    TopdownResetNpcChaseStuckWatchdog(npc);
    npc.combatState = TopdownNpcCombatState::None;
    TopdownStopNpcMovement(npc);
    TopdownResetNpcSearchTimers(npc);
}

void TopdownNpcAiHoldAndFire_UpdateInvestigating(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt)
{
    if (npc.combatState != TopdownNpcCombatState::Investigation &&
        npc.combatState != TopdownNpcCombatState::Search) {
        if (!TopdownBeginNpcInvestigationState(state, npc)) {
            TopdownBeginNpcSearchState(npc);
        }
        return;
    }

    if (npc.combatState == TopdownNpcCombatState::Investigation) {
        const TopdownNpcInvestigationUpdateResult result =
                TopdownUpdateNpcInvestigationState(state, npc, dt);

        switch (result) {
            case TopdownNpcInvestigationUpdateResult::Running:
                return;

            case TopdownNpcInvestigationUpdateResult::Arrived:
                TraceLog(LOG_INFO, "Investigation ended: arrived");
                TopdownBeginNpcSearchState(npc);
                return;

            case TopdownNpcInvestigationUpdateResult::Failed:
                TraceLog(LOG_INFO, "Investigation ended: failed");
                TopdownBeginNpcSearchState(npc);
                return;
        }
    }

    if (npc.combatState == TopdownNpcCombatState::Search) {
        const TopdownNpcSearchUpdateResult result =
                TopdownUpdateNpcSearchState(npc, dt);

        if (result == TopdownNpcSearchUpdateResult::Finished) {
            TopdownResetNpcSearchTimers(npc);
            npc.hasPlayerTarget = false;
            npc.combatState = TopdownNpcCombatState::None;
            npc.engagementState = TopdownNpcEngagementState::Unaware;
            TraceLog(LOG_INFO, "Must have been the wind...");
        }
        return;
    }

    TraceLog(LOG_WARNING, "Investigating execution ended with unexpected combat state");
}

void TopdownNpcAiHoldAndFire_UpdateEngaged(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt)
{
    if (npc.dead || npc.corpse || !npc.hostile) {
        return;
    }

    if (!TopdownIsPlayerAlive(state)) {
        StopNpcEngagedExecution(npc);
        return;
    }

    const float dtMs = dt * 1000.0f;

    UpdateNpcAttackCooldown(npc, dtMs);
    UpdateNpcRepathTimer(npc, dtMs);

    if (HandleNpcMotionInterrupts(npc)) {
        return;
    }

    if (npc.combatState == TopdownNpcCombatState::Attack) {
        npc.attackStateTimeMs += dtMs;

        if (npc.attackStateTimeMs >= npc.attackAnimationDurationMs) {
            npc.combatState = TopdownNpcCombatState::None;
            npc.attackStateTimeMs = 0.0f;
            npc.attackAnimationDurationMs = 0.0f;
        }
        return;
    }

    if (perception.seesPlayer) {
        UpdateNpcFacingTowardPlayer(state, npc);
    }

    if (TryNpcAttackOrRecover(state, npc, perception.seesPlayer)) {
        return;
    }

    if (npc.combatState != TopdownNpcCombatState::Chase) {
        npc.combatState = TopdownNpcCombatState::Chase;
        TopdownResetNpcChaseStuckWatchdog(npc);
        return;
    }

    Vector2 chaseTarget{};
    bool hasChaseTarget = false;

    if (perception.seesPlayer) {
        const TopdownPlayerRuntime& player = state.topdown.runtime.player;

        const Vector2 toPlayer = TopdownSub(player.position, npc.position);
        const float centerDist = TopdownLength(toPlayer);
        const float edgeDist = centerDist - player.radius - npc.collisionRadius;

        const float desiredDistance = npc.attackRange * 0.8f;

        if (edgeDist > desiredDistance) {
            chaseTarget = player.position;
            hasChaseTarget = true;
        } else {
            TopdownStopNpcMovement(npc);
            TopdownResetNpcChaseStuckWatchdog(npc);
            hasChaseTarget = false;
        }
    } else if (npc.hasPlayerTarget) {
        chaseTarget = npc.lastKnownPlayerPosition;
        hasChaseTarget = true;
    }

    if (npc.persistentChase) {
        chaseTarget = state.topdown.runtime.player.position;
        hasChaseTarget = true;
    }

    if (npc.persistentChase && hasChaseTarget) {
        const float distanceToChaseTarget =
                TopdownLength(TopdownSub(chaseTarget, npc.position));

        if (distanceToChaseTarget >= kHardChaseCutoffDistance) {
            npc.hasPlayerTarget = false;
            npc.combatState = TopdownNpcCombatState::None;
            npc.engagementState = TopdownNpcEngagementState::Unaware;
            TopdownStopNpcMovement(npc);
            return;
        }
    }

    if (TopdownUpdateNpcChaseStuckWatchdog(npc, dtMs)) {
        npc.hasPlayerTarget = false;
        npc.combatState = TopdownNpcCombatState::None;
        npc.engagementState = TopdownNpcEngagementState::Unaware;
        TopdownStopNpcMovement(npc);
        TopdownResetNpcChaseStuckWatchdog(npc);
        return;
    }

    if (hasChaseTarget) {
        UpdateNpcChasePathing(state, npc, chaseTarget);
    }
}