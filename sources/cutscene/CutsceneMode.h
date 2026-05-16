#pragma once

#include <string>

#include "data/GameState.h"

bool CutsceneQueueStart(GameState& state, const std::string& cutsceneId);
void CutsceneConsumePendingStart(GameState& state);
bool CutsceneStart(GameState& state, const std::string& cutsceneId);
void CutsceneStop(GameState& state);
void CutsceneComplete(GameState& state);
bool CutsceneRequestComplete(GameState& state);
bool CutsceneShowImage(GameState& state, const std::string& imageId, float fadeMs);
bool CutsceneShowText(GameState& state, const std::string& text, float fadeMs);
bool CutsceneClearText(GameState& state, float fadeMs);
bool CutsceneFadeFromBlack(GameState& state, float fadeMs);
bool CutsceneFadeToBlack(GameState& state, float fadeMs);
bool CutsceneHasActiveAction(const GameState& state);
void CutsceneUpdate(GameState& state, float dt);
void CutsceneHandleInput(GameState& state);
void CutsceneRenderUi(GameState& state);
bool CutsceneIsActive(const GameState& state);
