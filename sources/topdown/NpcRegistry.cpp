#include "topdown/NpcRegistry.h"

#include <exception>
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


static int HexDigitValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool ParseNpcHexColorString(const std::string& text, Color& outColor)
{
    const size_t start = (!text.empty() && text[0] == '#') ? 1 : 0;
    if (text.size() - start != 6) {
        return false;
    }

    unsigned char channels[3]{};
    for (int channel = 0; channel < 3; ++channel) {
        const int hi = HexDigitValue(text[start + channel * 2]);
        const int lo = HexDigitValue(text[start + channel * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        channels[channel] = static_cast<unsigned char>((hi << 4) | lo);
    }

    outColor = Color{channels[0], channels[1], channels[2], static_cast<unsigned char>(255)};
    return true;
}

static bool ReadRequiredNpcHexColor(
        const json& colors,
        const char* key,
        const char* npcAssetId,
        Color& outColor)
{
    auto it = colors.find(key);
    if (it == colors.end() || !it->is_string()) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has invalid sourceColors: missing string '%s'",
                 npcAssetId,
                 key);
        return false;
    }

    const std::string value = it->get<std::string>();
    if (!ParseNpcHexColorString(value, outColor)) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has invalid source color '%s' for key '%s'",
                 npcAssetId,
                 value.c_str(),
                 key);
        return false;
    }

    return true;
}

static bool ReadNpcSourceColors(
        const json& entry,
        TopdownNpcAssetDefinition& def)
{
    auto it = entry.find("sourceColors");
    if (it == entry.end()) {
        return false;
    }

    if (!it->is_object()) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has invalid sourceColors block; color substitution disabled",
                 def.assetId.c_str());
        return false;
    }

    const json& colors = *it;
    bool valid = true;

    valid = ReadRequiredNpcHexColor(colors, "skin1", def.assetId.c_str(), def.colorSubstitution.sourceSkin.colors[0]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "skin2", def.assetId.c_str(), def.colorSubstitution.sourceSkin.colors[1]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "skin3", def.assetId.c_str(), def.colorSubstitution.sourceSkin.colors[2]) && valid;

    valid = ReadRequiredNpcHexColor(colors, "hair1", def.assetId.c_str(), def.colorSubstitution.sourceHair.colors[0]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "hair2", def.assetId.c_str(), def.colorSubstitution.sourceHair.colors[1]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "hair3", def.assetId.c_str(), def.colorSubstitution.sourceHair.colors[2]) && valid;

    valid = ReadRequiredNpcHexColor(colors, "chest1", def.assetId.c_str(), def.colorSubstitution.sourceChest.colors[0]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "chest2", def.assetId.c_str(), def.colorSubstitution.sourceChest.colors[1]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "chest3", def.assetId.c_str(), def.colorSubstitution.sourceChest.colors[2]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "chest4", def.assetId.c_str(), def.colorSubstitution.sourceChest.colors[3]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "chest5", def.assetId.c_str(), def.colorSubstitution.sourceChest.colors[4]) && valid;

    valid = ReadRequiredNpcHexColor(colors, "legs1", def.assetId.c_str(), def.colorSubstitution.sourceLegs.colors[0]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "legs2", def.assetId.c_str(), def.colorSubstitution.sourceLegs.colors[1]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "legs3", def.assetId.c_str(), def.colorSubstitution.sourceLegs.colors[2]) && valid;
    valid = ReadRequiredNpcHexColor(colors, "legs4", def.assetId.c_str(), def.colorSubstitution.sourceLegs.colors[3]) && valid;

    if (!valid) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has incomplete or invalid sourceColors; color substitution disabled",
                 def.assetId.c_str());
    }

    return valid;
}

static bool HasNpcColorPreset3(
        const std::vector<TopdownNamedNpcColorSet3>& presets,
        const std::string& name)
{
    for (const TopdownNamedNpcColorSet3& preset : presets) {
        if (preset.name == name) return true;
    }
    return false;
}

static bool HasNpcColorPreset4(
        const std::vector<TopdownNamedNpcColorSet4>& presets,
        const std::string& name)
{
    for (const TopdownNamedNpcColorSet4& preset : presets) {
        if (preset.name == name) return true;
    }
    return false;
}

static bool HasNpcColorPreset5(
        const std::vector<TopdownNamedNpcColorSet5>& presets,
        const std::string& name)
{
    for (const TopdownNamedNpcColorSet5& preset : presets) {
        if (preset.name == name) return true;
    }
    return false;
}

static bool IsValidNpcRequestedPreset3(
        const std::vector<TopdownNamedNpcColorSet3>& presets,
        const std::string& name)
{
    return name == "Random" || HasNpcColorPreset3(presets, name);
}

static bool IsValidNpcRequestedPreset4(
        const std::vector<TopdownNamedNpcColorSet4>& presets,
        const std::string& name)
{
    return name == "Random" || HasNpcColorPreset4(presets, name);
}

static bool IsValidNpcRequestedPreset5(
        const std::vector<TopdownNamedNpcColorSet5>& presets,
        const std::string& name)
{
    return name == "Random" || HasNpcColorPreset5(presets, name);
}

static bool ReadRequiredNpcPresetId(
        const json& colors,
        const char* key,
        const char* npcAssetId,
        std::string& outPresetId)
{
    auto it = colors.find(key);
    if (it == colors.end() || !it->is_string()) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has invalid colors: missing string '%s'",
                 npcAssetId,
                 key);
        return false;
    }

    outPresetId = it->get<std::string>();
    if (outPresetId.empty()) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has empty color preset id for '%s'",
                 npcAssetId,
                 key);
        return false;
    }

    return true;
}

static bool ReadNpcRequestedColorPresets(
        const json& entry,
        const TopdownNpcColorPresetRegistry& registry,
        TopdownNpcAssetDefinition& def)
{
    auto it = entry.find("colors");
    if (it == entry.end()) {
        return false;
    }

    if (!it->is_object()) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has invalid colors block; color substitution disabled",
                 def.assetId.c_str());
        return false;
    }

    const json& colors = *it;
    bool valid = true;
    valid = ReadRequiredNpcPresetId(colors, "skin", def.assetId.c_str(), def.colorSubstitution.skinPresetId) && valid;
    valid = ReadRequiredNpcPresetId(colors, "hair", def.assetId.c_str(), def.colorSubstitution.hairPresetId) && valid;
    valid = ReadRequiredNpcPresetId(colors, "chest", def.assetId.c_str(), def.colorSubstitution.chestPresetId) && valid;
    valid = ReadRequiredNpcPresetId(colors, "legs", def.assetId.c_str(), def.colorSubstitution.legsPresetId) && valid;

    if (!valid) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has incomplete colors block; color substitution disabled",
                 def.assetId.c_str());
        return false;
    }

    if (!registry.loaded) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' requested color presets, but NPC color preset registry is not loaded; color substitution disabled",
                 def.assetId.c_str());
        return false;
    }

    if (!IsValidNpcRequestedPreset3(registry.skinPresets, def.colorSubstitution.skinPresetId)) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' requested unknown skin color preset '%s'; color substitution disabled",
                 def.assetId.c_str(),
                 def.colorSubstitution.skinPresetId.c_str());
        valid = false;
    }
    if (!IsValidNpcRequestedPreset3(registry.hairPresets, def.colorSubstitution.hairPresetId)) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' requested unknown hair color preset '%s'; color substitution disabled",
                 def.assetId.c_str(),
                 def.colorSubstitution.hairPresetId.c_str());
        valid = false;
    }
    if (!IsValidNpcRequestedPreset5(registry.chestPresets, def.colorSubstitution.chestPresetId)) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' requested unknown chest color preset '%s'; color substitution disabled",
                 def.assetId.c_str(),
                 def.colorSubstitution.chestPresetId.c_str());
        valid = false;
    }
    if (!IsValidNpcRequestedPreset4(registry.legsPresets, def.colorSubstitution.legsPresetId)) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' requested unknown legs color preset '%s'; color substitution disabled",
                 def.assetId.c_str(),
                 def.colorSubstitution.legsPresetId.c_str());
        valid = false;
    }

    return valid;
}

static void ReadNpcColorSubstitutionConfig(
        const json& entry,
        const TopdownNpcColorPresetRegistry& registry,
        TopdownNpcAssetDefinition& def)
{
    def.colorSubstitution = {};

    const bool hasSourceColors = entry.contains("sourceColors");
    const bool hasColors = entry.contains("colors");

    if (!hasSourceColors && hasColors) {
        TraceLog(LOG_WARNING,
                 "NPC definition '%s' has colors without sourceColors; color substitution disabled",
                 def.assetId.c_str());
        return;
    }

    if (!hasSourceColors) {
        return;
    }

    if (!hasColors) {
        return;
    }

    const bool sourceValid = ReadNpcSourceColors(entry, def);
    const bool presetsValid = ReadNpcRequestedColorPresets(entry, registry, def);

    def.colorSubstitution.active = sourceValid && presetsValid;
}

static bool ParseNpcPresetColorArray(
        const json& entry,
        int requiredCount,
        Color* outColors)
{
    if (!entry.is_array() || static_cast<int>(entry.size()) != requiredCount) {
        return false;
    }

    for (int i = 0; i < requiredCount; ++i) {
        if (!entry[i].is_string()) {
            return false;
        }

        if (!ParseNpcHexColorString(entry[i].get<std::string>(), outColors[i])) {
            return false;
        }
    }

    return true;
}

static bool NpcPresetNameExists3(
        const std::vector<TopdownNamedNpcColorSet3>& presets,
        const std::string& name)
{
    return HasNpcColorPreset3(presets, name);
}

static bool NpcPresetNameExists4(
        const std::vector<TopdownNamedNpcColorSet4>& presets,
        const std::string& name)
{
    return HasNpcColorPreset4(presets, name);
}

static bool NpcPresetNameExists5(
        const std::vector<TopdownNamedNpcColorSet5>& presets,
        const std::string& name)
{
    return HasNpcColorPreset5(presets, name);
}

static void LoadNpcColorPresetCategory3(
        const json& root,
        const char* categoryName,
        std::vector<TopdownNamedNpcColorSet3>& outPresets)
{
    auto categoryIt = root.find(categoryName);
    if (categoryIt == root.end() || !categoryIt->is_object()) {
        TraceLog(LOG_WARNING,
                 "NPC color preset registry missing object category '%s'",
                 categoryName);
        return;
    }

    for (auto presetIt = categoryIt->begin(); presetIt != categoryIt->end(); ++presetIt) {
        const std::string name = presetIt.key();
        if (NpcPresetNameExists3(outPresets, name)) {
            TraceLog(LOG_WARNING,
                     "Duplicate NPC %s color preset '%s'; skipping duplicate",
                     categoryName,
                     name.c_str());
            continue;
        }

        TopdownNamedNpcColorSet3 preset;
        preset.name = name;
        if (!ParseNpcPresetColorArray(*presetIt, 3, preset.set.colors)) {
            TraceLog(LOG_WARNING,
                     "NPC %s color preset '%s' has invalid color array; skipping",
                     categoryName,
                     name.c_str());
            continue;
        }
        outPresets.push_back(preset);
    }
}

static void LoadNpcColorPresetCategory4(
        const json& root,
        const char* categoryName,
        std::vector<TopdownNamedNpcColorSet4>& outPresets)
{
    auto categoryIt = root.find(categoryName);
    if (categoryIt == root.end() || !categoryIt->is_object()) {
        TraceLog(LOG_WARNING,
                 "NPC color preset registry missing object category '%s'",
                 categoryName);
        return;
    }

    for (auto presetIt = categoryIt->begin(); presetIt != categoryIt->end(); ++presetIt) {
        const std::string name = presetIt.key();
        if (NpcPresetNameExists4(outPresets, name)) {
            TraceLog(LOG_WARNING,
                     "Duplicate NPC %s color preset '%s'; skipping duplicate",
                     categoryName,
                     name.c_str());
            continue;
        }

        TopdownNamedNpcColorSet4 preset;
        preset.name = name;
        if (!ParseNpcPresetColorArray(*presetIt, 4, preset.set.colors)) {
            TraceLog(LOG_WARNING,
                     "NPC %s color preset '%s' has invalid color array; skipping",
                     categoryName,
                     name.c_str());
            continue;
        }
        outPresets.push_back(preset);
    }
}

static void LoadNpcColorPresetCategory5(
        const json& root,
        const char* categoryName,
        std::vector<TopdownNamedNpcColorSet5>& outPresets)
{
    auto categoryIt = root.find(categoryName);
    if (categoryIt == root.end() || !categoryIt->is_object()) {
        TraceLog(LOG_WARNING,
                 "NPC color preset registry missing object category '%s'",
                 categoryName);
        return;
    }

    for (auto presetIt = categoryIt->begin(); presetIt != categoryIt->end(); ++presetIt) {
        const std::string name = presetIt.key();
        if (NpcPresetNameExists5(outPresets, name)) {
            TraceLog(LOG_WARNING,
                     "Duplicate NPC %s color preset '%s'; skipping duplicate",
                     categoryName,
                     name.c_str());
            continue;
        }

        TopdownNamedNpcColorSet5 preset;
        preset.name = name;
        if (!ParseNpcPresetColorArray(*presetIt, 5, preset.set.colors)) {
            TraceLog(LOG_WARNING,
                     "NPC %s color preset '%s' has invalid color array; skipping",
                     categoryName,
                     name.c_str());
            continue;
        }
        outPresets.push_back(preset);
    }
}

static void LoadNpcColorPresetRegistry(GameState& state)
{
    TopdownNpcColorPresetRegistry& registry = state.topdown.npcColorPresets;
    registry = {};

    const fs::path jsonPath = fs::path(ASSETS_PATH "npc_color_presets.json");
    if (!fs::exists(jsonPath) || !fs::is_regular_file(jsonPath)) {
        TraceLog(LOG_WARNING,
                 "NPC color preset registry missing: %s",
                 jsonPath.string().c_str());
        return;
    }

    json root;
    try {
        std::ifstream in(jsonPath);
        if (!in.is_open()) {
            TraceLog(LOG_WARNING,
                     "Failed opening NPC color preset registry: %s",
                     jsonPath.string().c_str());
            return;
        }
        in >> root;
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING,
                 "Failed parsing NPC color preset registry '%s': %s",
                 jsonPath.string().c_str(),
                 e.what());
        return;
    }

    if (!root.is_object()) {
        TraceLog(LOG_WARNING,
                 "NPC color preset registry root must be an object: %s",
                 jsonPath.string().c_str());
        return;
    }

    LoadNpcColorPresetCategory3(root, "skin", registry.skinPresets);
    LoadNpcColorPresetCategory3(root, "hair", registry.hairPresets);
    LoadNpcColorPresetCategory5(root, "chest", registry.chestPresets);
    LoadNpcColorPresetCategory4(root, "legs", registry.legsPresets);

    registry.loaded = !registry.skinPresets.empty() &&
            !registry.hairPresets.empty() &&
            !registry.chestPresets.empty() &&
            !registry.legsPresets.empty();

    TraceLog(LOG_INFO,
             "Loaded NPC color presets: skin=%d hair=%d chest=%d legs=%d loaded=%s",
             static_cast<int>(registry.skinPresets.size()),
             static_cast<int>(registry.hairPresets.size()),
             static_cast<int>(registry.chestPresets.size()),
             static_cast<int>(registry.legsPresets.size()),
             registry.loaded ? "yes" : "no");
}

static const TopdownNamedNpcColorSet3* ResolveNpcColorPreset3(
        const std::vector<TopdownNamedNpcColorSet3>& presets,
        const std::string& requestedId)
{
    if (presets.empty()) return nullptr;

    if (requestedId == "Random") {
        return &presets[GetRandomValue(0, static_cast<int>(presets.size()) - 1)];
    }

    for (const TopdownNamedNpcColorSet3& preset : presets) {
        if (preset.name == requestedId) return &preset;
    }
    return nullptr;
}

static const TopdownNamedNpcColorSet4* ResolveNpcColorPreset4(
        const std::vector<TopdownNamedNpcColorSet4>& presets,
        const std::string& requestedId)
{
    if (presets.empty()) return nullptr;

    if (requestedId == "Random") {
        return &presets[GetRandomValue(0, static_cast<int>(presets.size()) - 1)];
    }

    for (const TopdownNamedNpcColorSet4& preset : presets) {
        if (preset.name == requestedId) return &preset;
    }
    return nullptr;
}

static const TopdownNamedNpcColorSet5* ResolveNpcColorPreset5(
        const std::vector<TopdownNamedNpcColorSet5>& presets,
        const std::string& requestedId)
{
    if (presets.empty()) return nullptr;

    if (requestedId == "Random") {
        return &presets[GetRandomValue(0, static_cast<int>(presets.size()) - 1)];
    }

    for (const TopdownNamedNpcColorSet5& preset : presets) {
        if (preset.name == requestedId) return &preset;
    }
    return nullptr;
}

static bool ResolveNpcColorSubstitutionForSpawn(
        const TopdownNpcColorPresetRegistry& registry,
        const TopdownNpcAssetRuntime& asset,
        TopdownNpcRuntime& npc)
{
    npc.colorSubstitution = {};

    if (!asset.colorSubstitution.active) {
        return true;
    }

    if (!registry.loaded) {
        TraceLog(LOG_WARNING,
                 "NPC '%s' asset '%s' has color substitution, but color preset registry is not loaded; disabling for this spawn",
                 npc.id.c_str(),
                 asset.assetId.c_str());
        return false;
    }

    const TopdownNamedNpcColorSet3* skin = ResolveNpcColorPreset3(registry.skinPresets, asset.colorSubstitution.skinPresetId);
    const TopdownNamedNpcColorSet3* hair = ResolveNpcColorPreset3(registry.hairPresets, asset.colorSubstitution.hairPresetId);
    const TopdownNamedNpcColorSet5* chest = ResolveNpcColorPreset5(registry.chestPresets, asset.colorSubstitution.chestPresetId);
    const TopdownNamedNpcColorSet4* legs = ResolveNpcColorPreset4(registry.legsPresets, asset.colorSubstitution.legsPresetId);

    if (skin == nullptr || hair == nullptr || chest == nullptr || legs == nullptr) {
        TraceLog(LOG_WARNING,
                 "NPC '%s' asset '%s' failed resolving color presets; disabling for this spawn",
                 npc.id.c_str(),
                 asset.assetId.c_str());
        npc.colorSubstitution = {};
        return false;
    }

    npc.colorSubstitution.active = true;
    npc.colorSubstitution.dstSkin = skin->set;
    npc.colorSubstitution.dstHair = hair->set;
    npc.colorSubstitution.dstChest = chest->set;
    npc.colorSubstitution.dstLegs = legs->set;
    npc.colorSubstitution.resolvedSkinPresetId = skin->name;
    npc.colorSubstitution.resolvedHairPresetId = hair->name;
    npc.colorSubstitution.resolvedChestPresetId = chest->name;
    npc.colorSubstitution.resolvedLegsPresetId = legs->name;

    return true;
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

    def.meleeAttack.attackStartSoundId = sounds.value("attackStart", std::string());
    def.meleeAttack.attackConnectSoundId = sounds.value("attackConnect", std::string());

    auto hitReactionIt = sounds.find("hitReaction");
    if (hitReactionIt != sounds.end() && hitReactionIt->is_array()) {
        for (const auto& soundEntry : *hitReactionIt) {
            if (soundEntry.is_string()) {
                def.meleeAttack.hitReactionSoundIds.push_back(soundEntry.get<std::string>());
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
        ReadNpcColorSubstitutionConfig(entry, state.topdown.npcColorPresets, def);
        def.movement.baseDrawScale = entry.value("baseDrawScale", 1.0f);
        def.movement.collisionRadius = entry.value("collisionRadius", 32.0f);
        def.movement.walkSpeed = entry.value("walkSpeed", 450.0f);
        def.movement.runSpeed = entry.value("runSpeed", 700.0f);
        def.movement.hurtStunMs = entry.value("hurtStunMs", 0.0f);
        def.movement.maxHealth = entry.value("maxHealth", 100.0f);
        def.movement.corpseExpirationMs = entry.value("corpseExpirationMs", -1.0f);

        def.ai.hostile = entry.value("hostile", true);

        {
            const std::string aiModeStr =
                    entry.value("aiMode", std::string("none"));

            if (!ParseNpcAiModeString(aiModeStr, def.ai.aiMode)) {
                TraceLog(LOG_WARNING,
                         "NPC definition '%s' has invalid aiMode '%s', defaulting to none",
                         def.assetId.c_str(),
                         aiModeStr.c_str());
                def.ai.aiMode = TopdownNpcAiMode::None;
            }
        }

        {
            const std::string attackTypeStr =
                    entry.value("attackType", std::string("none"));

            if (!ParseTopdownAttackTypeString(attackTypeStr, def.ai.attackType)) {
                TraceLog(LOG_WARNING,
                         "NPC definition '%s' has invalid attackType '%s', defaulting to none",
                         def.assetId.c_str(),
                         attackTypeStr.c_str());
                def.ai.attackType = TopdownAttackType::None;
            }
        }

        {
            const std::string tracerStyleStr =
                    entry.value("rangedTracerStyle", std::string("handgun"));

            if (!ParseTopdownTracerStyleString(tracerStyleStr, def.rangedAttack.rangedTracerStyle)) {
                TraceLog(LOG_WARNING,
                         "NPC definition '%s' has invalid rangedTracerStyle '%s', defaulting to handgun",
                         def.assetId.c_str(),
                         tracerStyleStr.c_str());
                def.rangedAttack.rangedTracerStyle = TopdownTracerStyle::Handgun;
            }
        }

        def.rangedAttack.rangedPelletCount = entry.value("rangedPelletCount", 1);
        def.rangedAttack.rangedSpreadDegrees = entry.value("rangedSpreadDegrees", 6.0f);
        def.rangedAttack.rangedMaxRange = entry.value("rangedMaxRange", 800.0f);
        ReadNpcBallisticImpactEffectsConfig(entry, def.rangedAttack.ballisticImpactEffects);
        ReadNpcMuzzleEffectsConfig(entry, def.rangedAttack.muzzleEffects);
        def.rangedAttack.reactionTimeMs = entry.value("reactionTimeMs", 180.0f);
        def.rangedAttack.aimInaccuracyMinDegrees = entry.value("aimInaccuracyMinDegrees", 2.0f);
        def.rangedAttack.aimInaccuracyMaxDegrees = entry.value("aimInaccuracyMaxDegrees", 10.0f);

        def.perception.visionRange = entry.value("visionRange", 700.0f);
        def.perception.hearingRange = entry.value("hearingRange", 220.0f);
        def.perception.gunshotHearingRange = entry.value("gunshotHearingRange", 1000.0f);
        def.perception.visionHalfAngleDegrees = entry.value("visionHalfAngleDegrees", 65.0f);

        def.meleeAttack.attackRange = entry.value("attackRange", 95.0f);
        def.meleeAttack.attackCooldownMs = entry.value("attackCooldownMs", 900.0f);
        def.meleeAttack.attackDamage = entry.value("attackDamage", 25.0f);
        def.meleeAttack.attackHitNormalizedTime = entry.value("attackHitNormalizedTime", 0.7f);
        def.meleeAttack.attackRecoverMs = entry.value("attackRecoverMs", 250.0f);
        def.meleeAttack.meleeHitPosX = entry.value("meleeHitPosX", 0.0f);
        def.meleeAttack.meleeHitPosY = entry.value("meleeHitPosY", 0.0f);

        ReadNpcSounds(entry, def);
        ReadNpcAttackEffectsConfig(entry, def.meleeAttack.attackEffects);

        def.ai.chaseRepathIntervalMs = entry.value("chaseRepathIntervalMs", 250.0f);

        if (def.movement.hurtStunMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has hurtStunMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.movement.hurtStunMs = 0.0f;
        }

        if (def.movement.maxHealth <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has maxHealth <= 0, clamping to 1",
                     def.assetId.c_str());
            def.movement.maxHealth = 1.0f;
        }

        if (def.movement.corpseExpirationMs < 0.0f && def.movement.corpseExpirationMs != -1.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has invalid corpseExpirationMs %.2f, using -1",
                     def.assetId.c_str(),
                     def.movement.corpseExpirationMs);
            def.movement.corpseExpirationMs = -1.0f;
        }

        if (def.perception.visionRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has visionRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.perception.visionRange = 0.0f;
        }

        if (def.perception.hearingRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has hearingRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.perception.hearingRange = 0.0f;
        }

        if (def.perception.gunshotHearingRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has gunshotHearingRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.perception.gunshotHearingRange = 0.0f;
        }

        if (def.perception.visionHalfAngleDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has visionHalfAngleDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.perception.visionHalfAngleDegrees = 0.0f;
        } else if (def.perception.visionHalfAngleDegrees > 180.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has visionHalfAngleDegrees > 180, clamping to 180",
                     def.assetId.c_str());
            def.perception.visionHalfAngleDegrees = 180.0f;
        }

        if (def.meleeAttack.attackRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.meleeAttack.attackRange = 0.0f;
        }

        if (def.meleeAttack.attackCooldownMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackCooldownMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.meleeAttack.attackCooldownMs = 0.0f;
        }

        if (def.meleeAttack.attackDamage < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackDamage < 0, clamping to 0",
                     def.assetId.c_str());
            def.meleeAttack.attackDamage = 0.0f;
        }

        if (def.meleeAttack.attackHitNormalizedTime < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackHitNormalizedTime < 0, clamping to 0",
                     def.assetId.c_str());
            def.meleeAttack.attackHitNormalizedTime = 0.0f;
        } else if (def.meleeAttack.attackHitNormalizedTime > 1.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackHitNormalizedTime > 1, clamping to 1",
                     def.assetId.c_str());
            def.meleeAttack.attackHitNormalizedTime = 1.0f;
        }

        if (def.meleeAttack.attackRecoverMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has attackRecoverMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.meleeAttack.attackRecoverMs = 0.0f;
        }

        if (def.ai.chaseRepathIntervalMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has chaseRepathIntervalMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.ai.chaseRepathIntervalMs = 0.0f;
        }

        if (def.rangedAttack.rangedPelletCount < 1) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has rangedPelletCount < 1, clamping to 1",
                     def.assetId.c_str());
            def.rangedAttack.rangedPelletCount = 1;
        }

        if (def.rangedAttack.rangedSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has rangedSpreadDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedAttack.rangedSpreadDegrees = 0.0f;
        }

        if (def.rangedAttack.rangedMaxRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has rangedMaxRange < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedAttack.rangedMaxRange = 0.0f;
        }

        if (def.rangedAttack.reactionTimeMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has reactionTimeMs < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedAttack.reactionTimeMs = 0.0f;
        }

        if (def.rangedAttack.aimInaccuracyMinDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has aimInaccuracyMinDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedAttack.aimInaccuracyMinDegrees = 0.0f;
        }

        if (def.rangedAttack.aimInaccuracyMaxDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has aimInaccuracyMaxDegrees < 0, clamping to 0",
                     def.assetId.c_str());
            def.rangedAttack.aimInaccuracyMaxDegrees = 0.0f;
        }

        if (def.rangedAttack.aimInaccuracyMaxDegrees < def.rangedAttack.aimInaccuracyMinDegrees) {
            TraceLog(LOG_WARNING,
                     "NPC definition '%s' has aimInaccuracyMaxDegrees < aimInaccuracyMinDegrees, clamping max up to min",
                     def.assetId.c_str());
            def.rangedAttack.aimInaccuracyMaxDegrees = def.rangedAttack.aimInaccuracyMinDegrees;
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
    LoadNpcColorPresetRegistry(state);

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
                 def.movement.baseDrawScale,
                 def.movement.collisionRadius,
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
    runtime.movement = def->movement;
    runtime.ai = def->ai;
    runtime.rangedAttack = def->rangedAttack;
    runtime.perception = def->perception;
    runtime.meleeAttack = def->meleeAttack;
    runtime.colorSubstitution = def->colorSubstitution;

    for (const TopdownNpcAnimationSourceDefinition& animSource : def->animations) {
        SpriteAssetHandle spriteHandle = -1;

        if (animSource.hasOrigin) {
            spriteHandle = LoadSpriteAssetFromAsepriteJsonWithOrigin(
                    state.resources,
                    animSource.asepriteJsonPath.c_str(),
                    def->movement.baseDrawScale,
                    animSource.origin,
                    ResourceScope::Scene);
        } else {
            spriteHandle = LoadSpriteAssetFromAsepriteJson(
                    state.resources,
                    animSource.asepriteJsonPath.c_str(),
                    def->movement.baseDrawScale,
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
                asset->movement.collisionRadius,
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

    npc.movement = asset->movement;
    npc.ai = asset->ai;
    npc.rangedAttack = asset->rangedAttack;
    npc.perception = asset->perception;
    npc.meleeAttack = asset->meleeAttack;

    npc.health = asset->movement.maxHealth;
    npc.corpseElapsedMs = 0.0f;

    npc.persistentChase = persistentChase;
    npc.guard = guard;
    npc.engagementState = guard
            ? TopdownNpcEngagementState::Guarding
            : TopdownNpcEngagementState::Unaware;
    npc.combatState = TopdownNpcCombatState::None;

    npc.preferredAttackRangeFactor = RandomRangeFloat(0.8f, 1.0f);
    npc.attackCooldownRemainingMs = 0.0f;

    ResolveNpcColorSubstitutionForSpawn(state.topdown.npcColorPresets, *asset, npc);

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

    const float radians = orientationDegrees * DEG2RAD;
    npc.facing = TopdownDirectionFromAngle(radians);
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
