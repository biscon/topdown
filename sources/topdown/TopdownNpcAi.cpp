#include "topdown/TopdownNpcAi.h"

void TopdownUpdateNpcAi(GameState& state, float dt)
{
    if (state.topdown.runtime.aiFrozen) {
        for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
            if (!npc.active || !npc.hostile) {
                continue;
            }

            TopdownStopNpcMovement(npc);
            npc.combatState = TopdownNpcCombatState::None;
            npc.attackHitPending = false;
            npc.attackHitApplied = false;
            npc.attackStateTimeMs = 0.0f;
            npc.attackAnimationDurationMs = 0.0f;
            npc.currentVelocity = Vector2{};
        }
        return;
    }

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        switch (npc.aiMode) {
            case TopdownNpcAiMode::SeekAndDestroy:
                TopdownUpdateNpcAiSeekAndDestroy(state, npc, dt);
                break;

            case TopdownNpcAiMode::None:
            default:
                break;
        }
    }
}
