#pragma once

#include <string>
#include "data/GameState.h"

const DialogueChoiceSetData* FindDialogueChoiceSetById(
        const GameState& state,
        const std::string& choiceSetId);

bool StartDialogueChoice(
        GameState& state,
        const std::string& choiceSetId,
        const std::vector<std::string>* hiddenOptionIds = nullptr);

void CancelDialogueChoice(GameState& state);

bool IsDialogueUiActive(const GameState& state);

void UpdateDialogueUi(GameState& state);
void RenderDialogueUi(const GameState& state);
