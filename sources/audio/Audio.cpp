#include "audio/Audio.h"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include "audio/AudioAsset.h"
#include "resources/Resources.h"
#include "raylib.h"

static AudioDefinitionData* FindAudioDef(GameState& state, const std::string& id)
{
    auto it = state.audio.defIndexById.find(id);
    if (it == state.audio.defIndexById.end()) {
        return nullptr;
    }

    const int index = it->second;
    if (index < 0 || index >= static_cast<int>(state.audio.definitions.size())) {
        return nullptr;
    }

    return &state.audio.definitions[index];
}

static void WarnMissingAudioIdOnce(GameState& state, const std::string& id)
{
    for (const std::string& existing : state.audio.missingIdsLogged) {
        if (existing == id) {
            return;
        }
    }

    state.audio.missingIdsLogged.push_back(id);
    TraceLog(LOG_WARNING, "Audio id not found: %s", id.c_str());
}

static void RebuildAudioDefIndex(GameState& state)
{
    state.audio.defIndexById.clear();

    for (int i = 0; i < static_cast<int>(state.audio.definitions.size()); ++i) {
        state.audio.defIndexById[state.audio.definitions[i].id] = i;
    }
}

static void StopEmitterSound(SoundEmitterInstance& emitter)
{
    if (!emitter.active) {
        return;
    }

    StopSound(emitter.sound);
    UnloadSoundAlias(emitter.sound);
    emitter.sound = {};
    emitter.active = false;
}

static int FindLevelEmitterIndexById(const GameState& state, const std::string& emitterId)
{
    for (int i = 0; i < static_cast<int>(state.audio.levelEmitters.size()); ++i) {
        if (state.audio.levelEmitters[i].id == emitterId) {
            return i;
        }
    }

    return -1;
}

static float Clamp01(float t)
{
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

static bool ComputeEmitterPlaybackParams(const GameState& state,
                                         const SoundEmitterInstance& emitter,
                                         const AudioDefinitionData& def,
                                         float& outVolume,
                                         float& outPan)
{
    if (emitter.radius <= 0.0f) {
        return false;
    }

    const Vector2 listenerPos = state.topdown.runtime.player.position;
    const float dx = emitter.position.x - listenerPos.x;
    const float dy = emitter.position.y - listenerPos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist >= emitter.radius) {
        return false;
    }

    const float atten = std::pow(1.0f - Clamp01(dist / emitter.radius), 2.0f);

    outVolume =
            def.volume *
            state.settings.soundVolume *
            emitter.volume *
            emitter.authoredVolume *
            atten;

    outPan = 0.5f;
    if (emitter.pan) {
        float normPan = -dx / emitter.radius;
        if (normPan < -1.0f) normPan = -1.0f;
        if (normPan > 1.0f) normPan = 1.0f;

        const float maxPanAmount = 0.35f;
        outPan = 0.5f + normPan * maxPanAmount;
        if (outPan < 0.0f) outPan = 0.0f;
        if (outPan > 1.0f) outPan = 1.0f;
    }

    return true;
}

static void UpdateLevelSoundEmitters(GameState& state)
{
    if (!state.topdown.runtime.levelActive) {
        for (SoundEmitterInstance& emitter : state.audio.levelEmitters) {
            StopEmitterSound(emitter);
        }
        return;
    }

    for (SoundEmitterInstance& emitter : state.audio.levelEmitters) {
        if (!emitter.loop) {
            StopEmitterSound(emitter);
            continue;
        }

        if (!emitter.enabled || emitter.radius <= 0.0f) {
            StopEmitterSound(emitter);
            continue;
        }

        AudioDefinitionData* def = FindAudioDef(state, emitter.soundId);
        if (def == nullptr) {
            WarnMissingAudioIdOnce(state, emitter.soundId);
            StopEmitterSound(emitter);
            continue;
        }

        if (def->type != AudioType::Sound) {
            TraceLog(LOG_WARNING,
                     "Sound emitter '%s' references non-sound audio id '%s'",
                     emitter.id.c_str(),
                     emitter.soundId.c_str());
            StopEmitterSound(emitter);
            continue;
        }

        Sound* base = GetSoundResource(state, def->soundHandle);
        if (base == nullptr) {
            TraceLog(LOG_WARNING,
                     "Sound emitter '%s' missing sound resource for audio id '%s'",
                     emitter.id.c_str(),
                     emitter.soundId.c_str());
            StopEmitterSound(emitter);
            continue;
        }

        float finalVolume = 0.0f;
        float pan = 0.5f;
        if (!ComputeEmitterPlaybackParams(state, emitter, *def, finalVolume, pan)) {
            StopEmitterSound(emitter);
            continue;
        }

        if (!emitter.active) {
            emitter.sound = LoadSoundAlias(*base);
            if (emitter.sound.frameCount <= 0) {
                TraceLog(LOG_ERROR,
                         "Failed creating sound alias for emitter '%s'",
                         emitter.id.c_str());
                emitter.sound = {};
                emitter.active = false;
                continue;
            }

            emitter.active = true;
            SetSoundVolume(emitter.sound, finalVolume);
            SetSoundPan(emitter.sound, pan);
            PlaySound(emitter.sound);
        } else {
            if (!IsSoundPlaying(emitter.sound)) {
                PlaySound(emitter.sound);
            }

            SetSoundVolume(emitter.sound, finalVolume);
            SetSoundPan(emitter.sound, pan);
        }
    }
}

static MusicPlaybackState& GetCurrentMusic(AudioData& audio)
{
    return audio.musicAIsCurrent ? audio.musicA : audio.musicB;
}

static MusicPlaybackState& GetNextMusic(AudioData& audio)
{
    return audio.musicAIsCurrent ? audio.musicB : audio.musicA;
}

static void SwapMusicSlots(AudioData& audio)
{
    audio.musicAIsCurrent = !audio.musicAIsCurrent;
}

void InitAudio(GameState& state)
{
    if (state.audio.initialized) {
        return;
    }


    //SetAudioStreamBufferSizeDefault(16384);
    SetAudioStreamBufferSizeDefault(32768);
    InitAudioDevice();
    state.audio = {};
    state.audio.initialized = true;

    std::vector<AudioDefinitionData> defs;
    if (!LoadAudioDefinitions(ASSETS_PATH "audio/audio.json", defs)) {
        TraceLog(LOG_WARNING, "Global audio definitions not loaded");
        return;
    }

    for (AudioDefinitionData& def : defs) {
        def.scope = ResourceScope::Global;
    }

    state.audio.definitions = std::move(defs);
    RebuildAudioDefIndex(state);

    for (int i = 0; i < static_cast<int>(state.audio.definitions.size()); ++i) {
        AudioDefinitionData& def = state.audio.definitions[i];

        if (def.type == AudioType::Sound) {
            def.soundHandle = LoadSoundResource(state, def.filePath, ResourceScope::Global);
            if (def.soundHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving sound resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
            }
        } else {
            def.musicHandle = LoadMusicResource(state, def.filePath, ResourceScope::Global);
            if (def.musicHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving music resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
            }
        }
    }

    TraceLog(LOG_INFO,
             "Audio initialized with %d global definitions",
             static_cast<int>(state.audio.definitions.size()));
    TraceLog(LOG_INFO, "InitAudio: musicVolume=%.2f soundVolume=%.2f",
             state.settings.musicVolume,
             state.settings.soundVolume);
}

void ShutdownAudio(GameState& state)
{
    if (!state.audio.initialized) {
        return;
    }

    for (ActiveSoundInstance& inst : state.audio.activeSounds) {
        if (inst.active) {
            StopSound(inst.sound);
            UnloadSoundAlias(inst.sound);
            inst.sound = {};
            inst.active = false;
        }
    }

    state.audio.activeSounds.clear();

    ClearLevelAudio(state);

    MusicPlaybackState& current = GetCurrentMusic(state.audio);
    MusicPlaybackState& next = GetNextMusic(state.audio);

    if (current.playing && current.musicHandle >= 0) {
        Music* music = GetMusicResource(state, current.musicHandle);
        if (music != nullptr) {
            StopMusicStream(*music);
        }
    }

    if (next.playing && next.musicHandle >= 0) {
        Music* music = GetMusicResource(state, next.musicHandle);
        if (music != nullptr) {
            StopMusicStream(*music);
        }
    }

    state.audio.musicA = {};
    state.audio.musicB = {};
    state.audio.musicAIsCurrent = true;
    state.audio.levelEmitters.clear();

    CloseAudioDevice();
    state.audio = {};

    TraceLog(LOG_INFO, "Audio shutdown");
}

void UpdateAudio(GameState& state, float dt)
{
    if (!state.audio.initialized) {
        return;
    }

    auto UpdateMusicSlot = [&](MusicPlaybackState& m)
    {
        if (!m.playing || m.musicHandle < 0) {
            return;
        }

        Music* music = GetMusicResource(state, m.musicHandle);
        if (music == nullptr) {
            m = {};
            return;
        }

        UpdateMusicStream(*music);

        AudioDefinitionData* playingDef = nullptr;
        for (AudioDefinitionData& def : state.audio.definitions) {
            if (def.type == AudioType::Music && def.musicHandle == m.musicHandle) {
                playingDef = &def;
                break;
            }
        }

        if (playingDef == nullptr) {
            m = {};
            return;
        }

        // Non-looping music finished naturally: clear slot state.
        if (!playingDef->loop && !IsMusicStreamPlaying(*music)) {
            m = {};
            return;
        }

        const float settingsVolume = state.settings.musicVolume;
        const float baseTargetVolume = playingDef->volume;

        m.targetVolume = baseTargetVolume;

        if (m.fadeMode != MusicFadeMode::None && m.fadeDuration > 0.0f) {
            m.fadeElapsed += dt * 1000.0f;
            float t = m.fadeElapsed / m.fadeDuration;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            if (m.fadeMode == MusicFadeMode::FadeIn) {
                m.volume = m.targetVolume * t;

                if (t >= 1.0f) {
                    m.volume = m.targetVolume;
                    m.fadeMode = MusicFadeMode::None;
                }
            } else if (m.fadeMode == MusicFadeMode::FadeOut) {
                m.volume = m.targetVolume * (1.0f - t);

                if (t >= 1.0f) {
                    StopMusicStream(*music);
                    m = {};
                    return;
                }
            }
        } else {
            m.volume = m.targetVolume;
        }

        SetMusicVolume(*music, m.volume * settingsVolume);
    };

    UpdateMusicSlot(state.audio.musicA);
    UpdateMusicSlot(state.audio.musicB);

    MusicPlaybackState& current = GetCurrentMusic(state.audio);
    MusicPlaybackState& next = GetNextMusic(state.audio);

    if (!current.playing && next.playing) {
        SwapMusicSlots(state.audio);
    }

    for (auto it = state.audio.activeSounds.begin(); it != state.audio.activeSounds.end(); ) {
        if (!it->active) {
            it = state.audio.activeSounds.erase(it);
            continue;
        }

        if (!IsSoundPlaying(it->sound)) {
            if (it->loop) {
                AudioDefinitionData* playingDef = nullptr;
                for (AudioDefinitionData& def : state.audio.definitions) {
                    if (def.type == AudioType::Sound && def.soundHandle == it->baseSoundHandle) {
                        playingDef = &def;
                        break;
                    }
                }

                float volume = state.settings.soundVolume;
                if (playingDef != nullptr) {
                    volume *= playingDef->volume;
                }

                SetSoundVolume(it->sound, volume);
                PlaySound(it->sound);
                ++it;
                continue;
            }

            UnloadSoundAlias(it->sound);
            it = state.audio.activeSounds.erase(it);
        } else {
            ++it;
        }
    }

    UpdateLevelSoundEmitters(state);
}

bool PlaySoundById(GameState& state, const std::string& id)
{
    AudioDefinitionData* def = FindAudioDef(state, id);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, id);
        return false;
    }

    if (def->type != AudioType::Sound) {
        TraceLog(LOG_WARNING, "Audio id '%s' is not a sound", id.c_str());
        return false;
    }

    Sound* base = GetSoundResource(state, def->soundHandle);
    if (base == nullptr) {
        TraceLog(LOG_WARNING, "Sound resource missing for audio id '%s'", id.c_str());
        return false;
    }

    Sound alias = LoadSoundAlias(*base);
    if (alias.frameCount <= 0) {
        TraceLog(LOG_ERROR, "Failed creating sound alias for audio id '%s'", id.c_str());
        return false;
    }

    const float volume = def->volume * state.settings.soundVolume;
    SetSoundVolume(alias, volume);
    PlaySound(alias);

    ActiveSoundInstance inst;
    inst.sound = alias;
    inst.baseSoundHandle = def->soundHandle;
    inst.scope = def->scope;
    inst.loop = def->loop;
    inst.active = true;
    state.audio.activeSounds.push_back(inst);

    return true;
}

bool PlaySoundById(GameState& state, const std::string& id, float pitch)
{
    AudioDefinitionData* def = FindAudioDef(state, id);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, id);
        return false;
    }

    if (def->type != AudioType::Sound) {
        TraceLog(LOG_WARNING, "Audio id '%s' is not a sound", id.c_str());
        return false;
    }

    Sound* base = GetSoundResource(state, def->soundHandle);
    if (base == nullptr) {
        TraceLog(LOG_WARNING, "Sound resource missing for audio id '%s'", id.c_str());
        return false;
    }

    Sound alias = LoadSoundAlias(*base);
    if (alias.frameCount <= 0) {
        TraceLog(LOG_ERROR, "Failed creating sound alias for audio id '%s'", id.c_str());
        return false;
    }

    const float volume = def->volume * state.settings.soundVolume;

    SetSoundVolume(alias, volume);
    SetSoundPitch(alias, pitch);
    PlaySound(alias);

    ActiveSoundInstance inst;
    inst.sound = alias;
    inst.baseSoundHandle = def->soundHandle;
    inst.scope = def->scope;
    inst.loop = def->loop;
    inst.active = true;
    state.audio.activeSounds.push_back(inst);

    return true;
}

bool StopSoundById(GameState& state, const std::string& id)
{
    AudioDefinitionData* def = FindAudioDef(state, id);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, id);
        return false;
    }

    if (def->type != AudioType::Sound) {
        TraceLog(LOG_WARNING, "Audio id '%s' is not a sound", id.c_str());
        return false;
    }

    bool stoppedAny = false;

    for (auto it = state.audio.activeSounds.begin(); it != state.audio.activeSounds.end(); ) {
        if (it->active && it->baseSoundHandle == def->soundHandle) {
            StopSound(it->sound);
            UnloadSoundAlias(it->sound);
            it = state.audio.activeSounds.erase(it);
            stoppedAny = true;
        } else {
            ++it;
        }
    }

    return stoppedAny;
}

bool IsSoundPlayingById(const GameState& state, const std::string& id)
{
    auto it = state.audio.defIndexById.find(id);
    if (it == state.audio.defIndexById.end()) {
        return false;
    }

    const int defIndex = it->second;
    if (defIndex < 0 || defIndex >= (int)state.audio.definitions.size()) {
        return false;
    }

    const AudioDefinitionData& def = state.audio.definitions[defIndex];

    for (const ActiveSoundInstance& inst : state.audio.activeSounds) {
        if (inst.active && inst.baseSoundHandle == def.soundHandle) {
            return true;
        }
    }

    return false;
}

bool PlayMusicById(GameState& state, const std::string& id, float fadeMs)
{
    TraceLog(LOG_INFO, "PlayMusicById called: id=%s fadeMs=%.2f", id.c_str(), fadeMs);
    AudioDefinitionData* def = FindAudioDef(state, id);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, id);
        return false;
    }

    if (def->type != AudioType::Music) {
        TraceLog(LOG_WARNING, "Audio id '%s' is not music", id.c_str());
        return false;
    }

    Music* music = GetMusicResource(state, def->musicHandle);
    if (music == nullptr) {
        TraceLog(LOG_WARNING, "Music resource missing for audio id '%s'", id.c_str());
        return false;
    }

    MusicPlaybackState& current = GetCurrentMusic(state.audio);
    MusicPlaybackState& next = GetNextMusic(state.audio);

    // Same track already current and no pending second slot
    if (current.playing &&
        current.musicHandle == def->musicHandle &&
        !next.playing) {
        current.targetVolume = def->volume;
        current.fadeMode = MusicFadeMode::None;
        current.fadeElapsed = 0.0f;
        current.fadeDuration = 0.0f;
        return true;
    }

    // Clear any previous next slot
    if (next.playing && next.musicHandle >= 0) {
        Music* nextMusic = GetMusicResource(state, next.musicHandle);
        if (nextMusic != nullptr) {
            StopMusicStream(*nextMusic);
        }
        next = {};
    }

    next.musicHandle = def->musicHandle;
    next.playing = true;
    next.targetVolume = def->volume;
    next.fadeElapsed = 0.0f;
    next.fadeDuration = fadeMs;

    if (fadeMs > 0.0f) {
        next.volume = 0.0f;
        next.fadeMode = MusicFadeMode::FadeIn;
    } else {
        next.volume = def->volume;
        next.fadeMode = MusicFadeMode::None;
    }
    StopMusicStream(*music);
    SeekMusicStream(*music, 0.0f);
    music->looping = def->loop;
    PlayMusicStream(*music);
    SetMusicVolume(*music, next.volume * state.settings.musicVolume);

    if (current.playing && current.musicHandle >= 0) {
        if (fadeMs > 0.0f) {
            current.fadeMode = MusicFadeMode::FadeOut;
            current.fadeElapsed = 0.0f;
            current.fadeDuration = fadeMs;
        } else {
            Music* oldMusic = GetMusicResource(state, current.musicHandle);
            if (oldMusic != nullptr) {
                StopMusicStream(*oldMusic);
            }
            current = {};
        }
    }

    if (!current.playing && next.playing) {
        SwapMusicSlots(state.audio);
    }

    return true;
}

bool PlaySoundEmitterById(GameState& state, const std::string& emitterId)
{
    if (!state.topdown.runtime.levelActive) {
        return false;
    }

    const int emitterIndex = FindLevelEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.levelEmitters.size())) {
        return false;
    }

    SoundEmitterInstance& emitter = state.audio.levelEmitters[emitterIndex];

    if (emitter.loop) {
        emitter.enabled = true;
        return true;
    }

    AudioDefinitionData* def = FindAudioDef(state, emitter.soundId);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, emitter.soundId);
        return false;
    }

    if (def->type != AudioType::Sound) {
        TraceLog(LOG_WARNING,
                 "Triggered emitter '%s' references non-sound audio id '%s'",
                 emitter.id.c_str(),
                 emitter.soundId.c_str());
        return false;
    }

    Sound* base = GetSoundResource(state, def->soundHandle);
    if (base == nullptr) {
        TraceLog(LOG_WARNING,
                 "Triggered emitter '%s' missing sound resource for audio id '%s'",
                 emitter.id.c_str(),
                 emitter.soundId.c_str());
        return false;
    }

    float finalVolume = 0.0f;
    float pan = 0.5f;
    if (!ComputeEmitterPlaybackParams(state, emitter, *def, finalVolume, pan)) {
        return false;
    }

    Sound alias = LoadSoundAlias(*base);
    if (alias.frameCount <= 0) {
        TraceLog(LOG_ERROR,
                 "Failed creating triggered sound alias for emitter '%s'",
                 emitter.id.c_str());
        return false;
    }

    SetSoundVolume(alias, finalVolume);
    SetSoundPan(alias, pan);
    PlaySound(alias);

    ActiveSoundInstance inst;
    inst.sound = alias;
    inst.baseSoundHandle = def->soundHandle;
    inst.scope = def->scope;
    inst.loop = false;
    inst.active = true;
    state.audio.activeSounds.push_back(inst);

    return true;
}

bool StopSoundEmitterById(GameState& state, const std::string& emitterId)
{
    if (!state.topdown.runtime.levelActive) {
        return false;
    }

    const int emitterIndex = FindLevelEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.levelEmitters.size())) {
        return false;
    }

    SoundEmitterInstance& emitter = state.audio.levelEmitters[emitterIndex];

    if (!emitter.loop) {
        return false;
    }

    emitter.enabled = false;
    StopEmitterSound(emitter);
    return true;
}

void StopMusic(GameState& state, float fadeMs)
{
    MusicPlaybackState& current = GetCurrentMusic(state.audio);
    MusicPlaybackState& next = GetNextMusic(state.audio);

    if (next.playing && next.musicHandle >= 0) {
        Music* music = GetMusicResource(state, next.musicHandle);
        if (music != nullptr) {
            StopMusicStream(*music);
        }
        next = {};
    }

    if (!current.playing || current.musicHandle < 0) {
        return;
    }

    Music* music = GetMusicResource(state, current.musicHandle);
    if (music == nullptr) {
        current = {};
        return;
    }

    if (fadeMs <= 0.0f) {
        StopMusicStream(*music);
        current = {};
        return;
    }

    current.fadeMode = MusicFadeMode::FadeOut;
    current.fadeElapsed = 0.0f;
    current.fadeDuration = fadeMs;
}

bool LoadLevelAudioDefinitions(GameState& state, const std::string& levelDir)
{
    namespace fs = std::filesystem;

    const fs::path audioJsonPath = fs::path(levelDir) / "audio.json";
    if (!fs::exists(audioJsonPath)) {
        return true;
    }

    std::vector<AudioDefinitionData> defs;
    if (!LoadAudioDefinitions(audioJsonPath.lexically_normal().string(), defs)) {
        TraceLog(LOG_ERROR, "Failed loading level audio definitions: %s", audioJsonPath.string().c_str());
        return false;
    }

    for (AudioDefinitionData& def : defs) {
        def.scope = ResourceScope::Scene;

        if (state.audio.defIndexById.find(def.id) != state.audio.defIndexById.end()) {
            TraceLog(LOG_ERROR, "Level audio id collides with existing audio id: %s", def.id.c_str());
            return false;
        }

        if (def.type == AudioType::Sound) {
            def.soundHandle = LoadSoundResource(state, def.filePath, ResourceScope::Scene);
            if (def.soundHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving level sound resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
                return false;
            }
        } else {
            def.musicHandle = LoadMusicResource(state, def.filePath, ResourceScope::Scene);
            if (def.musicHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving level music resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
                return false;
            }
        }

        state.audio.definitions.push_back(def);
    }

    RebuildAudioDefIndex(state);

    TraceLog(LOG_INFO,
             "Loaded level audio definitions: %d from %s",
             static_cast<int>(defs.size()),
             audioJsonPath.string().c_str());

    return true;
}

void ClearLevelAudio(GameState& state)
{
    for (SoundEmitterInstance& emitter : state.audio.levelEmitters) {
        if (emitter.active) {
            StopSound(emitter.sound);
            UnloadSoundAlias(emitter.sound);
            emitter.sound = {};
            emitter.active = false;
        }
    }

    state.audio.levelEmitters.clear();
    for (auto it = state.audio.activeSounds.begin(); it != state.audio.activeSounds.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            if (it->active) {
                StopSound(it->sound);
                UnloadSoundAlias(it->sound);
            }
            it = state.audio.activeSounds.erase(it);
        } else {
            ++it;
        }
    }

    auto StopLevelScopedMusicIfNeeded = [&](MusicPlaybackState& slot)
    {
        if (!slot.playing || slot.musicHandle < 0) {
            return;
        }

        AudioDefinitionData* playingDef = nullptr;
        for (AudioDefinitionData& def : state.audio.definitions) {
            if (def.type == AudioType::Music && def.musicHandle == slot.musicHandle) {
                playingDef = &def;
                break;
            }
        }

        if (playingDef != nullptr && playingDef->scope == ResourceScope::Scene) {
            Music* music = GetMusicResource(state, slot.musicHandle);
            if (music != nullptr) {
                StopMusicStream(*music);
            }
            slot = {};
        }
    };

    StopLevelScopedMusicIfNeeded(state.audio.musicA);
    StopLevelScopedMusicIfNeeded(state.audio.musicB);

    state.audio.definitions.erase(
            std::remove_if(
                    state.audio.definitions.begin(),
                    state.audio.definitions.end(),
                    [](const AudioDefinitionData& def) {
                        return def.scope == ResourceScope::Scene;
                    }),
            state.audio.definitions.end());

    RebuildAudioDefIndex(state);
}
