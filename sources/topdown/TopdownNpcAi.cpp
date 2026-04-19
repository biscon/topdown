#include "topdown/TopdownNpcAi.h"
#include "topdown/TopdownNpcInvestigation.h"
#include "TopdownNpcAiCommon.h"
#include "TopdownNpcAiSeekAndDestroy.h"
#include "TopdownHelpers.h"
#include "TopdownNpcAiHoldAndFire.h"

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

    const bool wasEngaged =
            npc.engagementState == TopdownNpcEngagementState::Engaged;

    if (perception.detectsPlayer) {
        const bool newlyDetectedPlayer =
                !wasEngaged;
        npc.hasPlayerTarget = true;
        npc.lastKnownPlayerPosition = perception.detectedPlayerPosition;
        npc.investigationPosition = perception.detectedPlayerPosition;
        npc.engagementState = TopdownNpcEngagementState::Engaged;
        if (newlyDetectedPlayer) {
            const float nearbyAlertRadius =
                    std::max(180.0f, npc.hearingRange);

            TopdownAlertNearbyNpcs(state, npc, nearbyAlertRadius);
        }
        return;
    }

    if (perception.heardGunshot &&
        npc.engagementState != TopdownNpcEngagementState::Engaged) {

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
            return;
        }

        switch (npc.engagementState) {
            case TopdownNpcEngagementState::Engaged:
                npc.combatState = TopdownNpcCombatState::None;
                npc.engagementState = TopdownNpcEngagementState::Investigating;
                TopdownStopNpcMovement(npc);
                return;

            case TopdownNpcEngagementState::Investigating:
                return;

            case TopdownNpcEngagementState::Unaware:
            default:
                npc.engagementState = TopdownNpcEngagementState::Investigating;
                return;
        }
    }

    if(npc.engagementState == TopdownNpcEngagementState::Investigating) {
        return;
    }

    npc.combatState = TopdownNpcCombatState::None;
    npc.engagementState = TopdownNpcEngagementState::Unaware;
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
            continue;
        }

        TopdownNpcPerceptionResult perception = EvaluateNpcPerception(state, npc);
        UpdateNpcEngagementState(state, npc, perception, dt);

        switch(npc.engagementState) {
            case TopdownNpcEngagementState::Unaware:
                // do nothing idle
                break;
            case TopdownNpcEngagementState::Investigating:
                DispatchNpcInvestigatingExecution(state, npc, perception, dt);
                break;
            case TopdownNpcEngagementState::Engaged:
                DispatchNpcEngagedExecution(state, npc, perception, dt);
                break;
        }
    }

    TopdownPruneNpcInvestigationContexts(state);
}
