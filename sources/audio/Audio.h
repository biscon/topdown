#pragma once

#include "data/GameState.h"

void InitAudio(GameState& state);
void ShutdownAudio(GameState& state);
void UpdateAudio(GameState& state, float dt);

bool PlaySoundById(GameState& state, const std::string& id);
bool PlaySoundById(GameState& state, const std::string& id, float pitch);
bool AudioPlaySoundAtPosition(GameState& state, const std::string& soundId, Vector2 position, float radius);
bool AudioPlaySoundAtPosition(GameState& state, const std::string& soundId, Vector2 position, float radius, float pitch);

constexpr float AUDIO_RADIUS_DOOR = 2000.0f;
constexpr float AUDIO_RADIUS_WINDOW = 2000.0f;
constexpr float AUDIO_RADIUS_NPC = 1800.0f;
constexpr float AUDIO_RADIUS_NPC_WEAPON = 2200.0f;

bool StopSoundById(GameState& state, const std::string& id);
bool IsSoundPlayingById(const GameState& state, const std::string& id);

bool PlayMusicById(GameState& state, const std::string& id, float fadeMs = 0.0f);
void StopMusic(GameState& state, float fadeMs = 0.0f);

bool PlaySoundEmitterById(GameState& state, const std::string& emitterId);
bool StopSoundEmitterById(GameState& state, const std::string& emitterId);

bool LoadLevelAudioDefinitions(GameState& state, const std::string& levelDir);
void ClearLevelAudio(GameState& state);
