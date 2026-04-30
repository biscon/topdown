#include "topdown/TopdownNpcAi.h"
#include "topdown/TopdownNpcInvestigation.h"
#include "TopdownNpcAiCommon.h"
#include "TopdownNpcAiSeekAndDestroy.h"
#include "TopdownHelpers.h"
#include "TopdownNpcAiHoldAndFire.h"
#include "TopdownNpcPatrol.h"
#include "NpcRegistry.h"

static constexpr float kEngagedLostTargetGraceMs = 700.0f;
static constexpr float kGuardLookAtSoundDurationMs = 900.0f;
static constexpr float kGuardReturnArriveRadius = 24.0f;

static bool NpcHasActivePatrol(const TopdownNpcRuntime& npc)
{
    return npc.scriptBehavior.mode == TopdownNpcScriptBehaviorMode::PatrolRoute &&
           npc.scriptBehavior.patrol.active;
}

static void BeginGuardLookAtSound(TopdownNpcRuntime& npc, Vector2 soundPosition)
{
    const Vector2 toSound = TopdownSub(soundPosition, npc.position);
    if (TopdownLengthSqr(toSound) <= 0.000001f) {
        return;
    }

    const Vector2 dir = TopdownNormalizeOrZero(toSound);
    npc.guardLookAtSoundRadians = std::atan2(dir.y, dir.x);
    npc.guardLookAtSoundTimerMs = kGuardLookAtSoundDurationMs;
}

static void UpdateGuardLookAtSound(TopdownNpcRuntime& npc, float dtMs)
{
    if (npc.guardLookAtSoundTimerMs <= 0.0f) {
        return;
    }

    npc.guardLookAtSoundTimerMs -= dtMs;
    if (npc.guardLookAtSoundTimerMs < 0.0f) {
        npc.guardLookAtSoundTimerMs = 0.0f;
    }

    npc.rotationRadians = npc.guardLookAtSoundRadians;
    npc.facing = Vector2{
            std::cos(npc.rotationRadians),
            std::sin(npc.rotationRadians)
    };
}

static void ReturnGuardToPostOrPatrol(TopdownNpcRuntime& npc)
{
    npc.combatState = TopdownNpcCombatState::None;
    npc.hasPlayerTarget = false;
    npc.engagedLostTargetTimerMs = 0.0f;
    npc.guardLookAtSoundTimerMs = 0.0f;

    if (NpcHasActivePatrol(npc)) {
        TopdownResumeNpcPatrol(npc);
        npc.engagementState = TopdownNpcEngagementState::Guarding;
        return;
    }

    npc.engagementState = TopdownNpcEngagementState::ReturningToGuardPost;
    TopdownStopNpcMovement(npc);
}

static void FreezeNpcAiState(TopdownNpcRuntime& npc)
{
    if (!npc.active) {
        return;
    }

    TopdownStopNpcMovement(npc);
    TopdownResetNpcSearchTimers(npc);
    TopdownResetNpcInvestigationState(npc);

    npc.combatState = TopdownNpcCombatState::None;
    npc.attackHitPending = false;
    npc.attackHitApplied = false;
    npc.attackStateTimeMs = 0.0f;
    npc.attackAnimationDurationMs = 0.0f;
    npc.currentVelocity = Vector2{};
    npc.engagedLostTargetTimerMs = 0.0f;
    npc.guardLookAtSoundTimerMs = 0.0f;
}

static void UpdateNpcReturningToGuardPost(GameState& state, TopdownNpcRuntime& npc, float /*dt*/)
{
    if (!npc.guard || !npc.hasGuardHomePosition) {
        npc.engagementState = npc.guard
                ? TopdownNpcEngagementState::Guarding
                : TopdownNpcEngagementState::Unaware;
        npc.combatState = TopdownNpcCombatState::None;
        TopdownStopNpcMovement(npc);
        return;
    }

    if (TopdownHasNpcReachedPoint(npc, npc.guardHomePosition, kGuardReturnArriveRadius)) {
        npc.engagementState = TopdownNpcEngagementState::Guarding;
        npc.combatState = TopdownNpcCombatState::None;
        TopdownStopNpcMovement(npc);
        return;
    }

    if (!npc.move.active || npc.repathTimerMs <= 0.0f) {
        TopdownBuildNpcPathToTarget(
                state,
                npc,
                npc.guardHomePosition,
                TopdownNpcMoveOwner::Ai);

        npc.repathTimerMs = 500.0f;
    }
}

static void DispatchNpcInvestigatingExecution(GameState& state, TopdownNpcRuntime& npc,
                                              const TopdownNpcPerceptionResult& perception, float dt) {
    switch (npc.aiMode) {
        case TopdownNpcAiMode::SeekAndDestroy:
            TopdownNpcAiSeekAndDestroy_UpdateInvestigating(state, npc, perception, dt);
            break;

        case TopdownNpcAiMode::HoldAndFire:
            TopdownNpcAiHoldAndFire_UpdateInvestigating(state, npc, perception, dt);
            break;

        case TopdownNpcAiMode::None:
        default:
            break;
    }
}

static void DispatchNpcEngagedExecution(GameState& state, TopdownNpcRuntime& npc,
                                        const TopdownNpcPerceptionResult& perception, float dt) {
    switch (npc.aiMode) {
        case TopdownNpcAiMode::SeekAndDestroy:
            TopdownNpcAiSeekAndDestroy_UpdateEngaged(state, npc, perception, dt);
            break;

        case TopdownNpcAiMode::HoldAndFire:
            TopdownNpcAiHoldAndFire_UpdateEngaged(state, npc, perception, dt);
            break;

        case TopdownNpcAiMode::None:
        default:
            break;
    }
}

static TopdownNpcPerceptionResult EvaluateNpcPerception(GameState& state, TopdownNpcRuntime& npc) {
    TopdownNpcPerceptionResult result;
    result.seesPlayer = TopdownNpcCanSeePlayer(state, npc);
    result.hearsPlayer = TopdownNpcCanHearPlayer(state, npc);
    result.detectsPlayer = result.seesPlayer || result.hearsPlayer;
    result.detectedPlayerPosition = state.topdown.runtime.player.position;

    TopdownForEachWorldEventOfType(
            state,
            TopdownWorldEventType::Gunshot,
            [&](const TopdownWorldEvent& evt)
            {
                const float distSqr =
                        TopdownLengthSqr(TopdownSub(npc.position, evt.position));

                if (distSqr <= evt.radius * evt.radius) {
                    result.heardGunshot = true;
                    result.heardGunshotPosition = evt.position;
                }
            }
    );

    return result;
}

static void UpdateNpcEngagementState(
        GameState& state,
        TopdownNpcRuntime& npc,
        const TopdownNpcPerceptionResult& perception,
        float dt)
{
    const float dtMs = dt * 1000.0f;

    npc.investigationRetargetCooldownMs -= dtMs;
    if (npc.investigationRetargetCooldownMs < 0.0f) {
        npc.investigationRetargetCooldownMs = 0.0f;
    }

    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, npc.assetId);

    const float reactionTimeMs =
            (asset != nullptr && asset->loaded)
            ? std::max(0.0f, asset->reactionTimeMs)
            : 0.0f;

    if (perception.detectsPlayer) {
        npc.guardLookAtSoundTimerMs = 0.0f;
        npc.engagedLostTargetTimerMs = 0.0f;

        const bool newlyDetectedPlayer =
                !npc.hasPlayerTarget;

        npc.hasPlayerTarget = true;
        npc.lastKnownPlayerPosition = perception.detectedPlayerPosition;
        npc.investigationPosition = perception.detectedPlayerPosition;

        if (newlyDetectedPlayer) {
            npc.reactionTimerMs = 0.0f;
            npc.hasReactedToPlayer = false;
        }

        if (!npc.hasReactedToPlayer) {
            npc.reactionTimerMs += dtMs;

            if (npc.reactionTimerMs >= reactionTimeMs) {
                npc.reactionTimerMs = reactionTimeMs;
                npc.hasReactedToPlayer = true;
                npc.engagementState = TopdownNpcEngagementState::Engaged;

                const float nearbyAlertRadius =
                        std::max(180.0f, npc.hearingRange);

                TopdownAlertNearbyNpcs(state, npc, nearbyAlertRadius);
            } else {
                npc.combatState = TopdownNpcCombatState::None;
                npc.engagementState = TopdownNpcEngagementState::Reacting;
                TopdownStopNpcMovement(npc);
            }
        } else {
            npc.engagementState = TopdownNpcEngagementState::Engaged;
        }
        return;
    }

    if (npc.engagementState == TopdownNpcEngagementState::Engaged &&
        npc.hasPlayerTarget &&
        !npc.persistentChase) {
        npc.engagedLostTargetTimerMs += dtMs;

        if (npc.engagedLostTargetTimerMs < kEngagedLostTargetGraceMs) {
            npc.engagementState = TopdownNpcEngagementState::Engaged;
            return;
        }

        npc.engagedLostTargetTimerMs = 0.0f;
        if (npc.guard) {
            npc.combatState = TopdownNpcCombatState::None;
            npc.engagementState = TopdownNpcEngagementState::Investigating;
            TopdownStopNpcMovement(npc);
            return;
        }
    }

    npc.reactionTimerMs = 0.0f;
    npc.hasReactedToPlayer = false;

    if (perception.heardGunshot &&
        npc.engagementState != TopdownNpcEngagementState::Engaged) {
        if (npc.guard) {
            if (!NpcHasActivePatrol(npc)) {
                BeginGuardLookAtSound(npc, perception.heardGunshotPosition);
                npc.engagementState = TopdownNpcEngagementState::Guarding;
                npc.combatState = TopdownNpcCombatState::None;
                TopdownStopNpcMovement(npc);
            }
            return;
        }

        const float distToCurrentInvestigation =
                TopdownLength(TopdownSub(
                        perception.heardGunshotPosition,
                        npc.investigationPosition));

        const bool canRetargetByTime =
                npc.investigationRetargetCooldownMs <= 0.0f;

        const bool canRetargetByDistance =
                distToCurrentInvestigation >= 300.0f;

        if (!npc.hasPlayerTarget || canRetargetByTime || canRetargetByDistance) {
            npc.hasPlayerTarget = true;
            npc.investigationPosition = perception.heardGunshotPosition;
            npc.combatState = TopdownNpcCombatState::None;
            npc.engagementState = TopdownNpcEngagementState::Investigating;
            TopdownStopNpcMovement(npc);
            npc.investigationRetargetCooldownMs = 300.0f;
        }
        return;
    }

    if (npc.hasPlayerTarget) {
        if (npc.persistentChase) {
            npc.engagementState = TopdownNpcEngagementState::Engaged;
            npc.engagedLostTargetTimerMs = 0.0f;
            return;
        }

        switch (npc.engagementState) {
            case TopdownNpcEngagementState::Reacting:
            case TopdownNpcEngagementState::Engaged:
                npc.combatState = TopdownNpcCombatState::None;
                npc.engagementState = TopdownNpcEngagementState::Investigating;
                TopdownStopNpcMovement(npc);
                npc.engagedLostTargetTimerMs = 0.0f;
                return;

            case TopdownNpcEngagementState::Investigating:
                return;

            case TopdownNpcEngagementState::Unaware:
            case TopdownNpcEngagementState::Guarding:
            case TopdownNpcEngagementState::ReturningToGuardPost:
            default:
                npc.engagementState = TopdownNpcEngagementState::Investigating;
                return;
        }
    }

    if (npc.engagementState == TopdownNpcEngagementState::Investigating) {
        return;
    }

    npc.combatState = TopdownNpcCombatState::None;
    npc.engagementState = npc.guard
            ? TopdownNpcEngagementState::Guarding
            : TopdownNpcEngagementState::Unaware;
    npc.engagedLostTargetTimerMs = 0.0f;
}

void TopdownUpdateNpcAi(GameState& state, float dt)
{
    // Freeze npc by zeroing ai files and velocity, then early out of AI processing
    if (state.topdown.runtime.aiFrozen) {
        for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
            FreezeNpcAiState(npc);
        }
        return;
    }

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }
        if (npc.dead || npc.corpse) {
            npc.engagementState = TopdownNpcEngagementState::Unaware;
            npc.combatState = TopdownNpcCombatState::None;
            npc.hasPlayerTarget = false;
            npc.guardLookAtSoundTimerMs = 0.0f;
            npc.engagedLostTargetTimerMs = 0.0f;
            TopdownClearNpcPatrol(state, npc);
            continue;
        }

        TopdownNpcPerceptionResult perception = EvaluateNpcPerception(state, npc);
        UpdateNpcEngagementState(state, npc, perception, dt);
        if (npc.engagementState == TopdownNpcEngagementState::Reacting ||
            npc.engagementState == TopdownNpcEngagementState::Investigating ||
            npc.engagementState == TopdownNpcEngagementState::Engaged ||
            npc.engagementState == TopdownNpcEngagementState::ReturningToGuardPost) {
            TopdownInterruptNpcPatrol(state, npc);
        }

        switch(npc.engagementState) {
            case TopdownNpcEngagementState::Unaware:
                TopdownUpdateNpcPatrol(state, npc, dt);
                break;

            case TopdownNpcEngagementState::Guarding:
                if (NpcHasActivePatrol(npc)) {
                    TopdownUpdateNpcPatrol(state, npc, dt);
                } else {
                    UpdateGuardLookAtSound(npc, dt * 1000.0f);
                }
                break;

            case TopdownNpcEngagementState::Reacting:
                // direct detection acquired, but reaction timer has not completed yet
                break;

            case TopdownNpcEngagementState::Investigating:
            {
                DispatchNpcInvestigatingExecution(state, npc, perception, dt);
                if (npc.guard &&
                    !npc.hasPlayerTarget &&
                    (npc.engagementState == TopdownNpcEngagementState::Unaware ||
                     npc.engagementState == TopdownNpcEngagementState::Guarding)) {
                    ReturnGuardToPostOrPatrol(npc);
                }
                break;
            }

            case TopdownNpcEngagementState::Engaged:
                DispatchNpcEngagedExecution(state, npc, perception, dt);
                break;

            case TopdownNpcEngagementState::ReturningToGuardPost:
                UpdateNpcReturningToGuardPost(state, npc, dt);
                break;
        }
    }

    TopdownPruneNpcInvestigationContexts(state);
    TopdownPruneNpcPatrolContexts(state);
}
