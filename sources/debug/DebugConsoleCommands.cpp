#include "debug/DebugConsoleInternal.h"
#include "debug/DebugConsole.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "adventure/AdventureUpdate.h"
#include "adventure/AdventureHelpers.h"
#include "audio/Audio.h"
#include "save/SaveGame.h"
#include "resources/TextureAsset.h"
#include "render/EffectShaderRegistry.h"
#include "topdown/LevelRegistry.h"

static std::vector<std::string> SplitConsoleWords(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string part;

    while (in >> part) {
        out.push_back(part);
    }

    return out;
}

static bool TryParseConsoleSlotIndex(const std::string& text, int& outSlotIndex)
{
    outSlotIndex = -1;

    if (text.empty()) {
        return false;
    }

    char* endPtr = nullptr;
    const long value = std::strtol(text.c_str(), &endPtr, 10);
    if (endPtr == text.c_str() || *endPtr != '\0') {
        return false;
    }

    if (value < 1 || value > 999) {
        return false;
    }

    outSlotIndex = static_cast<int>(value);
    return true;
}

static bool TryParseConsoleBoolArg(const std::string& text, bool& outValue)
{
    if (text == "1" || text == "on" || text == "true") {
        outValue = true;
        return true;
    }

    if (text == "0" || text == "off" || text == "false") {
        outValue = false;
        return true;
    }

    return false;
}

static const char* TopdownImageLayerKindToText(TopdownImageLayerKind kind)
{
    switch (kind) {
        case TopdownImageLayerKind::Bottom:
            return "bottom";
        case TopdownImageLayerKind::Top:
            return "top";
        default:
            return "unknown";
    }
}

static const char* TopdownEffectPlacementToText(TopdownEffectPlacement placement)
{
    switch (placement) {
        case TopdownEffectPlacement::AfterBottom:
            return "after_bottom";
        case TopdownEffectPlacement::AfterCharacters:
            return "after_characters";
        case TopdownEffectPlacement::Final:
            return "final";
        default:
            return "unknown";
    }
}

static const char* EffectBlendModeToText(EffectBlendMode mode)
{
    switch (mode) {
        case EffectBlendMode::Add:
            return "add";
        case EffectBlendMode::Multiply:
            return "multiply";
        case EffectBlendMode::Normal:
        default:
            return "normal";
    }
}

static void QueueTopdownLevelChange(
        GameState& state,
        const std::string& levelId,
        const std::string& spawnId = "")
{
    state.topdown.hasPendingLevelChange = true;
    state.topdown.pendingLevelId = levelId;
    state.topdown.pendingSpawnId = spawnId;
}

bool ExecuteConsoleSlashCommand(GameState& state, const std::string& line)
{
    const std::vector<std::string> args = SplitConsoleWords(line);
    if (args.empty()) {
        return true;
    }

    const std::string& cmd = args[0];

    if (cmd == "/help") {
        DebugConsoleAddLine(state, "Console commands:", SKYBLUE);
        DebugConsoleAddLine(state, "  /help", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /quit", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /clear", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /copylast [numLines]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /goto <levelId>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /reload", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /levels", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /resources", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /layers", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /effects", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /spawns", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /audio", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /play <audioId>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /music <audioId> [fadeMs]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /stopmusic [fadeMs]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /save <slot>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /load <slot>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /saves", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /god [on|off]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /aifreeze [on|off]", LIGHTGRAY);
        return true;
    }

    if (cmd == "/clear") {
        state.debug.console.lines.clear();
        state.debug.console.scrollOffset = 0;
        return true;
    }

    if (cmd == "/quit") {
        state.mode = GameMode::Quit;
        return true;
    }

    if (cmd == "/god" || cmd == "/godmode") {
        bool newValue = !state.topdown.runtime.godMode;

        if (args.size() >= 2) {
            if (!TryParseConsoleBoolArg(args[1], newValue)) {
                DebugConsoleAddLine(state, "usage: /god [on|off]", RED);
                return true;
            }
        }

        state.topdown.runtime.godMode = newValue;

        DebugConsoleAddLine(
                state,
                std::string("god mode: ") + (state.topdown.runtime.godMode ? "ON" : "OFF"),
                SKYBLUE);
        return true;
    }

    if (cmd == "/aifreeze") {
        bool newValue = !state.topdown.runtime.aiFrozen;

        if (args.size() >= 2) {
            if (!TryParseConsoleBoolArg(args[1], newValue)) {
                DebugConsoleAddLine(state, "usage: /aifreeze [on|off]", RED);
                return true;
            }
        }

        state.topdown.runtime.aiFrozen = newValue;

        if (state.topdown.runtime.aiFrozen) {
            for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
                if (!npc.active || !npc.hostile) {
                    continue;
                }

                npc.move = {};
                npc.moving = false;
                npc.running = false;

                npc.combatState = TopdownNpcCombatState::None;
                npc.attackHitPending = false;
                npc.attackHitApplied = false;
                npc.attackStateTimeMs = 0.0f;
                npc.attackAnimationDurationMs = 0.0f;
            }
        }

        DebugConsoleAddLine(
                state,
                std::string("AI freeze: ") + (state.topdown.runtime.aiFrozen ? "ON" : "OFF"),
                SKYBLUE);
        return true;
    }

    if (cmd == "/goto") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /goto <levelId> [spawnId]", RED);
            return true;
        }

        const std::string& levelId = args[1];

        if (args.size() >= 3) {
            const std::string& spawnId = args[2];
            QueueTopdownLevelChange(state, levelId, spawnId);
            DebugConsoleAddLine(state, "queued level load: " + levelId + " spawn=" + spawnId, SKYBLUE);
        } else {
            QueueTopdownLevelChange(state, levelId);
            DebugConsoleAddLine(state, "queued level load: " + levelId, SKYBLUE);
        }

        return true;
    }

    if (cmd == "/reload") {
        if (state.topdown.currentLevelId.empty()) {
            DebugConsoleAddLine(state, "no level loaded", RED);
            return true;
        }

        const std::string levelId = state.topdown.currentLevelId;
        const std::string spawnId = state.topdown.pendingSpawnId;

        QueueTopdownLevelChange(state, levelId, spawnId);
        DebugConsoleAddLine(state, "queued reload: " + levelId, SKYBLUE);
        return true;
    }

    if (cmd == "/levels") {
        DebugConsoleAddLine(state, "levels:", SKYBLUE);

        if (state.topdown.levelRegistry.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const TopdownLevelRegistryEntry& entry : state.topdown.levelRegistry) {
            std::string line = "  " + entry.levelId;

            if (!entry.saveName.empty()) {
                line += "  saveName=" + entry.saveName;
            }

            line += "  baseScale=" + std::to_string(entry.baseAssetScale);
            DebugConsoleAddLine(state, line, LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/save") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /save <slot>", RED);
            return true;
        }

        int slotIndex = -1;
        if (!TryParseConsoleSlotIndex(args[1], slotIndex)) {
            DebugConsoleAddLine(state, "invalid slot index", RED);
            return true;
        }

        if (SaveGameToSlot(state, slotIndex)) {
            DebugConsoleAddLine(
                    state,
                    "saved slot " + std::to_string(slotIndex) + ": " + GetSaveSlotSummary(slotIndex),
                    SKYBLUE);
        } else {
            DebugConsoleAddLine(
                    state,
                    "failed saving slot " + std::to_string(slotIndex),
                    RED);
        }

        return true;
    }

    if (cmd == "/load") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /load <slot>", RED);
            return true;
        }

        int slotIndex = -1;
        if (!TryParseConsoleSlotIndex(args[1], slotIndex)) {
            DebugConsoleAddLine(state, "invalid slot index", RED);
            return true;
        }

        if (!DoesSaveSlotExist(slotIndex)) {
            DebugConsoleAddLine(
                    state,
                    "save slot " + std::to_string(slotIndex) + " is empty",
                    RED);
            return true;
        }

        if (LoadGameFromSlot(state, slotIndex)) {
            DebugConsoleAddLine(
                    state,
                    "loaded slot " + std::to_string(slotIndex) + ": " + GetSaveSlotSummary(slotIndex),
                    SKYBLUE);
        } else {
            DebugConsoleAddLine(
                    state,
                    "failed loading slot " + std::to_string(slotIndex),
                    RED);
        }

        return true;
    }

    if (cmd == "/saves") {
        DebugConsoleAddLine(state, "save slots:", SKYBLUE);

        for (int slot = 1; slot <= 8; ++slot) {
            const std::string summary = GetSaveSlotSummary(slot);
            DebugConsoleAddLine(
                    state,
                    "  [" + std::to_string(slot) + "] " + summary,
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/resources") {
        auto TextureFilterModeToText = [](TextureFilterMode mode) -> const char* {
            switch (mode) {
                case TextureFilterMode::Bilinear:
                    return "bilinear";
                case TextureFilterMode::Point:
                default:
                    return "point";
            }
        };

        auto TextureWrapModeToText = [](TextureWrapMode mode) -> const char* {
            switch (mode) {
                case TextureWrapMode::Repeat:
                    return "repeat";
                case TextureWrapMode::Clamp:
                default:
                    return "clamp";
            }
        };

        DebugConsoleAddLine(
                state,
                TextFormat("textures: %d", static_cast<int>(state.resources.textures.size())),
                SKYBLUE);

        for (const TextureResource& tex : state.resources.textures) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [tex %d] %s", tex.handle, tex.path.c_str()),
                    LIGHTGRAY);

            DebugConsoleAddLine(
                    state,
                    TextFormat("      pma=%s filter=%s wrap=%s size=%dx%d scope=%s",
                               tex.premultiplyAlpha ? "true" : "false",
                               TextureFilterModeToText(tex.filterMode),
                               TextureWrapModeToText(tex.wrapMode),
                               tex.texture.width,
                               tex.texture.height,
                               tex.scope == ResourceScope::Global ? "global" : "scene"),
                    GRAY);
        }

        DebugConsoleAddLine(
                state,
                TextFormat("sprite assets: %d", static_cast<int>(state.resources.spriteAssets.size())),
                SKYBLUE);

        for (const SpriteAssetResource& asset : state.resources.spriteAssets) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [sprite %d] %s", asset.handle, asset.cacheKey.c_str()),
                    LIGHTGRAY);
        }

        DebugConsoleAddLine(
                state,
                TextFormat("sounds: %d", static_cast<int>(state.resources.sounds.size())),
                SKYBLUE);

        for (const SoundResource& sound : state.resources.sounds) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [sound %d] %s", sound.handle, sound.path.c_str()),
                    LIGHTGRAY);
        }

        DebugConsoleAddLine(
                state,
                TextFormat("music streams: %d", static_cast<int>(state.resources.musics.size())),
                SKYBLUE);

        for (const MusicResource& music : state.resources.musics) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [music %d] %s", music.handle, music.path.c_str()),
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/flags") {
        DebugConsoleAddLine(state, "bool flags:", SKYBLUE);
        if (state.script.flags.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const auto& kv : state.script.flags) {
                DebugConsoleAddLine(
                        state,
                        "  " + kv.first + " = " + (kv.second ? "true" : "false"),
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "int flags:", SKYBLUE);
        if (state.script.ints.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const auto& kv : state.script.ints) {
                DebugConsoleAddLine(
                        state,
                        "  " + kv.first + " = " + std::to_string(kv.second),
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "string flags:", SKYBLUE);
        if (state.script.strings.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const auto& kv : state.script.strings) {
                DebugConsoleAddLine(
                        state,
                        "  " + kv.first + " = \"" + kv.second + "\"",
                        LIGHTGRAY);
            }
        }

        return true;
    }

    if (cmd == "/items") {
        DebugConsoleAddLine(state, "item definitions:", SKYBLUE);

        if (state.adventure.itemDefinitions.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const ItemDefinitionData& item : state.adventure.itemDefinitions) {
                DebugConsoleAddLine(
                        state,
                        "  " + item.itemId + "  (" + item.displayName + ")",
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "inventories:", SKYBLUE);
        if (state.adventure.actorInventories.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const ActorInventoryData& inv : state.adventure.actorInventories) {
                std::string lineText = "  " + inv.actorId + ":";
                if (inv.itemIds.empty()) {
                    lineText += " <empty>";
                } else {
                    for (const std::string& itemId : inv.itemIds) {
                        lineText += " " + itemId;
                    }
                }

                if (!inv.heldItemId.empty()) {
                    lineText += "   held=" + inv.heldItemId;
                }

                DebugConsoleAddLine(state, lineText, LIGHTGRAY);
            }
        }

        return true;
    }

    if (cmd == "/effects") {
        if (!state.topdown.authored.loaded) {
            DebugConsoleAddLine(state, "no topdown level loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "effect regions:", SKYBLUE);

        if (state.topdown.authored.effectRegions.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (int i = 0; i < static_cast<int>(state.topdown.authored.effectRegions.size()); ++i) {
            const TopdownAuthoredEffectRegion& authored = state.topdown.authored.effectRegions[i];
            const TopdownRuntimeEffectRegion& runtime = state.topdown.runtime.render.effectRegions[i];

            const std::string shapeText =
                    authored.usePolygon
                    ? "poly(" + std::to_string(static_cast<int>(authored.polygon.size())) + ")"
                    : "rect";

            std::string line =
                    "  " + authored.id +
                    " shape=" + shapeText +
                    " placement=" + TopdownEffectPlacementToText(authored.placement) +
                    " sort=" + std::to_string(authored.sortIndex) +
                    " shader=" + std::string(EffectShaderTypeToString(runtime.shaderType)) +
                    " blend=" + EffectBlendModeToText(authored.blendMode) +
                    " visible=" + std::string(runtime.visible ? "true" : "false") +
                    " opacity=" + std::to_string(runtime.opacity) +
                    " walls=" + std::string(runtime.occludedByWalls ? "true" : "false");

            DebugConsoleAddLine(state, line, LIGHTGRAY);

            if (authored.usePolygon) {
                DebugConsoleAddLine(
                        state,
                        "      image=" + authored.imagePath +
                        " occlusionPoly=" + std::string(runtime.hasWallOcclusionPolygon ? "true" : "false") +
                        " originOverride=" + std::string(authored.hasOcclusionOriginOverride ? "true" : "false"),
                        GRAY);
            } else {
                DebugConsoleAddLine(
                        state,
                        "      rect=(" +
                        std::to_string(static_cast<int>(authored.worldRect.x)) + "," +
                        std::to_string(static_cast<int>(authored.worldRect.y)) + "," +
                        std::to_string(static_cast<int>(authored.worldRect.width)) + "," +
                        std::to_string(static_cast<int>(authored.worldRect.height)) + ")" +
                        " image=" + authored.imagePath,
                        GRAY);
            }
        }

        return true;
    }

    if (cmd == "/exits") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "exits:", SKYBLUE);

        if (state.adventure.currentScene.exits.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const SceneExit& exitObj : state.adventure.currentScene.exits) {
            DebugConsoleAddLine(
                    state,
                    "  " + exitObj.id +
                    "  (" + exitObj.displayName + ")" +
                    " -> scene=" + exitObj.targetScene +
                    " spawn=" + exitObj.targetSpawn,
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/spawns") {
        if (!state.topdown.authored.loaded) {
            DebugConsoleAddLine(state, "no topdown level loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "spawns:", SKYBLUE);

        if (state.topdown.authored.spawns.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const TopdownAuthoredSpawn& spawn : state.topdown.authored.spawns) {
            DebugConsoleAddLine(
                    state,
                    "  " + spawn.id +
                    " pos=(" +
                    std::to_string(static_cast<int>(spawn.position.x)) + "," +
                    std::to_string(static_cast<int>(spawn.position.y)) + ")" +
                    " orientation=" +
                    std::to_string(static_cast<int>(spawn.orientationDegrees)) +
                    " visible=" + std::string(spawn.visible ? "true" : "false"),
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/audio") {
        DebugConsoleAddLine(state, "audio definitions:", SKYBLUE);

        if (state.audio.definitions.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const AudioDefinitionData& def : state.audio.definitions) {
            const std::string typeText = (def.type == AudioType::Sound) ? "sound" : "music";
            const std::string scopeText = (def.scope == ResourceScope::Global) ? "global" : "scene";

            DebugConsoleAddLine(
                    state,
                    "  " + def.id + "  type=" + typeText + " scope=" + scopeText + " file=" + def.filePath,
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/emitters") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "sound emitters:", SKYBLUE);

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.soundEmitters.size()),
                static_cast<int>(state.audio.sceneEmitters.size()));

        if (count <= 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (int i = 0; i < count; ++i) {
            const SceneSoundEmitterData& sceneEmitter = state.adventure.currentScene.soundEmitters[i];
            const SoundEmitterInstance& emitter = state.audio.sceneEmitters[i];

            std::string line =
                    "  " + sceneEmitter.id +
                    " sound=" + sceneEmitter.soundId +
                    " radius=" + std::to_string(static_cast<int>(sceneEmitter.radius)) +
                    " loop=" + std::string(sceneEmitter.loop ? "true" : "false") +
                    " enabled=" + std::string(emitter.enabled ? "true" : "false") +
                    " active=" + std::string(emitter.active ? "true" : "false") +
                    " volume=" + std::to_string(emitter.volume);

            DebugConsoleAddLine(state, line, LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/play") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /play <audioId>", RED);
            return true;
        }

        if (PlaySoundById(state, args[1])) {
            DebugConsoleAddLine(state, "played sound: " + args[1], SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "failed playing sound: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/music") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /music <audioId> [fadeMs]", RED);
            return true;
        }

        float fadeMs = 0.0f;
        if (args.size() >= 3) {
            try {
                fadeMs = std::stof(args[2]);
            } catch (...) {
                DebugConsoleAddLine(state, "usage: /music <audioId> [fadeMs]", RED);
                return true;
            }
        }

        if (PlayMusicById(state, args[1], fadeMs)) {
            if (fadeMs > 0.0f) {
                DebugConsoleAddLine(
                        state,
                        "playing music: " + args[1] +
                        " (fade " + std::to_string(static_cast<int>(fadeMs)) + " ms)",
                        SKYBLUE);
            } else {
                DebugConsoleAddLine(state, "playing music: " + args[1], SKYBLUE);
            }
        } else {
            DebugConsoleAddLine(state, "failed playing music: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/stopmusic") {
        float fadeMs = 0.0f;

        if (args.size() >= 2) {
            try {
                fadeMs = std::stof(args[1]);
            } catch (...) {
                DebugConsoleAddLine(state, "usage: /stopmusic [fadeMs]", RED);
                return true;
            }
        }

        StopMusic(state, fadeMs);

        if (fadeMs > 0.0f) {
            DebugConsoleAddLine(
                    state,
                    "stopping music with fade: " + std::to_string(static_cast<int>(fadeMs)) + " ms",
                    SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "stopped music", SKYBLUE);
        }

        return true;
    }

    if (cmd == "/playemitter") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /playemitter <emitterId>", RED);
            return true;
        }

        if (PlaySoundEmitterById(state, args[1])) {
            DebugConsoleAddLine(state, "played emitter: " + args[1], SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "failed playing emitter: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/stopemitter") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /stopemitter <emitterId>", RED);
            return true;
        }

        if (StopSoundEmitterById(state, args[1])) {
            DebugConsoleAddLine(state, "stopped emitter: " + args[1], SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "failed stopping emitter: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/layers") {
        if (!state.topdown.authored.loaded) {
            DebugConsoleAddLine(state, "no topdown level loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "image layers:", SKYBLUE);

        if (state.topdown.authored.imageLayers.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const TopdownAuthoredImageLayer& layer : state.topdown.authored.imageLayers) {
            std::string line =
                    "  " + layer.name +
                    " kind=" + TopdownImageLayerKindToText(layer.kind) +
                    " visible=" + std::string(layer.visible ? "true" : "false") +
                    " opacity=" + std::to_string(layer.opacity) +
                    " scale=" + std::to_string(layer.scale) +
                    " blend=" + EffectBlendModeToText(layer.blendMode) +
                    " shader=" + std::string(EffectShaderTypeToString(layer.shaderType));

            DebugConsoleAddLine(state, line, LIGHTGRAY);

            DebugConsoleAddLine(
                    state,
                    "      pos=(" +
                    std::to_string(static_cast<int>(layer.position.x)) + "," +
                    std::to_string(static_cast<int>(layer.position.y)) + ")" +
                    " size=(" +
                    std::to_string(static_cast<int>(layer.imageSize.x)) + "," +
                    std::to_string(static_cast<int>(layer.imageSize.y)) + ")" +
                    " image=" + layer.imagePath,
                    GRAY);
        }

        return true;
    }

    if (cmd == "/copylast") {
        int lineCount = -1; // -1 = all

        if (args.size() >= 2) {
            try {
                lineCount = std::stoi(args[1]);
            } catch (...) {
                DebugConsoleAddLine(state, "usage: /copylast [numLines]", RED);
                return true;
            }

            if (lineCount < 0) {
                DebugConsoleAddLine(state, "usage: /copylast [numLines]", RED);
                return true;
            }
        }

        const auto& lines = state.debug.console.lines;
        const int total = static_cast<int>(lines.size());

        int startIndex = 0;

        if (lineCount >= 0) {
            startIndex = std::max(0, total - lineCount);
        }

        std::string buffer;
        buffer.reserve(4096);

        for (int i = startIndex; i < total; ++i) {
            buffer += lines[i].text;
            buffer += '\n';
        }

        if (buffer.empty()) {
            DebugConsoleAddLine(state, "nothing to copy", LIGHTGRAY);
            return true;
        }

        SetClipboardText(buffer.c_str());

        if (lineCount < 0) {
            DebugConsoleAddLine(state, "copied entire console history to clipboard", SKYBLUE);
        } else {
            DebugConsoleAddLine(
                    state,
                    "copied last " + std::to_string(total - startIndex) + " lines to clipboard",
                    SKYBLUE);
        }

        return true;
    }

    DebugConsoleAddLine(state, "unknown command: " + cmd, RED);
    return true;
}