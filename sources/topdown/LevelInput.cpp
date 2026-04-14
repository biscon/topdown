#include "topdown/LevelInput.h"
#include "input/Input.h"
#include "topdown/PlayerLoad.h"

void TopdownHandleInput(GameState& state)
{
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        switch (ev.key.key) {
            case KEY_F1:
                state.topdown.runtime.debug.showBlockers = !state.topdown.runtime.debug.showBlockers;
                ConsumeEvent(ev);
                break;

            case KEY_F2:
                state.topdown.runtime.debug.showTriggers = !state.topdown.runtime.debug.showTriggers;
                ConsumeEvent(ev);
                break;

            case KEY_F3:
                state.topdown.runtime.debug.showNav = !state.topdown.runtime.debug.showNav;
                ConsumeEvent(ev);
                break;

            case KEY_F4:
                state.topdown.runtime.debug.showPlayer = !state.topdown.runtime.debug.showPlayer;
                ConsumeEvent(ev);
                break;

            case KEY_F5:
                state.topdown.runtime.debug.showSpawnPoints = !state.topdown.runtime.debug.showSpawnPoints;
                ConsumeEvent(ev);
                break;

            case KEY_F6:
                state.topdown.runtime.debug.showEffects = !state.topdown.runtime.debug.showEffects;
                ConsumeEvent(ev);
                break;

            case KEY_F7:
                state.topdown.runtime.debug.showImageLayers = !state.topdown.runtime.debug.showImageLayers;
                ConsumeEvent(ev);
                break;

            case KEY_F8:
                state.topdown.runtime.debug.showCombatDebug = !state.topdown.runtime.debug.showCombatDebug;
                ConsumeEvent(ev);
                break;

            case KEY_F9:
                state.topdown.runtime.debug.showScriptDebug = !state.topdown.runtime.debug.showScriptDebug;
                ConsumeEvent(ev);
                break;

            case KEY_F10:
                state.topdown.runtime.debug.showAiDebug = !state.topdown.runtime.debug.showAiDebug;
                ConsumeEvent(ev);
                break;

            case KEY_F11:
                state.topdown.runtime.debug.showDoors = !state.topdown.runtime.debug.showDoors;
                ConsumeEvent(ev);
                break;

            case KEY_ONE:
            case KEY_TWO:
            case KEY_THREE:
            case KEY_FOUR:
            case KEY_FIVE:
            {
                if (!state.topdown.runtime.controlsEnabled || state.debug.console.open) {
                    break;
                }

                int slot = 0;
                switch (ev.key.key) {
                    case KEY_ONE:   slot = 1; break;
                    case KEY_TWO:   slot = 2; break;
                    case KEY_THREE: slot = 3; break;
                    case KEY_FOUR:  slot = 4; break;
                    case KEY_FIVE:  slot = 5; break;
                    default: break;
                }

                const TopdownPlayerWeaponConfig* weaponConfig =
                        FindTopdownPlayerWeaponConfigBySlot(state, slot);

                if (weaponConfig != nullptr &&
                    HasTopdownPlayerEquipmentAnimationSet(state, weaponConfig->equipmentSetId)) {
                    state.topdown.runtime.playerCharacter.equippedSetId = weaponConfig->equipmentSetId;
                    state.topdown.runtime.playerAttack.equipmentSetId = weaponConfig->equipmentSetId;
                    state.topdown.runtime.playerAttack.currentFireMode = weaponConfig->defaultFireMode;
                    state.topdown.runtime.playerAttack.triggerHeld = false;
                    state.topdown.runtime.playerAttack.pendingPrimaryAttack = false;
                    state.topdown.runtime.playerAttack.pendingSecondaryAttack = false;

                    TraceLog(LOG_INFO,
                             "Switched player weapon slot %d -> %s",
                             slot,
                             weaponConfig->equipmentSetId.c_str());
                } else {
                    TraceLog(LOG_WARNING,
                             "Player weapon slot %d is not configured or missing animation set",
                             slot);
                }

                ConsumeEvent(ev);
                break;
            }

            default:
                break;
        }
    }

    const bool inputBlocked =
            !state.topdown.runtime.controlsEnabled ||
            state.debug.console.open;

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseButtonPressed)) {
        if (inputBlocked) {
            break;
        }

        if (ev.mouse.button == MOUSE_LEFT_BUTTON) {
            state.topdown.runtime.playerAttack.pendingPrimaryAttack = true;
            state.topdown.runtime.playerAttack.triggerHeld = true;
            ConsumeEvent(ev);
        } else if (ev.mouse.button == MOUSE_RIGHT_BUTTON) {
            state.topdown.runtime.playerAttack.pendingSecondaryAttack = true;
            ConsumeEvent(ev);
        }
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseButtonReleased)) {
        if (ev.mouse.button == MOUSE_LEFT_BUTTON) {
            state.topdown.runtime.playerAttack.triggerHeld = false;
            ConsumeEvent(ev);
        }
    }

    if (!state.topdown.runtime.controlsEnabled || state.debug.console.open) {
        state.topdown.runtime.player.moveInputForward = 0.0f;
        state.topdown.runtime.player.moveInputRight = 0.0f;
        state.topdown.runtime.player.wantsRun = false;
        state.topdown.runtime.playerAttack.triggerHeld = false;
        state.topdown.runtime.playerAttack.pendingPrimaryAttack = false;
        state.topdown.runtime.playerAttack.pendingSecondaryAttack = false;
        return;
    }
}
