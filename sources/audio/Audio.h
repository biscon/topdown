#pragma once

#include "data/GameState.h"

void InitAudio(GameState& state);
void ShutdownAudio(GameState& state);
void UpdateAudio(GameState& state, float dt);

bool PlaySoundById(GameState& state, const std::string& id);
bool PlaySoundById(GameState& state, const std::string& id, float pitch);

bool StopSoundById(GameState& state, const std::string& id);
bool IsSoundPlayingById(const GameState& state, const std::string& id);

bool PlayMusicById(GameState& state, const std::string& id, float fadeMs = 0.0f);
void StopMusic(GameState& state, float fadeMs = 0.0f);

bool PlaySoundEmitterById(GameState& state, const std::string& emitterId);
bool StopSoundEmitterById(GameState& state, const std::string& emitterId);

bool LoadSceneAudioDefinitions(GameState& state, const std::string& sceneDir);
void BuildSceneSoundEmitters(GameState& state);
void ClearSceneAudio(GameState& state);