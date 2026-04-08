#pragma once
#include "data/GameState.h"

void UnloadSceneResources(ResourceData& resources);
void UnloadAllResources(ResourceData& resources);
SoundHandle LoadSoundResource(GameState& state, const std::string& path, ResourceScope scope);
MusicHandle LoadMusicResource(GameState& state, const std::string& path, ResourceScope scope);

Sound* GetSoundResource(GameState& state, SoundHandle handle);
Music* GetMusicResource(GameState& state, MusicHandle handle);