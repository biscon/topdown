#include "topdown/NpcRegistry.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

#include "resources/AsepriteAsset.h"
#include "utils/json.hpp"
#include "raymath.h"
#include "TopdownRvo.h"
#include "topdown/TopdownHelpers.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static bool ParseNpcAiModeString(
        const std::string& s,
        TopdownNpcAiMode& outMode)
{
    if (s.empty() || s == "none") {
        outMode = TopdownNpcAiMode::None;
        return true;
    }

    if (s == "seek_and_destroy" || s == "SeekAndDestroy") {
        outMode = TopdownNpcAiMode::SeekAndDestroy;
        return true;
    }

    return false;
}

static void ClampNpcAttackEffectsConfig(TopdownNpcAttackEffectsConfig& cfg)
{
    if (cfg.bloodImpactParticleCount < 0) cfg.bloodImpactParticleCount = 0;

    if (cfg.bloodImpactParticleSpeedMin < 0.0f) cfg.bloodImpactParticleSpeedMin = 0.0f;
    if (cfg.bloodImpactParticleSpeedMax < cfg.bloodImpactParticleSpeedMin) {
        cfg.bloodImpactParticleSpeedMax = cfg.bloodImpactParticleSpeedMin;
    }

    if (cfg.bloodImpactParticleLifetimeMsMin < 0.0f) cfg.bloodImpactParticleLifetimeMsMin = 0.0f;
    if (cfg.bloodImpactParticleLifetimeMsMax < cfg.bloodImpactParticleLifetimeMsMin) {
        cfg.bloodImpactParticleLifetimeMsMax = cfg.bloodImpactParticleLifetimeMsMin;
    }

    if (cfg.bloodImpactParticleSizeMin < 0.0f) cfg.bloodImpactParticleSizeMin = 0.0f;
    if (cfg.bloodImpactParticleSizeMax < cfg.bloodImpactParticleSizeMin) {
        cfg.bloodImpactParticleSizeMax = cfg.bloodImpactParticleSizeMin;
    }

    if (cfg.bloodImpactSpreadDegrees < 0.0f) cfg.bloodImpactSpreadDegrees = 0.0f;

    if (cfg.bloodDecalCountMin < 0) cfg.bloodDecalCountMin = 0;
    if (cfg.bloodDecalCountMax < cfg.bloodDecalCountMin) {
        cfg.bloodDecalCountMax = cfg.bloodDecalCountMin;
    }

    if (cfg.bloodDecalDistanceMin < 0.0f) cfg.bloodDecalDistanceMin = 0.0f;
    if (cfg.bloodDecalDistanceMax < cfg.bloodDecalDistanceMin) {
        cfg.bloodDecalDistanceMax = cfg.bloodDecalDistanceMin;
    }

    if (cfg.bloodDecalRadiusMin < 0.0f) cfg.bloodDecalRadiusMin = 0.0f;
    if (cfg.bloodDecalRadiusMax < cfg.bloodDecalRadiusMin) {
        cfg.bloodDecalRadiusMax = cfg.bloodDecalRadiusMin;
    }

    if (cfg.bloodDecalSpreadDegrees < 0.0f) cfg.bloodDecalSpreadDegrees = 0.0f;
    if (cfg.bloodDecalWallPadding < 0.0f) cfg.bloodDecalWallPadding = 0.0f;

    cfg.bloodDecalOpacityMin = Clamp(cfg.bloodDecalOpacityMin, 0.0f, 1.0f);
    cfg.bloodDecalOpacityMax = Clamp(cfg.bloodDecalOpacityMax, 0.0f, 1.0f);
    if (cfg.bloodDecalOpacityMax < cfg.bloodDecalOpacityMin) {
        cfg.bloodDecalOpacityMax = cfg.bloodDecalOpacityMin;
    }
}

static void ReadNpcAttackEffectsConfig(
        const json& entry,
        TopdownNpcAttackEffectsConfig& outCfg)
{
    auto it = entry.find("attackEffects");
    if (it == entry.end() || !it->is_object()) {
        ClampNpcAttackEffectsConfig(outCfg);
        return;
    }

    const json& fx = *it;

    outCfg.bloodImpactParticleCount =
            fx.value("bloodImpactParticleCount", outCfg.bloodImpactParticleCount);
    outCfg.bloodImpactParticleSpeedMin =
            fx.value("bloodImpactParticleSpeedMin", outCfg.bloodImpactParticleSpeedMin);
    outCfg.bloodImpactParticleSpeedMax =
            fx.value("bloodImpactParticleSpeedMax", outCfg.bloodImpactParticleSpeedMax);
    outCfg.bloodImpactParticleLifetimeMsMin =
            fx.value("bloodImpactParticleLifetimeMsMin", outCfg.bloodImpactParticleLifetimeMsMin);
    outCfg.bloodImpactParticleLifetimeMsMax =
            fx.value("bloodImpactParticleLifetimeMsMax", outCfg.bloodImpactParticleLifetimeMsMax);
    outCfg.bloodImpactParticleSizeMin =
            fx.value("bloodImpactParticleSizeMin", outCfg.bloodImpactParticleSizeMin);
    outCfg.bloodImpactParticleSizeMax =
            fx.value("bloodImpactParticleSizeMax", outCfg.bloodImpactParticleSizeMax);
    outCfg.bloodImpactSpreadDegrees =
            fx.value("bloodImpactSpreadDegrees", outCfg.bloodImpactSpreadDegrees);

    outCfg.bloodDecalCountMin =
            fx.value("bloodDecalCountMin", outCfg.bloodDecalCountMin);
    outCfg.bloodDecalCountMax =
            fx.value("bloodDecalCountMax", outCfg.bloodDecalCountMax);
    outCfg.bloodDecalDistanceMin =
            fx.value("bloodDecalDistanceMin", outCfg.bloodDecalDistanceMin);
    outCfg.bloodDecalDistanceMax =
            fx.value("bloodDecalDistanceMax", outCfg.bloodDecalDistanceMax);
    outCfg.bloodDecalRadiusMin =
            fx.value("bloodDecalRadiusMin", outCfg.bloodDecalRadiusMin);
    outCfg.bloodDecalRadiusMax =
            fx.value("bloodDecalRadiusMax", outCfg.bloodDecalRadiusMax);
    outCfg.bloodDecalSpreadDegrees =
            fx.value("bloodDecalSpreadDegrees", outCfg.bloodDecalSpreadDegrees);
    outCfg.bloodDecalWallPadding =
            fx.value("bloodDecalWallPadding", outCfg.bloodDecalWallPadding);
    outCfg.bloodDecalOpacityMin =
            fx.value("bloodDecalOpacityMin", outCfg.bloodDecalOpacityMin);
    outCfg.bloodDecalOpacityMax =
            fx.value("bloodDecalOpacityMax", outCfg.bloodDecalOpacityMax);

    ClampNpcAttackEffectsConfig(outCfg);
}

static void ReadNpcSounds(
        const json& entry,
        TopdownNpcAssetDefinition& def)
{
    auto it = entry.find("sounds");
    if (it == entry.end() || !it->is_object()) {
        return;
    }

    const json& sounds = *it;

    def.attackStartSoundId = sounds.value("attackStart", std::string());
    def.attackConnectSoundId = sounds.value("attackConnect", std::string());

    auto hitReactionIt = sounds.find("hitReaction");
    if (hitReactionIt != sounds.end() && hitReactionIt->is_array()) {
        for (const auto& soundEntry : *hitReactionIt) {
            if (soundEntry.is_string()) {
                def.hitReactionSoundIds.push_back(soundEntry.get<std::string>());
            }
        }
    }
}

bool TopdownNpcClipRefIsValid(const TopdownNpcClipRef& clipRef)
{
    return clipRef.spriteHandle >= 0 && clipRef.clipIndex >= 0;
}

void TopdownSetNpcAutomaticLoopAnimation(
        TopdownNpcRuntime& npc,
        const TopdownNpcClipRef& clipRef)
{
    const bool changed =
            npc.automaticLoopClip.spriteHandle != clipRef.spriteHandle ||
            npc.automaticLoopClip.clipIndex != clipRef.clipIndex ||
            npc.automaticLoopClip.clipName != clipRef.clipName;

    npc.automaticLoopClip = clipRef;

    if (changed) {
        npc.automaticLoopTimeMs = 0.0f;
    }
}

void TopdownSetNpcScriptLoopAnimation(
        TopdownNpcRuntime& npc,
        const TopdownNpcClipRef& clipRef)
{
    const bool changed =
            npc.scriptLoopClip.spriteHandle != clipRef.spriteHandle ||
            npc.scriptLoopClip.clipIndex != clipRef.clipIndex ||
            npc.scriptLoopClip.clipName != clipRef.clipName ||
            npc.animationMode != TopdownNpcAnimationMode::ScriptLoop;

    npc.animationMode = TopdownNpcAnimationMode::ScriptLoop;
    npc.scriptLoopClip = clipRef;

    if (changed) {
        npc.scriptLoopTimeMs = 0.0f;
    }
}

void TopdownClearNpcScriptLoopAnimation(TopdownNpcRuntime& npc)
{
    npc.animationMode = TopdownNpcAnimationMode::AutomaticLocomotion;
    npc.scriptLoopClip = {};
    npc.scriptLoopTimeMs = 0.0f;
}

void TopdownPlayNpcOneShotAnimation(
        TopdownNpcRuntime& npc,
        const TopdownNpcClipRef& clipRef)
{
    npc.oneShotActive = true;
    npc.oneShotClip = clipRef;
    npc.oneShotTimeMs = 0.0f;
}

void TopdownClearNpcOneShotAnimation(TopdownNpcRuntime& npc)
{
    npc.oneShotActive = false;
    npc.oneShotClip = {};
    npc.oneShotTimeMs = 0.0f;
}

const TopdownNpcClipRef* TopdownGetResolvedNpcAnimationClip(const TopdownNpcRuntime& npc)
{
    if (npc.oneShotActive && TopdownNpcClipRefIsValid(npc.oneShotClip)) {
        return &npc.oneShotClip;
    }

    if (npc.animationMode == TopdownNpcAnimationMode::ScriptLoop &&
        TopdownNpcClipRefIsValid(npc.scriptLoopClip)) {
        return &npc.scriptLoopClip;
    }

    if (TopdownNpcClipRefIsValid(npc.automaticLoopClip)) {
        return &npc.automaticLoopClip;
    }

    return nullptr;
}

std::string TopdownGetResolvedNpcAnimationName(const TopdownNpcRuntime& npc)
{
    const TopdownNpcClipRef* clip = TopdownGetResolvedNpcAnimationClip(npc);
    if (clip == nullptr) {
        return {};
    }

    return clip->clipName;
}

TopdownNpcClipRef TopdownMakeNpcClipRef(
        SpriteAssetHandle spriteHandle,
        int clipIndex,
        const char* clipName)
{
    TopdownNpcClipRef out;
    out.spriteHandle = spriteHandle;
    out.clipIndex = clipIndex;
    out.clipName = (clipName != nullptr) ? clipName : "";
    return out;
}

static int FindFirstClipIndexByTagName(
        const SpriteAssetResource& asset,
        const std::string& tagName)
{
    for (int i = 0; i < static_cast<int>(asset.clips.size()); ++i) {
        if (asset.clips[i].name == tagName) {
            return i;
        }
    }
    return -1;
}

static void AssignNpcClipIfPresent(
        TopdownNpcClipRef& dst,
        SpriteAssetHandle spriteHandle,
        const SpriteAssetResource& sprite,
        const char* tagName)
{
    if (TopdownNpcClipRefIsValid(dst)) {
        return;
    }

    const int clipIndex = FindFirstClipIndexByTagName(sprite, tagName);
    if (clipIndex < 0) {
        return;
    }

    dst = TopdownMakeNpcClipRef(spriteHandle, clipIndex, tagName);
}

static void MergeNpcRegistryFile(
        GameState& state,
        const fs::path& jsonPath)
{
    json root;
    {
        std::ifstream in(jsonPath);
        if (!in.is_open()) {
            TraceLog(LOG_WARNING,
                     "Failed opening NPC registry file: %s",
                     jsonPath.string().c_str());
            return;
        }
        in >> root;
    }

    if (!root.is_object() || !root.contains("npcs") || !root["npcs"].is_array()) {
        TraceLog(LOG_WARNING,
                 "NPC registry file missing 'npcs' array: %s",
                 jsonPath.string().c_str());
        return;
    }

    const fs::path dir = jsonPath.parent_path();

    for (const auto& entry : root["npcs"]) {
        if (!entry.is_object()) {
            continue;
        }

        TopdownNpcAssetDefinition def;
        def.assetId = entry.value("assetId", std::string());
        def.baseDrawScale = entry.value("baseDrawScale", 1.0f);
        def.collisionRadius = entry.value("collisionRadius", 32.0f);
        def.walkSpeed = entry.value("walkSpeed", 450.0f);
        def.runSpeed = entry.value("runSpeed", 700.0f);
        def.hurtStunMs = entry.value("hurtStunMs", 0.0f);
        def.maxHealth = entry.value("maxHealth", 100.0f);
        def.corpseExpirationMs = entry.value("corpseExpirationMs", -1.0f);

        def.hostile = entry.value("hostile", true);

        {
            const std::string aiModeStr =
                    entry.value("aiMode", std::string("none"));

            if (!ParseNpcAiModeString(aiModeStr, def.aiMode)) {
                TraceLog(LOG_WARNING,
                         "NPC definition '%s' has invalid aiMode '%s', defaulting to none",
                         def.assetId.c_str(),
                         aiModeStr.c_str());
                def.aiMode = TopdownNpcAiMode::None;
            }
        }

        def.visionRange = entry.value("visionRange", 700.0f);
        def.hearingRange = entry.value("hearingRange", 220.0f);
        def.gunshotHearingRange = entry.value("gunshotHearingRange", 1000.0f);
        def.visionHalfAngleDegrees = entry.value("visionHalfAngleDegrees", 65.0f);

        def.attackRange = entry.value("attackRange", 95.0f);
        def.attackCooldownMs = entry.value("attackCooldownMs", 900.0f);
        def.attackDamage = entry.value("attackDamage", 25.0f);
        def.attackHitNormalizedTime = entry.value("attackHitNormalizedTime", 0.7f);
        def.attackRecoverMs = entry.value("attackRecoverMs", 250.0f);
        def.meleeHitPosX = entry.value("meleeHitPosX", 0.0f);
        def.meleeHitPosY = entry.value("meleeHitPosY", 0.0f);

        ReadNpcSounds(entry, def);
        ReadNpcAttackEffectsConfig(entry, def.attackEffects);

        def.chaseRepathIntervalMs = entry.value("chaseRepathIntervalMs", 250.0f);
        def.loseTargetTimeoutMs = entry.value("loseTargetTimeoutMs", 1200.0f);

        if (def.hurtStunMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has hurtStunMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.hurtStunMs = 0.0f;
        }

        if (def.maxHealth <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has maxHealth <= 0, clamping to 1",
                     def.assetId.c_str());
            def.maxHealth = 1.0f;
        }

        if (def.corpseExpirationMs < 0.0f && def.corpseExpirationMs != -1.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has invalid corpseExpirationMs %.2f, using -1",
                     def.assetId.c_str(),
                     def.corpseExpirationMs);
            def.corpseExpirationMs = -1.0f;
        }

        if (def.visionRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has visionRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.visionRange = 0.0f;
        }

        if (def.hearingRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has hearingRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.hearingRange = 0.0f;
        }

        if (def.gunshotHearingRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has gunshotHearingRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.gunshotHearingRange = 0.0f;
        }

        if (def.visionHalfAngleDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has visionHalfAngleDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.visionHalfAngleDegrees = 0.0f;
        } else if (def.visionHalfAngleDegrees > 180.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has visionHalfAngleDegrees > 180, clamping to 180",
                     def.assetId.c_str());
            def.visionHalfAngleDegrees = 180.0f;
        }

        if (def.attackRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.attackRange = 0.0f;
        }

        if (def.attackCooldownMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackCooldownMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.attackCooldownMs = 0.0f;
        }

        if (def.attackDamage < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackDamage < 0, clamping to 0",
                     def.assetId.c_str());
            def.attackDamage = 0.0f;
        }

        if (def.attackHitNormalizedTime < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackHitNormalizedTime < 0, clamping to 0",
                     def.assetId.c_str());
            def.attackHitNormalizedTime = 0.0f;
        } else if (def.attackHitNormalizedTime > 1.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackHitNormalizedTime > 1, clamping to 1",
                     def.assetId.c_str());
            def.attackHitNormalizedTime = 1.0f;
        }

        if (def.attackRecoverMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackRecoverMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.attackRecoverMs = 0.0f;
        }

        if (def.chaseRepathIntervalMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has chaseRepathIntervalMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.chaseRepathIntervalMs = 0.0f;
        }

        if (def.loseTargetTimeoutMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has loseTargetTimeoutMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.loseTargetTimeoutMs = 0.0f;
        }

        if (def.assetId.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping NPC definition in %s: missing assetId",
                     jsonPath.string().c_str());
            continue;
        }

        const json* animationsJson = nullptr;
        auto animIt = entry.find("animations");
        if (animIt != entry.end() && animIt->is_array()) {
            animationsJson = &(*animIt);
        }

        if (animationsJson == nullptr || animationsJson->empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping NPC definition '%s' in %s: missing animations array",
                     def.assetId.c_str(),
                     jsonPath.string().c_str());
            continue;
        }

        for (const auto& animEntry : *animationsJson) {
            if (!animEntry.is_object()) {
                continue;
            }

            const std::string asepriteRel =
                    animEntry.value("asepriteJson", std::string());

            if (asepriteRel.empty()) {
                TraceLog(LOG_WARNING,
                         "Skipping animation source for NPC '%s' in %s: missing asepriteJson",
                         def.assetId.c_str(),
                         jsonPath.string().c_str());
                continue;
            }

            TopdownNpcAnimationSourceDefinition animDef;
            animDef.asepriteJsonPath =
                    NormalizePath((dir / asepriteRel).lexically_normal());

            const bool hasOriginX =
                    animEntry.contains("originX") && animEntry["originX"].is_number();
            const bool hasOriginY =
                    animEntry.contains("originY") && animEntry["originY"].is_number();

            if (hasOriginX != hasOriginY) {
                TraceLog(LOG_WARNING,
                         "NPC '%s' animation source '%s' has only one of originX/originY; ignoring origin override",
                         def.assetId.c_str(),
                         animDef.asepriteJsonPath.c_str());
            } else if (hasOriginX && hasOriginY) {
                animDef.hasOrigin = true;
                animDef.origin.x = animEntry["originX"].get<float>();
                animDef.origin.y = animEntry["originY"].get<float>();
            }

            def.animations.push_back(animDef);
        }

        if (def.animations.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping NPC definition '%s' in %s: no valid animation sources",
                     def.assetId.c_str(),
                     jsonPath.string().c_str());
            continue;
        }

        auto existing = std::find_if(
                state.topdown.npcAssetRegistry.begin(),
                state.topdown.npcAssetRegistry.end(),
                [&](const TopdownNpcAssetDefinition& other) {
                    return other.assetId == def.assetId;
                });

        if (existing != state.topdown.npcAssetRegistry.end()) {
            TraceLog(LOG_WARNING,
                     "Duplicate NPC assetId '%s' in registry, overriding with latest definition",
                     def.assetId.c_str());
            *existing = def;
        } else {
            state.topdown.npcAssetRegistry.push_back(def);
        }
    }
}

bool TopdownScanNpcRegistry(GameState& state)
{
    state.topdown.npcAssetRegistry.clear();
    state.topdown.npcAssets.clear();

    const fs::path npcDir = fs::path(ASSETS_PATH "characters/npcs");
    if (!fs::exists(npcDir) || !fs::is_directory(npcDir)) {
        TraceLog(LOG_WARNING,
                 "Topdown NPC registry directory missing: %s",
                 npcDir.string().c_str());
        return false;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(npcDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path path = entry.path();
        if (path.extension() == ".json") {
            files.push_back(path);
        }
    }

    std::sort(files.begin(), files.end());

    for (const fs::path& path : files) {
        MergeNpcRegistryFile(state, path);
    }

    TraceLog(LOG_INFO,
             "Scanned topdown NPC registry: %d asset definitions",
             static_cast<int>(state.topdown.npcAssetRegistry.size()));

    for (const TopdownNpcAssetDefinition& def : state.topdown.npcAssetRegistry) {
        TraceLog(LOG_INFO,
                 "  npc assetId=%s scale=%.3f radius=%.2f animSources=%d",
                 def.assetId.c_str(),
                 def.baseDrawScale,
                 def.collisionRadius,
                 static_cast<int>(def.animations.size()));
    }

    return !state.topdown.npcAssetRegistry.empty();
}

const TopdownNpcAssetDefinition* FindTopdownNpcAssetDefinition(
        const GameState& state,
        const std::string& assetId)
{
    for (const TopdownNpcAssetDefinition& def : state.topdown.npcAssetRegistry) {
        if (def.assetId == assetId) {
            return &def;
        }
    }
    return nullptr;
}

TopdownNpcAssetRuntime* FindTopdownNpcAssetRuntime(
        GameState& state,
        const std::string& assetId)
{
    for (TopdownNpcAssetRuntime& asset : state.topdown.npcAssets) {
        if (asset.assetId == assetId) {
            return &asset;
        }
    }
    return nullptr;
}

const TopdownNpcAssetRuntime* FindTopdownNpcAssetRuntime(
        const GameState& state,
        const std::string& assetId)
{
    for (const TopdownNpcAssetRuntime& asset : state.topdown.npcAssets) {
        if (asset.assetId == assetId) {
            return &asset;
        }
    }
    return nullptr;
}

bool EnsureTopdownNpcAssetLoaded(GameState& state, const std::string& assetId)
{
    if (FindTopdownNpcAssetRuntime(state, assetId) != nullptr) {
        return true;
    }

    const TopdownNpcAssetDefinition* def = FindTopdownNpcAssetDefinition(state, assetId);
    if (def == nullptr) {
        TraceLog(LOG_ERROR, "Unknown topdown NPC assetId: %s", assetId.c_str());
        return false;
    }

    TopdownNpcAssetRuntime runtime;
    runtime.assetId = def->assetId;
    runtime.baseDrawScale = def->baseDrawScale;
    runtime.collisionRadius = def->collisionRadius;
    runtime.walkSpeed = def->walkSpeed;
    runtime.runSpeed = def->runSpeed;
    runtime.hurtStunMs = def->hurtStunMs;
    runtime.maxHealth = def->maxHealth;
    runtime.corpseExpirationMs = def->corpseExpirationMs;
    runtime.hostile = def->hostile;
    runtime.aiMode = def->aiMode;

    runtime.visionRange = def->visionRange;
    runtime.hearingRange = def->hearingRange;
    runtime.gunshotHearingRange = def->gunshotHearingRange;
    runtime.visionHalfAngleDegrees = def->visionHalfAngleDegrees;

    runtime.attackRange = def->attackRange;
    runtime.attackCooldownMs = def->attackCooldownMs;
    runtime.attackDamage = def->attackDamage;
    runtime.attackHitNormalizedTime = def->attackHitNormalizedTime;
    runtime.attackRecoverMs = def->attackRecoverMs;
    runtime.meleeHitPosX = def->meleeHitPosX;
    runtime.meleeHitPosY = def->meleeHitPosY;

    runtime.attackStartSoundId = def->attackStartSoundId;
    runtime.attackConnectSoundId = def->attackConnectSoundId;
    runtime.hitReactionSoundIds = def->hitReactionSoundIds;
    runtime.attackEffects = def->attackEffects;

    runtime.chaseRepathIntervalMs = def->chaseRepathIntervalMs;
    runtime.loseTargetTimeoutMs = def->loseTargetTimeoutMs;

    for (const TopdownNpcAnimationSourceDefinition& animSource : def->animations) {
        SpriteAssetHandle spriteHandle = -1;

        if (animSource.hasOrigin) {
            spriteHandle = LoadSpriteAssetFromAsepriteJsonWithOrigin(
                    state.resources,
                    animSource.asepriteJsonPath.c_str(),
                    def->baseDrawScale,
                    animSource.origin,
                    ResourceScope::Scene);
        } else {
            spriteHandle = LoadSpriteAssetFromAsepriteJson(
                    state.resources,
                    animSource.asepriteJsonPath.c_str(),
                    def->baseDrawScale,
                    ResourceScope::Scene);
        }

        if (spriteHandle < 0) {
            TraceLog(LOG_ERROR,
                     "Failed loading topdown NPC sprite asset '%s' source %s",
                     def->assetId.c_str(),
                     animSource.asepriteJsonPath.c_str());
            return false;
        }

        const SpriteAssetResource* sprite =
                FindSpriteAssetResource(state.resources, spriteHandle);

        if (sprite == nullptr || !sprite->loaded) {
            TraceLog(LOG_ERROR,
                     "Loaded NPC sprite asset but failed resolving sprite resource: %s",
                     animSource.asepriteJsonPath.c_str());
            return false;
        }

        runtime.spriteHandles.push_back(spriteHandle);

        AssignNpcClipIfPresent(runtime.idleClip, spriteHandle, *sprite, "Idle");
        AssignNpcClipIfPresent(runtime.walkClip, spriteHandle, *sprite, "Walk");
        AssignNpcClipIfPresent(runtime.runClip, spriteHandle, *sprite, "Run");
        AssignNpcClipIfPresent(runtime.hurtClip, spriteHandle, *sprite, "Hurt");
        AssignNpcClipIfPresent(runtime.deathClip, spriteHandle, *sprite, "Death");
        AssignNpcClipIfPresent(runtime.rangedAttackClip, spriteHandle, *sprite, "RangedAttack");
        AssignNpcClipIfPresent(runtime.meleeAttackClip, spriteHandle, *sprite, "MeleeAttack");
    }

    runtime.loaded = true;
    state.topdown.npcAssets.push_back(runtime);

    TraceLog(LOG_INFO,
             "Loaded topdown NPC asset '%s' idle=%s walk=%s run=%s hurt=%s death=%s ranged=%s melee=%s",
             runtime.assetId.c_str(),
             TopdownNpcClipRefIsValid(runtime.idleClip) ? "yes" : "no",
             TopdownNpcClipRefIsValid(runtime.walkClip) ? "yes" : "no",
             TopdownNpcClipRefIsValid(runtime.runClip) ? "yes" : "no",
             TopdownNpcClipRefIsValid(runtime.hurtClip) ? "yes" : "no",
             TopdownNpcClipRefIsValid(runtime.deathClip) ? "yes" : "no",
             TopdownNpcClipRefIsValid(runtime.rangedAttackClip) ? "yes" : "no",
             TopdownNpcClipRefIsValid(runtime.meleeAttackClip) ? "yes" : "no");

    return true;
}

TopdownNpcClipRef FindTopdownNpcClipByName(
        const GameState& state,
        const TopdownNpcAssetRuntime& asset,
        const std::string& clipName)
{
    if (clipName.empty()) {
        return {};
    }

    for (SpriteAssetHandle spriteHandle : asset.spriteHandles) {
        const SpriteAssetResource* sprite =
                FindSpriteAssetResource(state.resources, spriteHandle);

        if (sprite == nullptr || !sprite->loaded) {
            continue;
        }

        const int clipIndex = FindFirstClipIndexByTagName(*sprite, clipName);
        if (clipIndex >= 0) {
            return TopdownMakeNpcClipRef(spriteHandle, clipIndex, clipName.c_str());
        }
    }

    return {};
}

static void AppendDoorCollisionSegments(
        const TopdownRuntimeData& runtime,
        std::vector<TopdownSegment>& outSegments)
{
    for (const TopdownRuntimeDoor& door : runtime.doors) {
        if (!door.visible) {
            continue;
        }

        Vector2 a{};
        Vector2 b{};
        Vector2 c{};
        Vector2 d{};
        TopdownBuildDoorCorners(door, a, b, c, d);

        outSegments.push_back(TopdownSegment{a, b});
        outSegments.push_back(TopdownSegment{b, c});
        outSegments.push_back(TopdownSegment{c, d});
        outSegments.push_back(TopdownSegment{d, a});
    }
}

static void AppendWindowCollisionSegments(
        const TopdownRuntimeData& runtime,
        std::vector<TopdownSegment>& outSegments)
{
    for (const TopdownRuntimeWindow& window : runtime.windows) {
        if (!window.visible || window.broken) {
            continue;
        }

        for (const TopdownSegment& edge : window.edges) {
            outSegments.push_back(edge);
        }
    }
}

static bool CandidateOverlapsBlockingGeometry(
        const TopdownRuntimeData& runtime,
        Vector2 candidate,
        float npcRadius,
        const std::vector<TopdownSegment>& blockingSegments,
        float spawnPadding)
{
    const float clearance = npcRadius + spawnPadding;
    const float clearanceSqr = clearance * clearance;

    for (const TopdownNpcRuntime& npc : runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        const float minDist = npc.collisionRadius + clearance;
        const float minDistSqr = minDist * minDist;
        const Vector2 delta = TopdownSub(candidate, npc.position);
        if (TopdownLengthSqr(delta) < minDistSqr) {
            return true;
        }
    }

    for (const TopdownSegment& segment : blockingSegments) {
        const Vector2 closest = TopdownClosestPointOnSegment(candidate, segment);
        const Vector2 delta = TopdownSub(candidate, closest);
        if (TopdownLengthSqr(delta) < clearanceSqr) {
            return true;
        }
    }

    return false;
}

static bool TryResolveSmartSpawnPosition(
        const TopdownRuntimeData& runtime,
        Vector2 preferredPosition,
        float npcRadius,
        Vector2& outPosition)
{
    static constexpr float kSpawnPadding = 4.0f;
    static constexpr int kMaxRings = 3;
    static constexpr float kMinRadiusStep = 12.0f;
    static constexpr float kRaycastEpsilon = 0.001f;

    std::vector<TopdownSegment> blockingSegments = runtime.collision.movementSegments;
    blockingSegments.reserve(
            blockingSegments.size() +
            runtime.doors.size() * 4 +
            runtime.windows.size() * 4);

    AppendDoorCollisionSegments(runtime, blockingSegments);
    AppendWindowCollisionSegments(runtime, blockingSegments);

    if (!CandidateOverlapsBlockingGeometry(
                runtime,
                preferredPosition,
                npcRadius,
                blockingSegments,
                kSpawnPadding)) {
        outPosition = preferredPosition;
        return true;
    }

    const float radiusStep = std::max(kMinRadiusStep, npcRadius * 2.0f + kSpawnPadding);
    const float slotArcLength = std::max(kMinRadiusStep, npcRadius * 2.0f + kSpawnPadding);

    for (int ringIndex = 1; ringIndex <= kMaxRings; ++ringIndex) {
        const float ringRadius = radiusStep * static_cast<float>(ringIndex);
        const float circumference = 2.0f * PI * ringRadius;
        const int slotCount = std::max(6, static_cast<int>(std::ceil(circumference / slotArcLength)));

        for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex) {
            const float t = static_cast<float>(slotIndex) / static_cast<float>(slotCount);
            const float radians = t * 2.0f * PI;
            const Vector2 offset{
                    std::cos(radians) * ringRadius,
                    std::sin(radians) * ringRadius
            };
            const Vector2 candidate = TopdownAdd(preferredPosition, offset);

            if (CandidateOverlapsBlockingGeometry(
                        runtime,
                        candidate,
                        npcRadius,
                        blockingSegments,
                        kSpawnPadding)) {
                continue;
            }

            const Vector2 toCandidate = TopdownSub(candidate, preferredPosition);
            const float rayDistance = TopdownLength(toCandidate);
            if (rayDistance <= 0.000001f) {
                outPosition = candidate;
                return true;
            }

            const Vector2 rayDir = TopdownMul(toCandidate, 1.0f / rayDistance);
            Vector2 hitPoint{};
            const bool blocked = TopdownRaycastSegments(
                    preferredPosition,
                    rayDir,
                    blockingSegments,
                    rayDistance - kRaycastEpsilon,
                    hitPoint,
                    nullptr);
            if (blocked) {
                continue;
            }

            outPosition = candidate;
            return true;
        }
    }

    return false;
}

bool TopdownSpawnNpcRuntime(
        GameState& state,
        const std::string& npcId,
        const std::string& assetId,
        Vector2 position,
        float orientationDegrees,
        bool visible,
        bool persistentChase,
        bool smartPlacement)
{
    if (npcId.empty() || assetId.empty()) {
        return false;
    }

    if (!EnsureTopdownNpcAssetLoaded(state, assetId)) {
        return false;
    }

    const TopdownNpcAssetRuntime* asset =
            FindTopdownNpcAssetRuntime(state, assetId);

    if (asset == nullptr || !asset->loaded) {
        return false;
    }

    if (smartPlacement) {
        Vector2 resolvedPosition{};
        if (!TryResolveSmartSpawnPosition(
                    state.topdown.runtime,
                    position,
                    asset->collisionRadius,
                    resolvedPosition)) {
            TraceLog(LOG_WARNING,
                     "Unable to find smart spawn position for NPC '%s' near %.1f, %.1f",
                     npcId.c_str(),
                     position.x,
                     position.y);
            return false;
        }

        position = resolvedPosition;
    }

    TopdownNpcRuntime npc;
    npc.handle = state.topdown.runtime.nextNpcHandle++;
    npc.id = npcId;
    npc.assetId = assetId;
    npc.active = true;
    npc.visible = visible;
    npc.dead = false;
    npc.corpse = false;

    npc.health = asset->maxHealth;
    npc.corpseExpirationMs = asset->corpseExpirationMs;
    npc.corpseElapsedMs = 0.0f;

    npc.hostile = asset->hostile;
    npc.persistentChase = persistentChase;
    npc.aiMode = asset->aiMode;
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
    npc.combatState = TopdownNpcCombatState::None;

    npc.visionRange = asset->visionRange;
    npc.hearingRange = asset->hearingRange;
    npc.gunshotHearingRange = asset->gunshotHearingRange;
    npc.visionHalfAngleDegrees = asset->visionHalfAngleDegrees;

    npc.attackRange = asset->attackRange;
    npc.attackCooldownMs = asset->attackCooldownMs;
    npc.attackCooldownRemainingMs = 0.0f;
    npc.attackDamage = asset->attackDamage;
    npc.attackHitNormalizedTime = asset->attackHitNormalizedTime;
    npc.attackRecoverMs = asset->attackRecoverMs;
    npc.meleeHitPosX = asset->meleeHitPosX;
    npc.meleeHitPosY = asset->meleeHitPosY;

    npc.attackStartSoundId = asset->attackStartSoundId;
    npc.attackConnectSoundId = asset->attackConnectSoundId;
    npc.hitReactionSoundIds = asset->hitReactionSoundIds;

    npc.attackEffects = asset->attackEffects;

    npc.chaseRepathIntervalMs = asset->chaseRepathIntervalMs;
    npc.loseTargetTimeoutMs = asset->loseTargetTimeoutMs;

    npc.hasPlayerTarget = false;
    npc.lastKnownPlayerPosition = {};
    npc.loseTargetTimerMs = 0.0f;
    npc.repathTimerMs = 0.0f;

    npc.attackHitPending = false;
    npc.attackHitApplied = false;
    npc.attackStateTimeMs = 0.0f;
    npc.attackAnimationDurationMs = 0.0f;

    npc.renderOpacity = 1.0f;

    npc.position = position;
    npc.collisionRadius = asset->collisionRadius;

    const float radians = orientationDegrees * DEG2RAD;
    npc.facing.x = std::cos(radians);
    npc.facing.y = std::sin(radians);
    npc.rotationRadians = radians;

    npc.oneShotActive = false;
    npc.oneShotClip = {};
    npc.oneShotTimeMs = 0.0f;

    if (TopdownNpcClipRefIsValid(asset->idleClip)) {
        TopdownSetNpcAutomaticLoopAnimation(npc, asset->idleClip);
    }

    npc.animationMode = TopdownNpcAnimationMode::AutomaticLocomotion;

    npc.move = {};
    npc.move.owner = TopdownNpcMoveOwner::None;

    state.topdown.runtime.npcs.push_back(npc);
    TopdownRvoRequestRebuild(state);
    return true;
}
