#include "topdown/TopdownNpcAi.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/NpcRegistry.h"
#include "topdown/LevelEffects.h"
#include "resources/AsepriteAsset.h"
#include "audio/Audio.h"
#include "raymath.h"
#include "LevelCollision.h"

constexpr const float kHardChaseCutoffDistance = 3500.0f;

static float SmoothStep01(float t)
{
    t = Clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static void ClearNpcSearchState(TopdownNpcRuntime& npc)
{
    npc.searchStateTimeMs = 0.0f;
    npc.searchDurationMs = 0.0f;
    npc.searchBaseFacingRadians = 0.0f;
    npc.searchSweepDegrees = 0.0f;
}

static void ResetNpcLostTargetProgress(TopdownNpcRuntime& npc)
{
    npc.lostTargetProgressTimerMs = 0.0f;
    npc.lostTargetLastDistance = 0.0f;
}

static void ResetNpcChaseStuckWatchdog(TopdownNpcRuntime& npc)
{
    npc.chaseStuckTimerMs = 0.0f;
    npc.chaseStuckLastPosition = npc.position;
}

static bool HasNpcReachedLastKnownTarget(const TopdownNpcRuntime& npc)
{
    const float arriveRadius = 300.0f;

    return TopdownHasNpcReachedPoint(
            npc,
            npc.lastKnownPlayerPosition,
            arriveRadius);
}

static void FinishNpcSearchAndForgetTarget(TopdownNpcRuntime& npc)
{
    npc.hasPlayerTarget = false;
    npc.loseTargetTimerMs = 0.0f;
    npc.repathTimerMs = 0.0f;
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
    npc.combatState = TopdownNpcCombatState::None;

    ResetNpcLostTargetProgress(npc);
    ResetNpcChaseStuckWatchdog(npc);
    ClearNpcSearchState(npc);
    TopdownStopNpcMovement(npc);
}

static void BeginNpcSearchState(TopdownNpcRuntime& npc)
{
    TopdownStopNpcMovement(npc);
    ResetNpcChaseStuckWatchdog(npc);

    npc.combatState = TopdownNpcCombatState::Search;
    npc.searchStateTimeMs = 0.0f;
    npc.searchDurationMs = 3600.0f;
    npc.searchSweepDegrees = 260.0f;

    Vector2 facing = TopdownNormalizeOrZero(npc.facing);
    if (TopdownLengthSqr(facing) <= 0.000001f) {
        facing = Vector2{
                std::cos(npc.rotationRadians),
                std::sin(npc.rotationRadians)
        };
        facing = TopdownNormalizeOrZero(facing);
    }

    if (TopdownLengthSqr(facing) <= 0.000001f) {
        facing = Vector2{1.0f, 0.0f};
    }

    npc.searchBaseFacingRadians = std::atan2(facing.y, facing.x);
    npc.rotationRadians = npc.searchBaseFacingRadians;
    npc.facing = facing;
}

static bool UpdateNpcChaseStuckWatchdog(
        TopdownNpcRuntime& npc,
        float dtMs)
{
    if (npc.chaseStuckTimerMs <= 0.0f) {
        npc.chaseStuckLastPosition = npc.position;
        npc.chaseStuckTimerMs = dtMs;
        return false;
    }

    npc.chaseStuckTimerMs += dtMs;

    if (npc.chaseStuckTimerMs < 800.0f) {
        return false;
    }

    const float movedDistance =
            TopdownLength(
                    TopdownSub(
                            npc.position,
                            npc.chaseStuckLastPosition));

    const bool movedTooLittle = movedDistance < 20.0f;

    npc.chaseStuckLastPosition = npc.position;
    npc.chaseStuckTimerMs = 0.0f;

    return movedTooLittle;
}

static bool TryBuildNpcChaseTarget(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer,
        Vector2& outChaseTarget)
{
    if (!npc.hasPlayerTarget) {
        return false;
    }

    if (npc.persistentChase) {
        outChaseTarget = state.topdown.runtime.player.position;
        return true;
    }

    if (currentlyDetectsPlayer) {
        outChaseTarget = state.topdown.runtime.player.position;
        return true;
    }

    outChaseTarget = npc.lastKnownPlayerPosition;
    return true;
}

static void UpdateNpcSearchState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (npc.combatState != TopdownNpcCombatState::Search) {
        return;
    }

    npc.searchStateTimeMs += dt * 1000.0f;

    const float durationMs = std::max(1.0f, npc.searchDurationMs);
    const float totalT = Clamp(npc.searchStateTimeMs / durationMs, 0.0f, 1.0f);

    const float halfSweepRadians =
            (npc.searchSweepDegrees * 0.5f) * DEG2RAD;

    float signedOffsetRadians = 0.0f;

    if (totalT < (1.0f / 3.0f)) {
        const float t = SmoothStep01(totalT / (1.0f / 3.0f));
        signedOffsetRadians = Lerp(0.0f, -halfSweepRadians, t);
    } else if (totalT < (2.0f / 3.0f)) {
        const float t = SmoothStep01((totalT - (1.0f / 3.0f)) / (1.0f / 3.0f));
        signedOffsetRadians = Lerp(-halfSweepRadians, halfSweepRadians, t);
    } else {
        const float t = SmoothStep01((totalT - (2.0f / 3.0f)) / (1.0f / 3.0f));
        signedOffsetRadians = Lerp(halfSweepRadians, 0.0f, t);
    }

    const float newAngle =
            NormalizeAngleRadians(npc.searchBaseFacingRadians + signedOffsetRadians);

    npc.rotationRadians = newAngle;
    npc.facing = Vector2{
            std::cos(newAngle),
            std::sin(newAngle)
    };

    if (TopdownNpcCanSeePlayer(state, npc) ||
        TopdownNpcCanHearPlayer(state, npc)) {
        TopdownAlertNpcToPlayer(state, npc);
        ClearNpcSearchState(npc);
        return;
    }

    if (npc.searchStateTimeMs >= durationMs) {
        npc.rotationRadians = npc.searchBaseFacingRadians;
        npc.facing = Vector2{
                std::cos(npc.searchBaseFacingRadians),
                std::sin(npc.searchBaseFacingRadians)
        };

        FinishNpcSearchAndForgetTarget(npc);
    }
}

static TopdownPlayerWeaponConfig BuildNpcAttackEffectsAsWeaponConfig(
        const TopdownNpcAttackEffectsConfig& fx)
{
    TopdownPlayerWeaponConfig cfg;
    cfg.equipmentSetId = "npc_attack_fx";

    cfg.bloodImpactParticleCount = fx.bloodImpactParticleCount;
    cfg.bloodImpactParticleSpeedMin = fx.bloodImpactParticleSpeedMin;
    cfg.bloodImpactParticleSpeedMax = fx.bloodImpactParticleSpeedMax;
    cfg.bloodImpactParticleLifetimeMsMin = fx.bloodImpactParticleLifetimeMsMin;
    cfg.bloodImpactParticleLifetimeMsMax = fx.bloodImpactParticleLifetimeMsMax;
    cfg.bloodImpactParticleSizeMin = fx.bloodImpactParticleSizeMin;
    cfg.bloodImpactParticleSizeMax = fx.bloodImpactParticleSizeMax;
    cfg.bloodImpactSpreadDegrees = fx.bloodImpactSpreadDegrees;

    cfg.bloodDecalCountMin = fx.bloodDecalCountMin;
    cfg.bloodDecalCountMax = fx.bloodDecalCountMax;
    cfg.bloodDecalDistanceMin = fx.bloodDecalDistanceMin;
    cfg.bloodDecalDistanceMax = fx.bloodDecalDistanceMax;
    cfg.bloodDecalRadiusMin = fx.bloodDecalRadiusMin;
    cfg.bloodDecalRadiusMax = fx.bloodDecalRadiusMax;
    cfg.bloodDecalSpreadDegrees = fx.bloodDecalSpreadDegrees;
    cfg.bloodDecalWallPadding = fx.bloodDecalWallPadding;
    cfg.bloodDecalOpacityMin = fx.bloodDecalOpacityMin;
    cfg.bloodDecalOpacityMax = fx.bloodDecalOpacityMax;

    return cfg;
}

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

static Vector2 BuildNpcFallbackMeleeHitWorldPosition(
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

static void UpdateNpcPerception(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dtMs)
{
    const bool seesPlayer = TopdownNpcCanSeePlayer(state, npc);
    const bool hearsPlayer = TopdownNpcCanHearPlayer(state, npc);

    if (seesPlayer || hearsPlayer) {
        if (!npc.hasPlayerTarget) {
            npc.hasPlayerTarget = true;
            npc.repathTimerMs = 0.0f;
        }

        npc.lastKnownPlayerPosition = state.topdown.runtime.player.position;
        npc.loseTargetTimerMs = 0.0f;
        ResetNpcLostTargetProgress(npc);
        npc.awarenessState = TopdownNpcAwarenessState::Alerted;

        const float nearbyAlertRadius =
                std::max(180.0f, npc.hearingRange);

        TopdownAlertNearbyNpcs(state, npc, nearbyAlertRadius);
        return;
    }

    if (npc.hasPlayerTarget) {
        npc.loseTargetTimerMs += dtMs;
        npc.awarenessState = TopdownNpcAwarenessState::Suspicious;

        if (npc.loseTargetTimerMs >= npc.loseTargetTimeoutMs) {
            if (HasNpcReachedLastKnownTarget(npc)) {
                BeginNpcSearchState(npc);
                return;
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

            if (npc.lostTargetProgressTimerMs >= 800.0f) {
                const float progress =
                        npc.lostTargetLastDistance - currentDistance;

                const bool madeTooLittleProgress = progress < 20.0f;
                if (madeTooLittleProgress) {
                    BeginNpcSearchState(npc);
                    return;
                }

                npc.lostTargetLastDistance = currentDistance;
                npc.lostTargetProgressTimerMs = 0.0f;
            }
        } else {
            ResetNpcLostTargetProgress(npc);
        }

        return;
    }

    ResetNpcLostTargetProgress(npc);
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
}

static void UpdateNpcPersistentChaseState(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer)
{
    if (!npc.hasPlayerTarget) {
        return;
    }

    npc.awarenessState = TopdownNpcAwarenessState::Alerted;
    npc.loseTargetTimerMs = 0.0f;
    npc.lastKnownPlayerPosition = state.topdown.runtime.player.position;
    ResetNpcLostTargetProgress(npc);

    if (currentlyDetectsPlayer) {
        ResetNpcChaseStuckWatchdog(npc);
    }
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
    ResetNpcChaseStuckWatchdog(npc);

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

static void UpdateNpcAttackState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (npc.combatState != TopdownNpcCombatState::Attack) {
        return;
    }

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
                    bloodOrigin = BuildNpcFallbackMeleeHitWorldPosition(state, npc);
                }

                const TopdownPlayerWeaponConfig bloodConfig =
                        BuildNpcAttackEffectsAsWeaponConfig(npc.attackEffects);

                SpawnBloodImpactParticles(
                        state,
                        bloodOrigin,
                        hitDir,
                        bloodConfig);

                QueueBloodSpatterDecals(
                        state,
                        bloodOrigin,
                        hitDir,
                        bloodConfig);
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

        if (TopdownIsPlayerAlive(state) && npc.hasPlayerTarget) {
            if (TopdownIsPlayerWithinNpcAttackRange(npc, state.topdown.runtime.player)) {
                npc.combatState = TopdownNpcCombatState::Recover;
            } else {
                npc.combatState = TopdownNpcCombatState::Chase;
            }
        } else {
            npc.combatState = TopdownNpcCombatState::None;
        }
    }
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

static void StopNpcAiForDeadPlayer(TopdownNpcRuntime& npc)
{
    npc.hasPlayerTarget = false;
    ResetNpcLostTargetProgress(npc);
    ResetNpcChaseStuckWatchdog(npc);
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
    npc.combatState = TopdownNpcCombatState::None;
    TopdownStopNpcMovement(npc);
    ClearNpcSearchState(npc);
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
        ResetNpcChaseStuckWatchdog(npc);
        npc.combatState =
                npc.hasPlayerTarget
                ? TopdownNpcCombatState::Chase
                : TopdownNpcCombatState::None;
        npc.currentVelocity = Vector2{};
        return true;
    }

    if (TopdownLengthSqr(npc.knockbackVelocity) > 0.000001f) {
        ResetNpcChaseStuckWatchdog(npc);
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

    if (npc.combatState == TopdownNpcCombatState::Search) {
        UpdateNpcSearchState(state, npc, dt);
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
        UpdateNpcPersistentChaseState(state, npc, currentlyDetectsPlayer);
    } else {
        UpdateNpcPerception(state, npc, dtMs);
    }
}

static bool HandleNpcNoTargetState(TopdownNpcRuntime& npc)
{
    if (npc.hasPlayerTarget) {
        return false;
    }

    npc.combatState = TopdownNpcCombatState::None;
    ResetNpcLostTargetProgress(npc);
    ResetNpcChaseStuckWatchdog(npc);
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
            TryBuildNpcChaseTarget(state, npc, currentlyDetectsPlayer, outChaseTarget);

    if (hasChaseTarget) {
        return false;
    }

    npc.combatState = TopdownNpcCombatState::None;
    ResetNpcLostTargetProgress(npc);
    ResetNpcChaseStuckWatchdog(npc);
    TopdownStopNpcMovement(npc);
    return true;
}

static bool HandleNpcHardChaseCutoff(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    const float distanceToPlayer =
            TopdownLength(
                    TopdownSub(
                            state.topdown.runtime.player.position,
                            npc.position));

    if (distanceToPlayer < kHardChaseCutoffDistance) {
        return false;
    }

    BeginNpcSearchState(npc);
    return true;
}

static void UpdateNpcFacingTowardPlayer(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer)
{
    if (!currentlyDetectsPlayer) {
        return;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const Vector2 toPlayer = TopdownSub(player.position, npc.position);

    if (TopdownLengthSqr(toPlayer) <= 0.000001f) {
        return;
    }

    const Vector2 facing = TopdownNormalizeOrZero(toPlayer);
    npc.facing = facing;
    npc.rotationRadians = std::atan2(facing.y, facing.x);
}

static bool HandleNpcAttackOrRecover(
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
    ResetNpcLostTargetProgress(npc);
    ResetNpcChaseStuckWatchdog(npc);

    if (npc.attackCooldownRemainingMs <= 0.0f) {
        StartNpcAttack(state, npc);
    } else {
        npc.combatState = TopdownNpcCombatState::Recover;
    }

    return true;
}

static bool HandleNpcChaseWatchdog(
        TopdownNpcRuntime& npc,
        bool persistentChaseActive,
        bool currentlyDetectsPlayer,
        float dtMs)
{
    if (persistentChaseActive && !currentlyDetectsPlayer) {
        if (UpdateNpcChaseStuckWatchdog(npc, dtMs)) {
            BeginNpcSearchState(npc);
            return true;
        }
    } else {
        ResetNpcChaseStuckWatchdog(npc);
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
    if (!npc.move.active ||
        npc.move.owner != TopdownNpcMoveOwner::Ai ||
        npc.repathTimerMs <= 0.0f) {
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

    if (ShouldSkipNpcAiUpdate(npc)) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (!TopdownIsPlayerAlive(state)) {
        StopNpcAiForDeadPlayer(npc);
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

    const bool persistentChaseActive =
            npc.persistentChase &&
            npc.hasPlayerTarget;

    UpdateNpcTargetingPhase(state, npc, dtMs, currentlyDetectsPlayer);

    if (npc.combatState == TopdownNpcCombatState::Search) {
        UpdateNpcSearchState(state, npc, dt);
        return;
    }

    if (HandleNpcNoTargetState(npc)) {
        return;
    }

    Vector2 chaseTarget{};
    if (HandleNpcMissingChaseTarget(state, npc, currentlyDetectsPlayer, chaseTarget)) {
        return;
    }

    if (HandleNpcHardChaseCutoff(state, npc)) {
        return;
    }

    UpdateNpcFacingTowardPlayer(state, npc, currentlyDetectsPlayer);

    if (HandleNpcAttackOrRecover(state, npc, currentlyDetectsPlayer)) {
        return;
    }

    npc.combatState = TopdownNpcCombatState::Chase;

    if (HandleNpcChaseWatchdog(npc, persistentChaseActive, currentlyDetectsPlayer, dtMs)) {
        return;
    }

    if (ShouldSkipNpcAiPathing(npc)) {
        return;
    }

    UpdateNpcChasePathing(state, npc, chaseTarget);
}
