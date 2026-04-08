#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"
#include "resources/ResourceData.h"

// ------------------------------------------------------------
// Basic audio enums
// ------------------------------------------------------------

enum class AudioType {
    Sound,
    Music
};

enum class AudioGroup {
    Sound,
    Music
};

// ------------------------------------------------------------
// Audio definition (id-based, filled later in Patch 2)
// ------------------------------------------------------------

struct AudioDefinitionData {
    std::string id;

    AudioType type = AudioType::Sound;
    AudioGroup group = AudioGroup::Sound;

    std::string filePath;
    ResourceScope scope = ResourceScope::Global;

    float volume = 1.0f;
    bool loop = false;

    // resolved resource handles (filled later)
    int soundHandle = -1;
    int musicHandle = -1;
};

// ------------------------------------------------------------
// Runtime sound instance (for alias tracking)
// ------------------------------------------------------------

struct ActiveSoundInstance {
    Sound sound{};        // alias instance
    int baseSoundHandle = -1;
    ResourceScope scope = ResourceScope::Global;
    bool loop = false;
    bool active = false;
};

// ------------------------------------------------------------
// Music playback state
// ------------------------------------------------------------

enum class MusicFadeMode {
    None,
    FadeIn,
    FadeOut
};

struct MusicPlaybackState {
    int musicHandle = -1;
    bool playing = false;

    float volume = 0.0f;
    float targetVolume = 1.0f;

    float fadeElapsed = 0.0f;
    float fadeDuration = 0.0f;

    MusicFadeMode fadeMode = MusicFadeMode::None;
};

// ------------------------------------------------------------
// Scene sound emitter runtime
// ------------------------------------------------------------

struct SoundEmitterInstance {
    int sceneEmitterIndex = -1;

    bool enabled = true;
    float volume = 1.0f;

    bool active = false;
    Sound sound{};
};

// ------------------------------------------------------------
// AudioData (top-level subsystem state)
// ------------------------------------------------------------

struct AudioData {
    bool initialized = false;

    // definitions (filled later)
    std::vector<AudioDefinitionData> definitions;

    // runtime
    std::vector<ActiveSoundInstance> activeSounds;

    MusicPlaybackState musicA;
    MusicPlaybackState musicB;
    bool musicAIsCurrent = true;

    std::vector<SoundEmitterInstance> sceneEmitters;

    // debug: missing id suppression (filled later)
    std::vector<std::string> missingIdsLogged;

    std::unordered_map<std::string, int> defIndexById;
};
