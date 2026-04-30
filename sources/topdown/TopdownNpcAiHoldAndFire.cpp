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

enum class HoldAndFireCombatIntent {
    HoldPosition,
    Attack,
    Approach,
    BackOff,
    Strafe
};

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

    Vector2 muzzleWorld{};
    if (!TopdownComputeNpcMuzzleWorldPosition(state, npc, *asset, muzzleWorld)) {
        muzzleWorld = npc.position;
    }
    const int pelletCount = std::max(1, asset->rangedPelletCount);

    SpawnMuzzleFlashEffectAnchoredToNpc(
            state,
            npc.handle,
            muzzleWorld,
            baseDir,
            asset->muzzleEffects);

    SpawnMuzzleSmokeParticles(
            state,
            muzzleWorld,
            baseDir,
            asset->muzzleEffects);


    // Make other npcs investigates this npcs gunshots

    TopdownPushWorldEvent(state,
                          TopdownWorldEventType::Gunshot,
                          npc.position,
                          1000,
                          TopdownWorldEventSourceType::Npc, -1);

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
                        muzzleWorld,
                        shotDir,
                        asset->rangedMaxRange);


        AppendTracerEffectAnchoredToNpc(
                state,
                npc.handle,
                muzzleWorld,
                hit.point,
                asset->rangedTracerStyle);


        if (hit.type == TopdownShotHitType::Wall) {
            SpawnWallImpactParticles(
                    state,
                    hit.point,
                    hit.normal,
                    asset->ballisticImpactEffects);
            continue;
        }

        if (hit.type == TopdownShotHitType::Door && hit.door != nullptr) {
            SpawnWallImpactParticles(
                    state,
                    hit.point,
                    hit.normal,
                    asset->ballisticImpactEffects);
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
                AudioPlaySoundAtPosition(
                        state,
                        npc.attackConnectSoundId,
                        npc.position,
                        AUDIO_RADIUS_NPC_WEAPON,
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
        AudioPlaySoundAtPosition(
                state,
                npc.attackStartSoundId,
                npc.position,
                AUDIO_RADIUS_NPC_WEAPON,
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
        if (TopdownBeginNpcInvestigationState(state, npc)) {
            TraceLog(
                    LOG_INFO,
                    "HoldAndFire began investigation successfully (combatState=%d)",
                    static_cast<int>(npc.combatState));
        } else {
            TraceLog(LOG_WARNING, "HoldAndFire failed to begin investigation");
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

    if (npc.combatState == TopdownNpcCombatState::Investigation ||
        npc.engagementState == TopdownNpcEngagementState::Investigating) {
        return;
    }

    const float dtMs = dt * 1000.0f;

    npc.strafeTimerMs -= dtMs;
    if (npc.strafeTimerMs <= 0.0f) {
        npc.strafeDir *= -1;
        npc.strafeTimerMs = RandomRangeFloat(600.0f, 1400.0f);
    }

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

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    if (asset == nullptr || !asset->loaded) {
        StopNpcEngagedExecution(npc);
        return;
    }

    if (perception.seesPlayer) {
        UpdateNpcFacingTowardPlayer(state, npc);
    }

    const Vector2 toPlayer = TopdownSub(player.position, npc.position);
    const float centerDist = TopdownLength(toPlayer);
    const float edgeDist = centerDist - player.radius - npc.collisionRadius;

    const bool inAttackRange =
            edgeDist <= asset->rangedMaxRange;

    bool clearShot = false;
    if (perception.seesPlayer) {
        Vector2 muzzleWorld{};
        if (!TopdownComputeNpcMuzzleWorldPosition(state, npc, *asset, muzzleWorld)) {
            muzzleWorld = npc.position;
        }

        clearShot = !TopdownIsNpcShotBlockedByOtherNpc(
                state,
                npc,
                muzzleWorld,
                player.position,
                asset->rangedMaxRange);
    }

    const bool canTakeShot =
            perception.seesPlayer &&
            inAttackRange &&
            clearShot;

    HoldAndFireCombatIntent intent = HoldAndFireCombatIntent::HoldPosition;
    Vector2 intentTarget{};
    bool hasIntentTarget = false;
    bool forceWalk = false;

    if (canTakeShot && npc.attackCooldownRemainingMs <= 0.0f) {
        intent = HoldAndFireCombatIntent::Attack;
    } else if (perception.seesPlayer) {
        const float desiredDistance =
                npc.attackRange * npc.preferredAttackRangeFactor;

        const float tolerance = 20.0f;

        if (!inAttackRange) {
            intent = HoldAndFireCombatIntent::Approach;
            intentTarget = player.position;
            hasIntentTarget = true;
        } else if (!clearShot) {
            intent = HoldAndFireCombatIntent::Strafe;
        } else if (edgeDist > desiredDistance + tolerance) {
            intent = HoldAndFireCombatIntent::Approach;
            intentTarget = player.position;
            hasIntentTarget = true;
        } else if (edgeDist < desiredDistance - tolerance) {
            const Vector2 awayDir =
                    TopdownNormalizeOrZero(TopdownSub(npc.position, player.position));

            if (TopdownLengthSqr(awayDir) > 0.000001f) {
                intent = HoldAndFireCombatIntent::BackOff;
                intentTarget = TopdownAdd(npc.position, TopdownMul(awayDir, 100.0f));
                hasIntentTarget = true;
                forceWalk = true;
            } else {
                intent = HoldAndFireCombatIntent::HoldPosition;
            }
        } else {
            intent = HoldAndFireCombatIntent::HoldPosition;
        }
    } else if (npc.hasPlayerTarget) {
        intent = HoldAndFireCombatIntent::Approach;
        intentTarget = npc.lastKnownPlayerPosition;
        hasIntentTarget = true;
    }

    if (npc.persistentChase) {
        intent = HoldAndFireCombatIntent::Approach;
        intentTarget = state.topdown.runtime.player.position;
        hasIntentTarget = true;
        forceWalk = false;
    }

    if (npc.persistentChase && hasIntentTarget) {
        const float distanceToChaseTarget =
                TopdownLength(TopdownSub(intentTarget, npc.position));

        if (distanceToChaseTarget >= kHardChaseCutoffDistance) {
            npc.hasPlayerTarget = false;
            npc.combatState = TopdownNpcCombatState::None;
            npc.engagementState = TopdownNpcEngagementState::Unaware;
            TopdownStopNpcMovement(npc);
            return;
        }
    }

    switch (intent) {
        case HoldAndFireCombatIntent::Attack:
            StartNpcRangedAttack(state, npc);
            return;

        case HoldAndFireCombatIntent::HoldPosition:
            TopdownStopNpcMovement(npc);
            TopdownResetNpcChaseStuckWatchdog(npc);
            return;

        case HoldAndFireCombatIntent::Strafe:
        {
            const Vector2 forward = TopdownNormalizeOrZero(toPlayer);
            if (TopdownLengthSqr(forward) <= 0.000001f) {
                TopdownStopNpcMovement(npc);
                TopdownResetNpcChaseStuckWatchdog(npc);
                return;
            }

            const Vector2 strafeLeft{ -forward.y, forward.x };
            const Vector2 strafeRight{ forward.y, -forward.x };

            const Vector2 strafeDirVec =
                    (npc.strafeDir > 0) ? strafeRight : strafeLeft;

            intentTarget = TopdownAdd(npc.position, TopdownMul(strafeDirVec, 120.0f));
            hasIntentTarget = true;
            forceWalk = true;
            break;
        }

        case HoldAndFireCombatIntent::Approach:
        case HoldAndFireCombatIntent::BackOff:
            break;
    }

    if (npc.combatState != TopdownNpcCombatState::Chase) {
        npc.combatState = TopdownNpcCombatState::Chase;
        TopdownResetNpcChaseStuckWatchdog(npc);
        return;
    }

    if (TopdownUpdateNpcChaseStuckWatchdog(npc, dtMs)) {
        npc.hasPlayerTarget = false;
        npc.combatState = TopdownNpcCombatState::None;
        npc.engagementState = TopdownNpcEngagementState::Unaware;
        TopdownStopNpcMovement(npc);
        TopdownResetNpcChaseStuckWatchdog(npc);
        return;
    }

    if (hasIntentTarget) {
        UpdateNpcChasePathing(state, npc, intentTarget);

        if (forceWalk &&
            npc.move.active &&
            npc.move.owner == TopdownNpcMoveOwner::Ai) {
            npc.move.running = false;
            npc.running = false;
        }
    }
}
