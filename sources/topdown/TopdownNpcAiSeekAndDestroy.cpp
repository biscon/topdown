#include "topdown/TopdownNpcAiSeekAndDestroy.h"
#include "topdown/TopdownNpcAi.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcInvestigation.h"
#include "topdown/NpcRegistry.h"
#include "topdown/LevelEffects.h"
#include "resources/AsepriteAsset.h"
#include "audio/Audio.h"
#include "raymath.h"
#include "LevelCollision.h"
#include "TopdownNpcAiCommon.h"

// Give up hard chase and enter search if the player becomes implausibly far away.
constexpr const float kHardChaseCutoffDistance = 3500.0f;

static bool TryBuildNpcMeleeHitWorldPosition(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        Vector2& outWorldPos)
{
    const TopdownNpcClipRef* clipRef = nullptr;

    if (npc.oneShotActive && TopdownNpcClipRefIsValid(npc.oneShotClip)) {
        clipRef = &npc.oneShotClip;
    } else if (TopdownNpcClipRefIsValid(npc.automaticLoopClip)) {
        clipRef = &npc.automaticLoopClip;
    }

    if (clipRef == nullptr || !TopdownNpcClipRefIsValid(*clipRef)) {
        return false;
    }

    const SpriteAssetResource* sprite =
            FindSpriteAssetResource(state.resources, clipRef->spriteHandle);

    if (sprite == nullptr || !sprite->loaded || !sprite->hasExplicitOrigin) {
        return false;
    }

    const float drawScale = sprite->baseDrawScale;

    const float localX = (npc.meleeHitPosX - sprite->origin.x) * drawScale;
    const float localY = (npc.meleeHitPosY - sprite->origin.y) * drawScale;

    const float radians = npc.rotationRadians;
    const Vector2 forward{ std::cos(radians), std::sin(radians) };
    const Vector2 right{ -forward.y, forward.x };

    outWorldPos = TopdownAdd(
            npc.position,
            TopdownAdd(
                    TopdownMul(forward, localX),
                    TopdownMul(right, localY)));

    return true;
}

static Vector2 BuildNpcMeleeFallbackImpactPointOnPlayer(
        const GameState& state,
        const TopdownNpcRuntime& npc)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    Vector2 toPlayer = TopdownNormalizeOrZero(TopdownSub(player.position, npc.position));
    if (TopdownLengthSqr(toPlayer) <= 0.000001f) {
        toPlayer = npc.facing;
    }
    if (TopdownLengthSqr(toPlayer) <= 0.000001f) {
        toPlayer = Vector2{ std::cos(npc.rotationRadians), std::sin(npc.rotationRadians) };
    }
    if (TopdownLengthSqr(toPlayer) <= 0.000001f) {
        toPlayer = Vector2{1.0f, 0.0f};
    }

    return TopdownAdd(
            player.position,
            TopdownMul(toPlayer, -player.radius * 0.55f));
}

static void StartNpcAttack(
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
    npc.attackHitPending = true;
    npc.attackHitApplied = false;

    if (TopdownNpcClipRefIsValid(asset->meleeAttackClip)) {
        TopdownPlayNpcOneShotAnimation(npc, asset->meleeAttackClip);
        npc.attackAnimationDurationMs =
                TopdownGetNpcClipDurationMs(state, asset->meleeAttackClip);
    } else {
        npc.attackAnimationDurationMs = std::max(1.0f, npc.attackRecoverMs);
    }

    if (npc.attackAnimationDurationMs <= 0.0f) {
        npc.attackAnimationDurationMs = std::max(1.0f, npc.attackRecoverMs);
    }

    if (!npc.attackStartSoundId.empty()) {
        PlaySoundById(state, npc.attackStartSoundId, RandomRangeFloat(0.95f, 1.05f));
    }
}

static bool UpdateNpcAttackState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    npc.attackStateTimeMs += dt * 1000.0f;

    const float durationMs = std::max(1.0f, npc.attackAnimationDurationMs);
    const float normalizedTime = npc.attackStateTimeMs / durationMs;

    if (npc.attackHitPending && !npc.attackHitApplied) {
        if (normalizedTime >= npc.attackHitNormalizedTime) {
            if (TopdownIsPlayerAlive(state) &&
                TopdownIsPlayerWithinNpcAttackRange(npc, state.topdown.runtime.player)) {
                TopdownApplyDamageToPlayer(
                        state,
                        npc.attackDamage,
                        npc.position);

                if (!npc.attackConnectSoundId.empty()) {
                    PlaySoundById(
                            state,
                            npc.attackConnectSoundId,
                            RandomRangeFloat(0.95f, 1.05f));
                }

                Vector2 hitDir = TopdownNormalizeOrZero(
                        TopdownSub(state.topdown.runtime.player.position, npc.position));

                if (TopdownLengthSqr(hitDir) <= 0.000001f) {
                    hitDir = npc.facing;
                }
                if (TopdownLengthSqr(hitDir) <= 0.000001f) {
                    hitDir = Vector2{
                            std::cos(npc.rotationRadians),
                            std::sin(npc.rotationRadians)
                    };
                }
                if (TopdownLengthSqr(hitDir) <= 0.000001f) {
                    hitDir = Vector2{1.0f, 0.0f};
                }

                Vector2 bloodOrigin{};
                if (!TryBuildNpcMeleeHitWorldPosition(state, npc, bloodOrigin)) {
                    bloodOrigin = BuildNpcMeleeFallbackImpactPointOnPlayer(state, npc);
                }

                SpawnBloodImpactParticles(
                        state,
                        bloodOrigin,
                        hitDir,
                        npc.attackEffects);

                QueueBloodSpatterDecals(
                        state,
                        bloodOrigin,
                        hitDir,
                        npc.attackEffects);
            }

            npc.attackHitApplied = true;
            npc.attackHitPending = false;
            npc.attackCooldownRemainingMs =
                    std::max(npc.attackCooldownRemainingMs, npc.attackCooldownMs);
        }
    }

    if (npc.attackStateTimeMs >= durationMs) {
        npc.attackStateTimeMs = 0.0f;
        npc.attackAnimationDurationMs = 0.0f;
        npc.attackHitPending = false;
        npc.attackHitApplied = false;
        return true;
    }
    return false;
}

static bool ShouldSkipNpcAiUpdate(const TopdownNpcRuntime& npc)
{
    if (!npc.active || npc.dead || npc.corpse) {
        return true;
    }

    if (!npc.hostile) {
        return true;
    }

    if (npc.aiMode != TopdownNpcAiMode::SeekAndDestroy) {
        return true;
    }

    return false;
}

static void BeginNpcNonPersistentLostTargetFallback(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    if (TopdownBeginNpcInvestigationState(state, npc)) {
        return;
    }

    TopdownBeginNpcSearchState(npc);
}

static void BeginNpcForcedSearchFallback(TopdownNpcRuntime& npc)
{
    TopdownBeginNpcSearchState(npc);
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

static bool HandleNpcImmediateCombatStates(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (npc.combatState == TopdownNpcCombatState::Attack) {
        UpdateNpcAttackState(state, npc, dt);
        return true;
    }
    if (npc.combatState == TopdownNpcCombatState::Investigation) {
        TopdownUpdateNpcInvestigationState(state, npc, dt);
        return true;
    }
    // Search must be handled before normal targeting/perception.
    // Otherwise perception can re-enter Search every frame, resetting the
    // search timers and causing NPCs to spin indefinitely without finishing.
    if (npc.combatState == TopdownNpcCombatState::Search) {
        //TopdownUpdateNpcSearchState(state, npc, dt);
        return true;
    }
    return false;
}

static void UpdateNpcTargetingPhase(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dtMs,
        bool currentlyDetectsPlayer)
{
    const bool persistentChaseActive =
            npc.persistentChase &&
            npc.hasPlayerTarget;

    if (persistentChaseActive) {
        TopdownUpdateNpcPersistentChaseState(state, npc, currentlyDetectsPlayer);
    } else {
        TopdownUpdateNpcPerception(state, npc, dtMs);
    }
}

static bool HasNpcEnteredExclusiveCombatStateThisFrame(
        TopdownNpcCombatState combatStateAtFrameStart,
        TopdownNpcCombatState combatStateNow)
{
    if (combatStateNow == combatStateAtFrameStart) {
        return false;
    }

    return combatStateNow == TopdownNpcCombatState::Attack ||
           combatStateNow == TopdownNpcCombatState::Investigation ||
           combatStateNow == TopdownNpcCombatState::Search;
}

static bool HandleNpcNoTargetState(TopdownNpcRuntime& npc)
{
    if (npc.hasPlayerTarget) {
        return false;
    }

    npc.combatState = TopdownNpcCombatState::None;
    TopdownResetNpcLostTargetProgress(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);
    TopdownStopNpcMovement(npc);
    return true;
}

static bool HandleNpcMissingChaseTarget(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer,
        Vector2& outChaseTarget)
{
    const bool hasChaseTarget =
            TopdownTryBuildNpcChaseTarget(state, npc, currentlyDetectsPlayer, outChaseTarget);

    if (hasChaseTarget) {
        return false;
    }

    npc.combatState = TopdownNpcCombatState::None;
    TopdownResetNpcLostTargetProgress(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);
    TopdownStopNpcMovement(npc);
    return true;
}

static bool HandleNpcPersistentHardChaseCutoff(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    if (!npc.persistentChase || !npc.hasPlayerTarget) {
        return false;
    }

    const float distanceToPlayer =
            TopdownLength(
                    TopdownSub(
                            state.topdown.runtime.player.position,
                            npc.position));

    if (distanceToPlayer < kHardChaseCutoffDistance) {
        return false;
    }

    BeginNpcForcedSearchFallback(npc);
    return true;
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
        bool currentlyDetectsPlayer)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const bool inAttackRange =
            TopdownIsPlayerWithinNpcAttackRange(npc, player);

    if (!(currentlyDetectsPlayer && inAttackRange)) {
        return false;
    }

    TopdownStopNpcMovement(npc);
    TopdownResetNpcLostTargetProgress(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);

    if (npc.attackCooldownRemainingMs <= 0.0f) {
        StartNpcAttack(state, npc);
    } else {
        npc.combatState = TopdownNpcCombatState::Recover;
    }

    return true;
}

static bool HandleNpcNonPersistentLostTargetFallback(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer,
        float dtMs)
{
    if (npc.persistentChase || currentlyDetectsPlayer || !npc.hasPlayerTarget) {
        TopdownResetNpcLostTargetProgress(npc);
        TopdownResetNpcChaseStuckWatchdog(npc);
        return false;
    }

    const float currentDistance =
            TopdownLength(
                    TopdownSub(
                            npc.lastKnownPlayerPosition,
                            npc.position));

    if (npc.lostTargetProgressTimerMs <= 0.0f) {
        npc.lostTargetLastDistance = currentDistance;
        npc.lostTargetProgressTimerMs = dtMs;
    } else {
        npc.lostTargetProgressTimerMs += dtMs;
    }

    // Give up on non-persistent chase if we make too little progress toward
    // the last known player position.
    if (npc.lostTargetProgressTimerMs >= 800.0f) {
        const float progress = npc.lostTargetLastDistance - currentDistance;
        const bool madeTooLittleProgress = progress < 20.0f;

        npc.lostTargetLastDistance = currentDistance;
        npc.lostTargetProgressTimerMs = 0.0f;

        if (madeTooLittleProgress) {
            BeginNpcNonPersistentLostTargetFallback(state, npc);
            return true;
        }
    }

    if (TopdownUpdateNpcChaseStuckWatchdog(npc, dtMs)) {
        BeginNpcNonPersistentLostTargetFallback(state, npc);
        return true;
    }

    return false;
}

static bool ShouldSkipNpcAiPathing(const TopdownNpcRuntime& npc)
{
    return npc.move.active && npc.move.owner == TopdownNpcMoveOwner::Script;
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

void TopdownUpdateNpcAiSeekAndDestroy(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    const float dtMs = dt * 1000.0f;
    const TopdownNpcCombatState combatStateAtFrameStart = npc.combatState;

    if (ShouldSkipNpcAiUpdate(npc)) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (!TopdownIsPlayerAlive(state)) {
        //StopNpcAiForDeadPlayer(npc);
        return;
    }

    UpdateNpcAttackCooldown(npc, dtMs);
    UpdateNpcRepathTimer(npc, dtMs);

    if (HandleNpcMotionInterrupts(npc)) {
        return;
    }

    if (HandleNpcImmediateCombatStates(state, npc, dt)) {
        return;
    }

    const bool currentlySeesPlayer = TopdownNpcCanSeePlayer(state, npc);
    const bool currentlyHearsPlayer = TopdownNpcCanHearPlayer(state, npc);
    const bool currentlyDetectsPlayer = currentlySeesPlayer || currentlyHearsPlayer;

    UpdateNpcTargetingPhase(state, npc, dtMs, currentlyDetectsPlayer);

    if (HasNpcEnteredExclusiveCombatStateThisFrame(
                combatStateAtFrameStart,
                npc.combatState)) {
        return;
    }

    if (HandleNpcNoTargetState(npc)) {
        return;
    }

    if (!npc.persistentChase &&
        !currentlyDetectsPlayer &&
        npc.loseTargetTimerMs >= npc.loseTargetTimeoutMs) {
        BeginNpcNonPersistentLostTargetFallback(state, npc);
        return;
    }

    Vector2 chaseTarget{};
    if (HandleNpcMissingChaseTarget(state, npc, currentlyDetectsPlayer, chaseTarget)) {
        return;
    }

    if (HandleNpcPersistentHardChaseCutoff(state, npc)) {
        return;
    }

    if (currentlyDetectsPlayer) {
        UpdateNpcFacingTowardPlayer(state, npc);
    }

    if (TryNpcAttackOrRecover(state, npc, currentlyDetectsPlayer)) {
        return;
    }

    // Alert/perception only acquires target metadata; chase policy is owned by this state update.
    npc.combatState = TopdownNpcCombatState::Chase;

    if (HandleNpcNonPersistentLostTargetFallback(state, npc, currentlyDetectsPlayer, dtMs)) {
        return;
    }

    if (ShouldSkipNpcAiPathing(npc)) {
        return;
    }

    UpdateNpcChasePathing(state, npc, chaseTarget);
}

static void StopNpcEngagedExecution(TopdownNpcRuntime& npc)
{
    npc.hasPlayerTarget = false;
    TopdownResetNpcLostTargetProgress(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);
    npc.combatState = TopdownNpcCombatState::None;
    TopdownStopNpcMovement(npc);
    TopdownResetNpcSearchTimers(npc);
}

// ------------------------------ new FSM ---------------------------------------------------
void TopdownNpcAiSeekAndDestroy_UpdateInvestigating(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt)
{
    const float dtMs = dt * 1000.0f;
    if (npc.combatState != TopdownNpcCombatState::Search) {
        TopdownBeginNpcSearchState(npc);
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
}

void TopdownNpcAiSeekAndDestroy_UpdateEngaged(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt)
{
    if (npc.dead || npc.corpse || !npc.hostile) {
        return;
    }

    // early out if player is dead
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
        const bool attackFinished = UpdateNpcAttackState(state, npc, dt);
        if (attackFinished) {
            npc.combatState = TopdownNpcCombatState::None;
        }
        return;
    }

    if(perception.detectsPlayer) UpdateNpcFacingTowardPlayer(state, npc);

    if (TryNpcAttackOrRecover(state, npc, perception.detectsPlayer)) {
        return;
    }

    // Npc has acquired the player at this point, set chase since neither attack nor recover was possible
    npc.combatState = TopdownNpcCombatState::Chase;

    Vector2 chaseTarget{};
    bool hasChaseTarget = false;

    if (perception.detectsPlayer) {
        chaseTarget = state.topdown.runtime.player.position;
        hasChaseTarget = true;
    } else if (npc.hasPlayerTarget) {
        chaseTarget = npc.lastKnownPlayerPosition;
        hasChaseTarget = true;
    }
    // always set chaseTarget to player while in persistent chase
    if(npc.persistentChase) {
        chaseTarget = state.topdown.runtime.player.position;
        hasChaseTarget = true;
    }

    // sanity upper bound cut off for persistentChase
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

    if (hasChaseTarget) {
        UpdateNpcChasePathing(state, npc, chaseTarget);
    }
}