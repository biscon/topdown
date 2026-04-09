#include "topdown/TopdownNpcAi.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/NpcRegistry.h"
#include "topdown/LevelEffects.h"
#include "resources/AsepriteAsset.h"
#include "audio/Audio.h"

static TopdownPlayerWeaponConfig BuildNpcMeleeBloodWeaponConfig()
{
    TopdownPlayerWeaponConfig cfg;

    cfg.equipmentSetId = "npc_melee";

    cfg.bloodImpactParticleCount = 10;
    cfg.bloodImpactParticleSpeedMin = 45.0f;
    cfg.bloodImpactParticleSpeedMax = 125.0f;
    cfg.bloodImpactParticleLifetimeMsMin = 160.0f;
    cfg.bloodImpactParticleLifetimeMsMax = 320.0f;
    cfg.bloodImpactParticleSizeMin = 2.5f;
    cfg.bloodImpactParticleSizeMax = 5.5f;
    cfg.bloodImpactSpreadDegrees = 75.0f;

    cfg.bloodDecalCountMin = 3;
    cfg.bloodDecalCountMax = 5;
    cfg.bloodDecalDistanceMin = 8.0f;
    cfg.bloodDecalDistanceMax = 60.0f;
    cfg.bloodDecalRadiusMin = 7.0f;
    cfg.bloodDecalRadiusMax = 14.0f;
    cfg.bloodDecalSpreadDegrees = 50.0f;
    cfg.bloodDecalWallPadding = 6.0f;
    cfg.bloodDecalOpacityMin = 0.72f;
    cfg.bloodDecalOpacityMax = 0.95f;

    return cfg;
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

static bool HasNpcReachedInvestigationDestination(const TopdownNpcRuntime& npc)
{
    const float arriveRadius = 200;

    if (npc.hasInvestigationTarget) {
        return TopdownHasNpcReachedPoint(
                npc,
                npc.investigationTarget,
                arriveRadius);
    }

    return TopdownHasNpcReachedPoint(
            npc,
            npc.lastKnownPlayerPosition,
            arriveRadius);
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
        npc.awarenessState = TopdownNpcAwarenessState::Alerted;

        npc.investigationStuckTimerMs = 0.0f;
        npc.hasInvestigationTarget = false;
        npc.investigationTarget = Vector2{};

        const float nearbyAlertRadius =
                std::max(180.0f, npc.hearingRange);

        TopdownAlertNearbyNpcs(state, npc, nearbyAlertRadius);
        return;
    }

    if (npc.hasPlayerTarget) {
        npc.loseTargetTimerMs += dtMs;
        npc.awarenessState = TopdownNpcAwarenessState::Suspicious;

        // Only start "give up if stuck near investigation zone" logic
        // after the normal lose-target timeout has elapsed.
        if (npc.loseTargetTimerMs >= npc.loseTargetTimeoutMs) {
            const float nearLastKnownRadius = 100.0f;
            const bool nearLastKnown =
                    TopdownHasNpcReachedPoint(
                            npc,
                            npc.lastKnownPlayerPosition,
                            nearLastKnownRadius);

            const bool barelyMoving =
                    TopdownLengthSqr(npc.currentVelocity) < (20.0f * 20.0f);

            if (nearLastKnown && barelyMoving) {
                npc.investigationStuckTimerMs += dtMs;
            } else {
                npc.investigationStuckTimerMs = 0.0f;
            }

            // Normal success case: reached assigned investigation point.
            if (HasNpcReachedInvestigationDestination(npc)) {
                npc.hasPlayerTarget = false;
                npc.loseTargetTimerMs = 0.0f;
                npc.repathTimerMs = 0.0f;
                npc.investigationStuckTimerMs = 0.0f;
                npc.hasInvestigationTarget = false;
                npc.investigationTarget = Vector2{};
                TopdownStopNpcMovement(npc);
                return;
            }

            // Failsafe case: close enough to the whole investigation area,
            // but blocked / jammed / not making progress for a while.
            if (npc.investigationStuckTimerMs >= 700.0f) {
                npc.hasPlayerTarget = false;
                npc.loseTargetTimerMs = 0.0f;
                npc.repathTimerMs = 0.0f;
                npc.investigationStuckTimerMs = 0.0f;
                npc.hasInvestigationTarget = false;
                npc.investigationTarget = Vector2{};
                TopdownStopNpcMovement(npc);
                return;
            }
        }

        return;
    }

    npc.awarenessState = TopdownNpcAwarenessState::Idle;
    npc.investigationStuckTimerMs = 0.0f;
    npc.hasInvestigationTarget = false;
    npc.investigationTarget = Vector2{};
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

void TopdownUpdateNpcAiSeekAndDestroy(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    const float dtMs = dt * 1000.0f;

    if (!npc.active || npc.dead || npc.corpse) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (!npc.hostile) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (npc.aiMode != TopdownNpcAiMode::SeekAndDestroy) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (!TopdownIsPlayerAlive(state)) {
        npc.hasPlayerTarget = false;
        npc.awarenessState = TopdownNpcAwarenessState::Idle;
        npc.combatState = TopdownNpcCombatState::None;
        TopdownStopNpcMovement(npc);
        return;
    }

    if (npc.attackCooldownRemainingMs > 0.0f) {
        npc.attackCooldownRemainingMs -= dtMs;
        if (npc.attackCooldownRemainingMs < 0.0f) {
            npc.attackCooldownRemainingMs = 0.0f;
        }
    }

    if (npc.repathTimerMs > 0.0f) {
        npc.repathTimerMs -= dtMs;
        if (npc.repathTimerMs < 0.0f) {
            npc.repathTimerMs = 0.0f;
        }
    }

    if (npc.hurtStunRemainingMs > 0.0f) {
        npc.combatState =
                npc.hasPlayerTarget
                ? TopdownNpcCombatState::Chase
                : TopdownNpcCombatState::None;
        npc.currentVelocity = Vector2{};
        return;
    }

    if (TopdownLengthSqr(npc.knockbackVelocity) > 0.000001f) {
        return;
    }

    if (npc.combatState == TopdownNpcCombatState::Attack) {
        UpdateNpcAttackState(state, npc, dt);
        return;
    }

    const bool currentlySeesPlayer = TopdownNpcCanSeePlayer(state, npc);
    const bool currentlyHearsPlayer = TopdownNpcCanHearPlayer(state, npc);
    const bool currentlyDetectsPlayer = currentlySeesPlayer || currentlyHearsPlayer;

    UpdateNpcPerception(state, npc, dtMs);

    if (!npc.hasPlayerTarget) {
        npc.combatState = TopdownNpcCombatState::None;
        TopdownStopNpcMovement(npc);
        return;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const bool inAttackRange =
            TopdownIsPlayerWithinNpcAttackRange(npc, player);

    if (currentlyDetectsPlayer) {
        const Vector2 toPlayer = TopdownSub(player.position, npc.position);
        if (TopdownLengthSqr(toPlayer) > 0.000001f) {
            const Vector2 facing = TopdownNormalizeOrZero(toPlayer);
            npc.facing = facing;
            npc.rotationRadians = std::atan2(facing.y, facing.x);
        }
    }

    if (currentlyDetectsPlayer && inAttackRange) {
        TopdownStopNpcMovement(npc);

        if (npc.attackCooldownRemainingMs <= 0.0f) {
            StartNpcAttack(state, npc);
        } else {
            npc.combatState = TopdownNpcCombatState::Recover;
        }

        return;
    }

    npc.combatState = TopdownNpcCombatState::Chase;

    if (npc.move.active && npc.move.owner == TopdownNpcMoveOwner::Script) {
        return;
    }

    if (!npc.move.active ||
        npc.move.owner != TopdownNpcMoveOwner::Ai ||
        npc.repathTimerMs <= 0.0f) {

        Vector2 chaseTarget{};

        if (currentlyDetectsPlayer) {
            // Real chase while target is actively detected.
            chaseTarget = player.position;
            npc.hasInvestigationTarget = false;
            npc.investigationTarget = Vector2{};
            npc.investigationStuckTimerMs = 0.0f;
        } else {
            // Investigation mode: spread out around the last known point.
            npc.investigationTarget =
                    TopdownBuildNpcInvestigationTargetAroundPoint(
                            state,
                            npc,
                            npc.lastKnownPlayerPosition);
            npc.hasInvestigationTarget = true;
            chaseTarget = npc.investigationTarget;
        }

        TopdownBuildNpcPathToTarget(
                state,
                npc,
                chaseTarget,
                TopdownNpcMoveOwner::Ai);

        npc.repathTimerMs = std::max(1.0f, npc.chaseRepathIntervalMs);
    }
}
