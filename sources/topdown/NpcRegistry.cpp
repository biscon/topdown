#include "topdown/NpcRegistry.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

#include "resources/AsepriteAsset.h"
#include "utils/json.hpp"
#include "raymath.h"
#include "TopdownRvo.h"
#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcRingSlots.h"

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

    if (s == "hold_and_fire" || s == "HoldAndFire") {
        outMode = TopdownNpcAiMode::HoldAndFire;
        return true;
    }

    return false;
}

static bool ParseTopdownAttackTypeString(
        const std::string& s,
        TopdownAttackType& outType)
{
    if (s.empty() || s == "none") {
        outType = TopdownAttackType::None;
        return true;
    }

    if (s == "melee" || s == "Melee") {
        outType = TopdownAttackType::Melee;
        return true;
    }

    if (s == "ranged" || s == "Ranged") {
        outType = TopdownAttackType::Ranged;
        return true;
    }

    return false;
}

static bool ParseTopdownTracerStyleString(
        const std::string& s,
        TopdownTracerStyle& outStyle)
{
    if (s.empty() || s == "none") {
        outStyle = TopdownTracerStyle::None;
        return true;
    }

    if (s == "handgun" || s == "Handgun") {
        outStyle = TopdownTracerStyle::Handgun;
        return true;
    }

    if (s == "shotgun" || s == "Shotgun") {
        outStyle = TopdownTracerStyle::Shotgun;
        return true;
    }

    if (s == "rifle" || s == "Rifle") {
        outStyle = TopdownTracerStyle::Rifle;
        return true;
    }

    return false;
}

static void ClampNpcAttackEffectsConfig(TopdownNpcAttackEffectsConfig& cfg)
{
    TopdownBloodEffectConfig& bloodCfg = cfg.bloodEffects;

    if (bloodCfg.bloodImpactParticleCount < 0) bloodCfg.bloodImpactParticleCount = 0;

    if (bloodCfg.bloodImpactParticleSpeedMin < 0.0f) bloodCfg.bloodImpactParticleSpeedMin = 0.0f;
    if (bloodCfg.bloodImpactParticleSpeedMax < bloodCfg.bloodImpactParticleSpeedMin) {
        bloodCfg.bloodImpactParticleSpeedMax = bloodCfg.bloodImpactParticleSpeedMin;
    }

    if (bloodCfg.bloodImpactParticleLifetimeMsMin < 0.0f) bloodCfg.bloodImpactParticleLifetimeMsMin = 0.0f;
    if (bloodCfg.bloodImpactParticleLifetimeMsMax < bloodCfg.bloodImpactParticleLifetimeMsMin) {
        bloodCfg.bloodImpactParticleLifetimeMsMax = bloodCfg.bloodImpactParticleLifetimeMsMin;
    }

    if (bloodCfg.bloodImpactParticleSizeMin < 0.0f) bloodCfg.bloodImpactParticleSizeMin = 0.0f;
    if (bloodCfg.bloodImpactParticleSizeMax < bloodCfg.bloodImpactParticleSizeMin) {
        bloodCfg.bloodImpactParticleSizeMax = bloodCfg.bloodImpactParticleSizeMin;
    }

    if (bloodCfg.bloodImpactSpreadDegrees < 0.0f) bloodCfg.bloodImpactSpreadDegrees = 0.0f;

    if (bloodCfg.bloodDecalCountMin < 0) bloodCfg.bloodDecalCountMin = 0;
    if (bloodCfg.bloodDecalCountMax < bloodCfg.bloodDecalCountMin) {
        bloodCfg.bloodDecalCountMax = bloodCfg.bloodDecalCountMin;
    }

    if (bloodCfg.bloodDecalDistanceMin < 0.0f) bloodCfg.bloodDecalDistanceMin = 0.0f;
    if (bloodCfg.bloodDecalDistanceMax < bloodCfg.bloodDecalDistanceMin) {
        bloodCfg.bloodDecalDistanceMax = bloodCfg.bloodDecalDistanceMin;
    }

    if (bloodCfg.bloodDecalRadiusMin < 0.0f) bloodCfg.bloodDecalRadiusMin = 0.0f;
    if (bloodCfg.bloodDecalRadiusMax < bloodCfg.bloodDecalRadiusMin) {
        bloodCfg.bloodDecalRadiusMax = bloodCfg.bloodDecalRadiusMin;
    }

    if (bloodCfg.bloodDecalSpreadDegrees < 0.0f) bloodCfg.bloodDecalSpreadDegrees = 0.0f;
    if (bloodCfg.bloodDecalWallPadding < 0.0f) bloodCfg.bloodDecalWallPadding = 0.0f;

    bloodCfg.bloodDecalOpacityMin = Clamp(bloodCfg.bloodDecalOpacityMin, 0.0f, 1.0f);
    bloodCfg.bloodDecalOpacityMax = Clamp(bloodCfg.bloodDecalOpacityMax, 0.0f, 1.0f);
    if (bloodCfg.bloodDecalOpacityMax < bloodCfg.bloodDecalOpacityMin) {
        bloodCfg.bloodDecalOpacityMax = bloodCfg.bloodDecalOpacityMin;
    }
}

static void ClampBallisticImpactEffectsConfig(
        TopdownBallisticImpactEffectConfig& cfg)
{
    if (cfg.wallImpactParticleCount < 0) cfg.wallImpactParticleCount = 0;
    if (cfg.wallImpactParticleSpeedMin < 0.0f) cfg.wallImpactParticleSpeedMin = 0.0f;
    if (cfg.wallImpactParticleSpeedMax < cfg.wallImpactParticleSpeedMin) {
        cfg.wallImpactParticleSpeedMax = cfg.wallImpactParticleSpeedMin;
    }
    if (cfg.wallImpactParticleLifetimeMsMin < 0.0f) cfg.wallImpactParticleLifetimeMsMin = 0.0f;
    if (cfg.wallImpactParticleLifetimeMsMax < cfg.wallImpactParticleLifetimeMsMin) {
        cfg.wallImpactParticleLifetimeMsMax = cfg.wallImpactParticleLifetimeMsMin;
    }
    if (cfg.wallImpactParticleSizeMin < 0.0f) cfg.wallImpactParticleSizeMin = 0.0f;
    if (cfg.wallImpactParticleSizeMax < cfg.wallImpactParticleSizeMin) {
        cfg.wallImpactParticleSizeMax = cfg.wallImpactParticleSizeMin;
    }
    if (cfg.wallImpactSpreadDegrees < 0.0f) cfg.wallImpactSpreadDegrees = 0.0f;
}

static void ClampMuzzleEffectsConfig(
        TopdownMuzzleEffectConfig& cfg)
{
    if (cfg.muzzleFlashLifetimeMs < 0.0f) cfg.muzzleFlashLifetimeMs = 0.0f;
    if (cfg.muzzleFlashForwardLength < 0.0f) cfg.muzzleFlashForwardLength = 0.0f;
    if (cfg.muzzleFlashSideWidth < 0.0f) cfg.muzzleFlashSideWidth = 0.0f;
    if (cfg.muzzleSmokeParticleCount < 0) cfg.muzzleSmokeParticleCount = 0;
    if (cfg.muzzleSmokeSpeedMin < 0.0f) cfg.muzzleSmokeSpeedMin = 0.0f;
    if (cfg.muzzleSmokeSpeedMax < cfg.muzzleSmokeSpeedMin) {
        cfg.muzzleSmokeSpeedMax = cfg.muzzleSmokeSpeedMin;
    }
    if (cfg.muzzleSmokeLifetimeMsMin < 0.0f) cfg.muzzleSmokeLifetimeMsMin = 0.0f;
    if (cfg.muzzleSmokeLifetimeMsMax < cfg.muzzleSmokeLifetimeMsMin) {
        cfg.muzzleSmokeLifetimeMsMax = cfg.muzzleSmokeLifetimeMsMin;
    }
    if (cfg.muzzleSmokeSizeMin < 0.0f) cfg.muzzleSmokeSizeMin = 0.0f;
    if (cfg.muzzleSmokeSizeMax < cfg.muzzleSmokeSizeMin) {
        cfg.muzzleSmokeSizeMax = cfg.muzzleSmokeSizeMin;
    }
    if (cfg.muzzleSmokeSpreadDegrees < 0.0f) cfg.muzzleSmokeSpreadDegrees = 0.0f;
    cfg.muzzleSmokeForwardBias = Clamp(cfg.muzzleSmokeForwardBias, 0.0f, 1.0f);
}

static void ReadNpcBallisticImpactEffectsConfig(
        const json& entry,
        TopdownBallisticImpactEffectConfig& outCfg)
{
    auto it = entry.find("ballisticImpactEffects");
    if (it == entry.end() || !it->is_object()) {
        ClampBallisticImpactEffectsConfig(outCfg);
        return;
    }

    const json& fx = *it;
    outCfg.wallImpactParticleCount =
            fx.value("wallImpactParticleCount", outCfg.wallImpactParticleCount);
    outCfg.wallImpactParticleSpeedMin =
            fx.value("wallImpactParticleSpeedMin", outCfg.wallImpactParticleSpeedMin);
    outCfg.wallImpactParticleSpeedMax =
            fx.value("wallImpactParticleSpeedMax", outCfg.wallImpactParticleSpeedMax);
    outCfg.wallImpactParticleLifetimeMsMin =
            fx.value("wallImpactParticleLifetimeMsMin", outCfg.wallImpactParticleLifetimeMsMin);
    outCfg.wallImpactParticleLifetimeMsMax =
            fx.value("wallImpactParticleLifetimeMsMax", outCfg.wallImpactParticleLifetimeMsMax);
    outCfg.wallImpactParticleSizeMin =
            fx.value("wallImpactParticleSizeMin", outCfg.wallImpactParticleSizeMin);
    outCfg.wallImpactParticleSizeMax =
            fx.value("wallImpactParticleSizeMax", outCfg.wallImpactParticleSizeMax);
    outCfg.wallImpactSpreadDegrees =
            fx.value("wallImpactSpreadDegrees", outCfg.wallImpactSpreadDegrees);

    ClampBallisticImpactEffectsConfig(outCfg);
}

static void ReadNpcMuzzleEffectsConfig(
        const json& entry,
        TopdownMuzzleEffectConfig& outCfg)
{
    auto it = entry.find("muzzleEffects");
    if (it == entry.end() || !it->is_object()) {
        ClampMuzzleEffectsConfig(outCfg);
        return;
    }

    const json& fx = *it;
    outCfg.muzzleX = fx.value("muzzleX", outCfg.muzzleX);
    outCfg.muzzleY = fx.value("muzzleY", outCfg.muzzleY);
    outCfg.muzzleFlashLifetimeMs =
            fx.value("muzzleFlashLifetimeMs", outCfg.muzzleFlashLifetimeMs);
    outCfg.muzzleFlashForwardLength =
            fx.value("muzzleFlashForwardLength", outCfg.muzzleFlashForwardLength);
    outCfg.muzzleFlashSideWidth =
            fx.value("muzzleFlashSideWidth", outCfg.muzzleFlashSideWidth);
    outCfg.muzzleSmokeParticleCount =
            fx.value("muzzleSmokeParticleCount", outCfg.muzzleSmokeParticleCount);
    outCfg.muzzleSmokeSpeedMin =
            fx.value("muzzleSmokeSpeedMin", outCfg.muzzleSmokeSpeedMin);
    outCfg.muzzleSmokeSpeedMax =
            fx.value("muzzleSmokeSpeedMax", outCfg.muzzleSmokeSpeedMax);
    outCfg.muzzleSmokeLifetimeMsMin =
            fx.value("muzzleSmokeLifetimeMsMin", outCfg.muzzleSmokeLifetimeMsMin);
    outCfg.muzzleSmokeLifetimeMsMax =
            fx.value("muzzleSmokeLifetimeMsMax", outCfg.muzzleSmokeLifetimeMsMax);
    outCfg.muzzleSmokeSizeMin =
            fx.value("muzzleSmokeSizeMin", outCfg.muzzleSmokeSizeMin);
    outCfg.muzzleSmokeSizeMax =
            fx.value("muzzleSmokeSizeMax", outCfg.muzzleSmokeSizeMax);
    outCfg.muzzleSmokeSpreadDegrees =
            fx.value("muzzleSmokeSpreadDegrees", outCfg.muzzleSmokeSpreadDegrees);
    outCfg.muzzleSmokeForwardBias =
            fx.value("muzzleSmokeForwardBias", outCfg.muzzleSmokeForwardBias);

    ClampMuzzleEffectsConfig(outCfg);
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

    TopdownBloodEffectConfig& bloodCfg = outCfg.bloodEffects;
    bloodCfg.bloodImpactParticleCount =
            fx.value("bloodImpactParticleCount", bloodCfg.bloodImpactParticleCount);
    bloodCfg.bloodImpactParticleSpeedMin =
            fx.value("bloodImpactParticleSpeedMin", bloodCfg.bloodImpactParticleSpeedMin);
    bloodCfg.bloodImpactParticleSpeedMax =
            fx.value("bloodImpactParticleSpeedMax", bloodCfg.bloodImpactParticleSpeedMax);
    bloodCfg.bloodImpactParticleLifetimeMsMin =
            fx.value("bloodImpactParticleLifetimeMsMin", bloodCfg.bloodImpactParticleLifetimeMsMin);
    bloodCfg.bloodImpactParticleLifetimeMsMax =
            fx.value("bloodImpactParticleLifetimeMsMax", bloodCfg.bloodImpactParticleLifetimeMsMax);
    bloodCfg.bloodImpactParticleSizeMin =
            fx.value("bloodImpactParticleSizeMin", bloodCfg.bloodImpactParticleSizeMin);
    bloodCfg.bloodImpactParticleSizeMax =
            fx.value("bloodImpactParticleSizeMax", bloodCfg.bloodImpactParticleSizeMax);
    bloodCfg.bloodImpactSpreadDegrees =
            fx.value("bloodImpactSpreadDegrees", bloodCfg.bloodImpactSpreadDegrees);

    bloodCfg.bloodDecalCountMin =
            fx.value("bloodDecalCountMin", bloodCfg.bloodDecalCountMin);
    bloodCfg.bloodDecalCountMax =
            fx.value("bloodDecalCountMax", bloodCfg.bloodDecalCountMax);
    bloodCfg.bloodDecalDistanceMin =
            fx.value("bloodDecalDistanceMin", bloodCfg.bloodDecalDistanceMin);
    bloodCfg.bloodDecalDistanceMax =
            fx.value("bloodDecalDistanceMax", bloodCfg.bloodDecalDistanceMax);
    bloodCfg.bloodDecalRadiusMin =
            fx.value("bloodDecalRadiusMin", bloodCfg.bloodDecalRadiusMin);
    bloodCfg.bloodDecalRadiusMax =
            fx.value("bloodDecalRadiusMax", bloodCfg.bloodDecalRadiusMax);
    bloodCfg.bloodDecalSpreadDegrees =
            fx.value("bloodDecalSpreadDegrees", bloodCfg.bloodDecalSpreadDegrees);
    bloodCfg.bloodDecalWallPadding =
            fx.value("bloodDecalWallPadding", bloodCfg.bloodDecalWallPadding);
    bloodCfg.bloodDecalOpacityMin =
            fx.value("bloodDecalOpacityMin", bloodCfg.bloodDecalOpacityMin);
    bloodCfg.bloodDecalOpacityMax =
            fx.value("bloodDecalOpacityMax", bloodCfg.bloodDecalOpacityMax);

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

        {
            const std::string attackTypeStr =
                    entry.value("attackType", std::string("none"));

            if (!ParseTopdownAttackTypeString(attackTypeStr, def.attackType)) {
                TraceLog(LOG_WARNING,
                         "NPC definition '%s' has invalid attackType '%s', defaulting to none",
                         def.assetId.c_str(),
                         attackTypeStr.c_str());
                def.attackType = TopdownAttackType::None;
            }
        }

        {
            const std::string tracerStyleStr =
                    entry.value("rangedTracerStyle", std::string("handgun"));

            if (!ParseTopdownTracerStyleString(tracerStyleStr, def.rangedTracerStyle)) {
                TraceLog(LOG_WARNING,
                         "NPC definition '%s' has invalid rangedTracerStyle '%s', defaulting to handgun",
                         def.assetId.c_str(),
                         tracerStyleStr.c_str());
                def.rangedTracerStyle = TopdownTracerStyle::Handgun;
            }
        }

        def.rangedPelletCount = entry.value("rangedPelletCount", 1);
        def.rangedSpreadDegrees = entry.value("rangedSpreadDegrees", 6.0f);
        def.rangedMaxRange = entry.value("rangedMaxRange", 800.0f);
        ReadNpcBallisticImpactEffectsConfig(entry, def.ballisticImpactEffects);
        ReadNpcMuzzleEffectsConfig(entry, def.muzzleEffects);
        def.reactionTimeMs = entry.value("reactionTimeMs", 180.0f);
        def.aimInaccuracyMinDegrees = entry.value("aimInaccuracyMinDegrees", 2.0f);
        def.aimInaccuracyMaxDegrees = entry.value("aimInaccuracyMaxDegrees", 10.0f);

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

        if (def.rangedPelletCount < 1) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has rangedPelletCount < 1, clamping to 1",
                     def.assetId.c_str());
            def.rangedPelletCount = 1;
        }

        if (def.rangedSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has rangedSpreadDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedSpreadDegrees = 0.0f;
        }

        if (def.rangedMaxRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has rangedMaxRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedMaxRange = 0.0f;
        }

        if (def.reactionTimeMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has reactionTimeMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.reactionTimeMs = 0.0f;
        }

        if (def.aimInaccuracyMinDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has aimInaccuracyMinDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.aimInaccuracyMinDegrees = 0.0f;
        }

        if (def.aimInaccuracyMaxDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has aimInaccuracyMaxDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.aimInaccuracyMaxDegrees = 0.0f;
        }

        if (def.aimInaccuracyMaxDegrees < def.aimInaccuracyMinDegrees) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has aimInaccuracyMaxDegrees < aimInaccuracyMinDegrees, clamping max up to min",
                     def.assetId.c_str());
            def.aimInaccuracyMaxDegrees = def.aimInaccuracyMinDegrees;
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

    runtime.attackType = def->attackType;
    runtime.rangedTracerStyle = def->rangedTracerStyle;
    runtime.rangedPelletCount = def->rangedPelletCount;
    runtime.rangedSpreadDegrees = def->rangedSpreadDegrees;
    runtime.rangedMaxRange = def->rangedMaxRange;
    runtime.ballisticImpactEffects = def->ballisticImpactEffects;
    runtime.muzzleEffects = def->muzzleEffects;
    runtime.reactionTimeMs = def->reactionTimeMs;
    runtime.aimInaccuracyMinDegrees = def->aimInaccuracyMinDegrees;
    runtime.aimInaccuracyMaxDegrees = def->aimInaccuracyMaxDegrees;

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

static bool TryResolveSmartSpawnPosition(
        const TopdownRuntimeData& runtime,
        Vector2 preferredPosition,
        float npcRadius,
        Vector2& outPosition)
{
    TopdownNpcRingSlotBuildConfig ringConfig;
    ringConfig.candidatePadding = 4.0f;
    ringConfig.maxRings = 3;
    ringConfig.minRadiusStep = 12.0f;
    ringConfig.raycastEpsilon = 0.001f;
    ringConfig.includeOriginCandidate = true;

    std::vector<Vector2> slots;
    TopdownCollectValidNpcRingSlots(
            runtime,
            preferredPosition,
            npcRadius,
            ringConfig,
            slots,
            -1,
            1);

    if (slots.empty()) {
        return false;
    }

    outPosition = slots.front();
    return true;
}

bool TopdownSpawnNpcRuntime(
        GameState& state,
        const std::string& npcId,
        const std::string& assetId,
        Vector2 position,
        float orientationDegrees,
        bool visible,
        bool persistentChase,
        bool guard,
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
    npc.guard = guard;
    npc.aiMode = asset->aiMode;
    npc.engagementState = guard
            ? TopdownNpcEngagementState::Guarding
            : TopdownNpcEngagementState::Unaware;
    npc.combatState = TopdownNpcCombatState::None;

    npc.visionRange = asset->visionRange;
    npc.hearingRange = asset->hearingRange;
    npc.gunshotHearingRange = asset->gunshotHearingRange;
    npc.visionHalfAngleDegrees = asset->visionHalfAngleDegrees;

    npc.attackRange = asset->attackRange;
    npc.preferredAttackRangeFactor = RandomRangeFloat(0.8f, 1.0f);
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

    npc.hasPlayerTarget = false;
    npc.lastKnownPlayerPosition = {};
    npc.repathTimerMs = 0.0f;

    npc.attackHitPending = false;
    npc.attackHitApplied = false;
    npc.attackStateTimeMs = 0.0f;
    npc.attackAnimationDurationMs = 0.0f;

    npc.strafeDir = (GetRandomValue(0, 1) == 0) ? -1 : 1;
    npc.strafeTimerMs = RandomRangeFloat(400.0f, 1200.0f);

    npc.renderOpacity = 1.0f;

    npc.position = position;
    npc.collisionRadius = asset->collisionRadius;

    const float radians = orientationDegrees * DEG2RAD;
    npc.facing.x = std::cos(radians);
    npc.facing.y = std::sin(radians);
    npc.rotationRadians = radians;
    npc.guardHomePosition = position;
    npc.hasGuardHomePosition = true;
    npc.guardLookAtSoundTimerMs = 0.0f;
    npc.guardLookAtSoundRadians = npc.rotationRadians;

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
