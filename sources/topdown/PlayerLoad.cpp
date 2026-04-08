#include "topdown/PlayerLoad.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#include "resources/AsepriteAsset.h"
#include "utils/json.hpp"
#include "raymath.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct PlayerAnimationOriginOverride {
    bool hasOrigin = false;
    Vector2 origin{};
};

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static PlayerAnimationOriginOverride ReadPlayerAnimationOriginOverride(
        const json& playerConfig,
        const std::string& animationId)
{
    PlayerAnimationOriginOverride out{};

    auto it = playerConfig.find(animationId);
    if (it == playerConfig.end() || !it->is_object()) {
        return out;
    }

    const json& obj = *it;

    if (!obj.contains("originX") || !obj.contains("originY")) {
        return out;
    }

    if (!obj["originX"].is_number() || !obj["originY"].is_number()) {
        return out;
    }

    out.hasOrigin = true;
    out.origin.x = obj["originX"].get<float>();
    out.origin.y = obj["originY"].get<float>();
    return out;
}

struct PlayerAnimationMuzzleOverride {
    bool hasMuzzle = false;
    Vector2 muzzle{};
};

static PlayerAnimationMuzzleOverride ReadPlayerAnimationMuzzleOverride(
        const json& playerConfig,
        const std::string& animationId)
{
    PlayerAnimationMuzzleOverride out{};

    auto it = playerConfig.find(animationId);
    if (it == playerConfig.end() || !it->is_object()) {
        return out;
    }

    const json& obj = *it;

    if (!obj.contains("muzzleX") || !obj.contains("muzzleY")) {
        return out;
    }

    if (!obj["muzzleX"].is_number() || !obj["muzzleY"].is_number()) {
        return out;
    }

    out.hasMuzzle = true;
    out.muzzle.x = obj["muzzleX"].get<float>();
    out.muzzle.y = obj["muzzleY"].get<float>();
    return out;
}

static bool ParseAttackTypeString(
        const std::string& s,
        TopdownAttackType& outType)
{
    if (s == "none") {
        outType = TopdownAttackType::None;
        return true;
    }

    if (s == "melee") {
        outType = TopdownAttackType::Melee;
        return true;
    }

    if (s == "ranged") {
        outType = TopdownAttackType::Ranged;
        return true;
    }

    return false;
}

static bool ParseTracerStyleString(
        const std::string& s,
        TopdownTracerStyle& outStyle)
{
    if (s.empty() || s == "none") {
        outStyle = TopdownTracerStyle::None;
        return true;
    }

    if (s == "handgun") {
        outStyle = TopdownTracerStyle::Handgun;
        return true;
    }

    if (s == "shotgun") {
        outStyle = TopdownTracerStyle::Shotgun;
        return true;
    }

    if (s == "rifle") {
        outStyle = TopdownTracerStyle::Rifle;
        return true;
    }

    return false;
}

static bool ParseFireModeString(
        const std::string& s,
        TopdownFireMode& outMode)
{
    if (s == "semi") {
        outMode = TopdownFireMode::SemiAuto;
        return true;
    }

    if (s == "full") {
        outMode = TopdownFireMode::FullAuto;
        return true;
    }

    if (s == "burst") {
        outMode = TopdownFireMode::Burst;
        return true;
    }

    return false;
}

static bool HasWeaponConfigForEquipmentSet(
        const TopdownCharacterAssetData& asset,
        const std::string& equipmentSetId)
{
    for (const TopdownPlayerWeaponConfig& cfg : asset.weaponConfigs) {
        if (cfg.equipmentSetId == equipmentSetId) {
            return true;
        }
    }

    return false;
}

static void ReadPlayerWeaponConfigs(
        const json& playerConfig,
        TopdownCharacterAssetData& asset)
{
    asset.weaponConfigs.clear();

    auto it = playerConfig.find("weaponConfigs");
    if (it == playerConfig.end()) {
        TraceLog(LOG_INFO, "Topdown player config has no weaponConfigs array");
        return;
    }

    if (!it->is_array()) {
        TraceLog(LOG_WARNING, "Topdown player config weaponConfigs is not an array");
        return;
    }

    for (const auto& entry : *it) {
        if (!entry.is_object()) {
            TraceLog(LOG_WARNING, "Skipping player weapon config entry: not an object");
            continue;
        }

        TopdownPlayerWeaponConfig cfg;
        cfg.equipmentSetId = entry.value("equipmentSetId", std::string());
        cfg.slot = entry.value("slot", 0);

        if (cfg.equipmentSetId.empty()) {
            TraceLog(LOG_WARNING, "Skipping player weapon config: missing equipmentSetId");
            continue;
        }

        if (cfg.slot <= 0) {
            TraceLog(LOG_WARNING,
                     "Skipping player weapon config '%s': invalid slot %d",
                     cfg.equipmentSetId.c_str(),
                     cfg.slot);
            continue;
        }

        if (HasWeaponConfigForEquipmentSet(asset, cfg.equipmentSetId)) {
            TraceLog(LOG_WARNING,
                     "Skipping duplicate player weapon config for equipmentSetId '%s'",
                     cfg.equipmentSetId.c_str());
            continue;
        }

        {
            const std::string attackTypeStr =
                    entry.value("primaryAttackType", std::string("none"));

            if (!ParseAttackTypeString(attackTypeStr, cfg.primaryAttackType)) {
                TraceLog(LOG_WARNING,
                         "Player weapon config '%s': invalid primaryAttackType '%s', defaulting to none",
                         cfg.equipmentSetId.c_str(),
                         attackTypeStr.c_str());
                cfg.primaryAttackType = TopdownAttackType::None;
            }
        }

        {
            const std::string attackTypeStr =
                    entry.value("secondaryAttackType", std::string("none"));

            if (!ParseAttackTypeString(attackTypeStr, cfg.secondaryAttackType)) {
                TraceLog(LOG_WARNING,
                         "Player weapon config '%s': invalid secondaryAttackType '%s', defaulting to none",
                         cfg.equipmentSetId.c_str(),
                         attackTypeStr.c_str());
                cfg.secondaryAttackType = TopdownAttackType::None;
            }
        }

        cfg.primaryCooldownMs = entry.value("primaryCooldownMs", 0.0f);
        cfg.secondaryCooldownMs = entry.value("secondaryCooldownMs", 0.0f);

        cfg.rangedDamage = entry.value("rangedDamage", 0.0f);
        cfg.meleeDamage = entry.value("meleeDamage", 0.0f);

        if (cfg.rangedDamage < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': rangedDamage < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.rangedDamage = 0.0f;
        }

        if (cfg.meleeDamage < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': meleeDamage < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.meleeDamage = 0.0f;
        }

        cfg.maxRange = entry.value("maxRange", 0.0f);

        cfg.pelletCount = entry.value("pelletCount", 1);
        if (cfg.pelletCount < 1) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': pelletCount < 1, clamping to 1",
                     cfg.equipmentSetId.c_str());
            cfg.pelletCount = 1;
        }

        cfg.spreadDegrees = entry.value("spreadDegrees", 0.0f);
        if (cfg.spreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': spreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.spreadDegrees = 0.0f;
        }

        cfg.meleeRange = entry.value("meleeRange", 0.0f);
        if (cfg.meleeRange < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': meleeRange < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.meleeRange = 0.0f;
        }

        cfg.meleeArcDegrees = entry.value("meleeArcDegrees", 0.0f);
        if (cfg.meleeArcDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': meleeArcDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.meleeArcDegrees = 0.0f;
        }

        cfg.rangedKnockback = entry.value("rangedKnockback", 0.0f);
        if (cfg.rangedKnockback < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': rangedKnockback < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.rangedKnockback = 0.0f;
        }

        cfg.meleeKnockback = entry.value("meleeKnockback", 0.0f);
        if (cfg.meleeKnockback < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': meleeKnockback < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.meleeKnockback = 0.0f;
        }

        if (entry.contains("muzzleOriginX") && entry["muzzleOriginX"].is_number()) {
            cfg.muzzleOrigin.x = entry["muzzleOriginX"].get<float>();
        }

        if (entry.contains("muzzleOriginY") && entry["muzzleOriginY"].is_number()) {
            cfg.muzzleOrigin.y = entry["muzzleOriginY"].get<float>();
        }

        {
            const std::string tracerStyleStr =
                    entry.value("tracerStyle", std::string("none"));

            if (!ParseTracerStyleString(tracerStyleStr, cfg.tracerStyle)) {
                TraceLog(LOG_WARNING,
                         "Player weapon config '%s': invalid tracerStyle '%s', defaulting to none",
                         cfg.equipmentSetId.c_str(),
                         tracerStyleStr.c_str());
                cfg.tracerStyle = TopdownTracerStyle::None;
            }
        }

        // walls
        cfg.wallImpactParticleCount = entry.value("wallImpactParticleCount", cfg.wallImpactParticleCount);
        if (cfg.wallImpactParticleCount < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleCount < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleCount = 0;
        }

        cfg.wallImpactParticleSpeedMin = entry.value("wallImpactParticleSpeedMin", cfg.wallImpactParticleSpeedMin);
        cfg.wallImpactParticleSpeedMax = entry.value("wallImpactParticleSpeedMax", cfg.wallImpactParticleSpeedMax);

        if (cfg.wallImpactParticleSpeedMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSpeedMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleSpeedMin = 0.0f;
        }

        if (cfg.wallImpactParticleSpeedMax < cfg.wallImpactParticleSpeedMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSpeedMax < wallImpactParticleSpeedMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleSpeedMax = cfg.wallImpactParticleSpeedMin;
        }

        cfg.wallImpactParticleLifetimeMsMin =
                entry.value("wallImpactParticleLifetimeMsMin", cfg.wallImpactParticleLifetimeMsMin);
        cfg.wallImpactParticleLifetimeMsMax =
                entry.value("wallImpactParticleLifetimeMsMax", cfg.wallImpactParticleLifetimeMsMax);

        if (cfg.wallImpactParticleLifetimeMsMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleLifetimeMsMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleLifetimeMsMin = 0.0f;
        }

        if (cfg.wallImpactParticleLifetimeMsMax < cfg.wallImpactParticleLifetimeMsMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleLifetimeMsMax < wallImpactParticleLifetimeMsMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleLifetimeMsMax = cfg.wallImpactParticleLifetimeMsMin;
        }

        cfg.wallImpactParticleSizeMin =
                entry.value("wallImpactParticleSizeMin", cfg.wallImpactParticleSizeMin);
        cfg.wallImpactParticleSizeMax =
                entry.value("wallImpactParticleSizeMax", cfg.wallImpactParticleSizeMax);

        if (cfg.wallImpactParticleSizeMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSizeMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleSizeMin = 0.0f;
        }

        if (cfg.wallImpactParticleSizeMax < cfg.wallImpactParticleSizeMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSizeMax < wallImpactParticleSizeMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactParticleSizeMax = cfg.wallImpactParticleSizeMin;
        }

        cfg.wallImpactSpreadDegrees =
                entry.value("wallImpactSpreadDegrees", cfg.wallImpactSpreadDegrees);

        if (cfg.wallImpactSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.wallImpactSpreadDegrees = 0.0f;
        }

        cfg.muzzleFlashLifetimeMs =
                entry.value("muzzleFlashLifetimeMs", cfg.muzzleFlashLifetimeMs);
        if (cfg.muzzleFlashLifetimeMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleFlashLifetimeMs < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleFlashLifetimeMs = 0.0f;
        }

        cfg.muzzleFlashForwardLength =
                entry.value("muzzleFlashForwardLength", cfg.muzzleFlashForwardLength);
        if (cfg.muzzleFlashForwardLength < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleFlashForwardLength < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleFlashForwardLength = 0.0f;
        }

        cfg.muzzleFlashSideWidth =
                entry.value("muzzleFlashSideWidth", cfg.muzzleFlashSideWidth);
        if (cfg.muzzleFlashSideWidth < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleFlashSideWidth < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleFlashSideWidth = 0.0f;
        }

        cfg.muzzleSmokeParticleCount =
                entry.value("muzzleSmokeParticleCount", cfg.muzzleSmokeParticleCount);
        if (cfg.muzzleSmokeParticleCount < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeParticleCount < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeParticleCount = 0;
        }

        cfg.muzzleSmokeSpeedMin =
                entry.value("muzzleSmokeSpeedMin", cfg.muzzleSmokeSpeedMin);
        cfg.muzzleSmokeSpeedMax =
                entry.value("muzzleSmokeSpeedMax", cfg.muzzleSmokeSpeedMax);

        if (cfg.muzzleSmokeSpeedMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSpeedMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeSpeedMin = 0.0f;
        }

        if (cfg.muzzleSmokeSpeedMax < cfg.muzzleSmokeSpeedMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSpeedMax < muzzleSmokeSpeedMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeSpeedMax = cfg.muzzleSmokeSpeedMin;
        }

        cfg.muzzleSmokeLifetimeMsMin =
                entry.value("muzzleSmokeLifetimeMsMin", cfg.muzzleSmokeLifetimeMsMin);
        cfg.muzzleSmokeLifetimeMsMax =
                entry.value("muzzleSmokeLifetimeMsMax", cfg.muzzleSmokeLifetimeMsMax);

        if (cfg.muzzleSmokeLifetimeMsMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeLifetimeMsMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeLifetimeMsMin = 0.0f;
        }

        if (cfg.muzzleSmokeLifetimeMsMax < cfg.muzzleSmokeLifetimeMsMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeLifetimeMsMax < muzzleSmokeLifetimeMsMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeLifetimeMsMax = cfg.muzzleSmokeLifetimeMsMin;
        }

        cfg.muzzleSmokeSizeMin =
                entry.value("muzzleSmokeSizeMin", cfg.muzzleSmokeSizeMin);
        cfg.muzzleSmokeSizeMax =
                entry.value("muzzleSmokeSizeMax", cfg.muzzleSmokeSizeMax);

        if (cfg.muzzleSmokeSizeMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSizeMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeSizeMin = 0.0f;
        }

        if (cfg.muzzleSmokeSizeMax < cfg.muzzleSmokeSizeMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSizeMax < muzzleSmokeSizeMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeSizeMax = cfg.muzzleSmokeSizeMin;
        }

        cfg.muzzleSmokeSpreadDegrees =
                entry.value("muzzleSmokeSpreadDegrees", cfg.muzzleSmokeSpreadDegrees);
        if (cfg.muzzleSmokeSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeSpreadDegrees = 0.0f;
        }

        cfg.muzzleSmokeForwardBias =
                entry.value("muzzleSmokeForwardBias", cfg.muzzleSmokeForwardBias);
        if (cfg.muzzleSmokeForwardBias < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeForwardBias < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeForwardBias = 0.0f;
        } else if (cfg.muzzleSmokeForwardBias > 1.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeForwardBias > 1, clamping to 1",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleSmokeForwardBias = 1.0f;
        }


        // blood
        cfg.bloodImpactParticleCount =
                entry.value("bloodImpactParticleCount", cfg.bloodImpactParticleCount);
        if (cfg.bloodImpactParticleCount < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleCount < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleCount = 0;
        }

        cfg.bloodImpactParticleSpeedMin =
                entry.value("bloodImpactParticleSpeedMin", cfg.bloodImpactParticleSpeedMin);
        cfg.bloodImpactParticleSpeedMax =
                entry.value("bloodImpactParticleSpeedMax", cfg.bloodImpactParticleSpeedMax);

        if (cfg.bloodImpactParticleSpeedMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSpeedMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleSpeedMin = 0.0f;
        }
        if (cfg.bloodImpactParticleSpeedMax < cfg.bloodImpactParticleSpeedMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSpeedMax < bloodImpactParticleSpeedMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleSpeedMax = cfg.bloodImpactParticleSpeedMin;
        }

        cfg.bloodImpactParticleLifetimeMsMin =
                entry.value("bloodImpactParticleLifetimeMsMin", cfg.bloodImpactParticleLifetimeMsMin);
        cfg.bloodImpactParticleLifetimeMsMax =
                entry.value("bloodImpactParticleLifetimeMsMax", cfg.bloodImpactParticleLifetimeMsMax);

        if (cfg.bloodImpactParticleLifetimeMsMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleLifetimeMsMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleLifetimeMsMin = 0.0f;
        }
        if (cfg.bloodImpactParticleLifetimeMsMax < cfg.bloodImpactParticleLifetimeMsMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleLifetimeMsMax < bloodImpactParticleLifetimeMsMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleLifetimeMsMax = cfg.bloodImpactParticleLifetimeMsMin;
        }

        cfg.bloodImpactParticleSizeMin =
                entry.value("bloodImpactParticleSizeMin", cfg.bloodImpactParticleSizeMin);
        cfg.bloodImpactParticleSizeMax =
                entry.value("bloodImpactParticleSizeMax", cfg.bloodImpactParticleSizeMax);

        if (cfg.bloodImpactParticleSizeMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSizeMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleSizeMin = 0.0f;
        }
        if (cfg.bloodImpactParticleSizeMax < cfg.bloodImpactParticleSizeMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSizeMax < bloodImpactParticleSizeMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactParticleSizeMax = cfg.bloodImpactParticleSizeMin;
        }

        cfg.bloodImpactSpreadDegrees =
                entry.value("bloodImpactSpreadDegrees", cfg.bloodImpactSpreadDegrees);
        if (cfg.bloodImpactSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodImpactSpreadDegrees = 0.0f;
        }

        cfg.bloodDecalCountMin =
                entry.value("bloodDecalCountMin", cfg.bloodDecalCountMin);
        cfg.bloodDecalCountMax =
                entry.value("bloodDecalCountMax", cfg.bloodDecalCountMax);

        if (cfg.bloodDecalCountMin < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalCountMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalCountMin = 0;
        }
        if (cfg.bloodDecalCountMax < cfg.bloodDecalCountMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalCountMax < bloodDecalCountMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalCountMax = cfg.bloodDecalCountMin;
        }

        cfg.bloodDecalDistanceMin =
                entry.value("bloodDecalDistanceMin", cfg.bloodDecalDistanceMin);
        cfg.bloodDecalDistanceMax =
                entry.value("bloodDecalDistanceMax", cfg.bloodDecalDistanceMax);

        if (cfg.bloodDecalDistanceMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalDistanceMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalDistanceMin = 0.0f;
        }
        if (cfg.bloodDecalDistanceMax < cfg.bloodDecalDistanceMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalDistanceMax < bloodDecalDistanceMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalDistanceMax = cfg.bloodDecalDistanceMin;
        }

        cfg.bloodDecalRadiusMin =
                entry.value("bloodDecalRadiusMin", cfg.bloodDecalRadiusMin);
        cfg.bloodDecalRadiusMax =
                entry.value("bloodDecalRadiusMax", cfg.bloodDecalRadiusMax);

        if (cfg.bloodDecalRadiusMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalRadiusMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalRadiusMin = 0.0f;
        }
        if (cfg.bloodDecalRadiusMax < cfg.bloodDecalRadiusMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalRadiusMax < bloodDecalRadiusMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalRadiusMax = cfg.bloodDecalRadiusMin;
        }

        cfg.bloodDecalSpreadDegrees =
                entry.value("bloodDecalSpreadDegrees", cfg.bloodDecalSpreadDegrees);
        if (cfg.bloodDecalSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalSpreadDegrees = 0.0f;
        }

        cfg.bloodDecalWallPadding =
                entry.value("bloodDecalWallPadding", cfg.bloodDecalWallPadding);
        if (cfg.bloodDecalWallPadding < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalWallPadding < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalWallPadding = 0.0f;
        }

        cfg.bloodDecalOpacityMin =
                entry.value("bloodDecalOpacityMin", cfg.bloodDecalOpacityMin);
        cfg.bloodDecalOpacityMax =
                entry.value("bloodDecalOpacityMax", cfg.bloodDecalOpacityMax);

        cfg.bloodDecalOpacityMin = Clamp(cfg.bloodDecalOpacityMin, 0.0f, 1.0f);
        cfg.bloodDecalOpacityMax = Clamp(cfg.bloodDecalOpacityMax, 0.0f, 1.0f);

        if (cfg.bloodDecalOpacityMax < cfg.bloodDecalOpacityMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalOpacityMax < bloodDecalOpacityMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.bloodDecalOpacityMax = cfg.bloodDecalOpacityMin;
        }


        {
            auto modesIt = entry.find("supportedFireModes");
            if (modesIt != entry.end() && modesIt->is_array()) {
                for (const auto& modeEntry : *modesIt) {
                    if (!modeEntry.is_string()) {
                        TraceLog(LOG_WARNING,
                                 "Player weapon config '%s': skipping non-string fire mode entry",
                                 cfg.equipmentSetId.c_str());
                        continue;
                    }

                    TopdownFireMode mode{};
                    const std::string modeStr = modeEntry.get<std::string>();

                    if (!ParseFireModeString(modeStr, mode)) {
                        TraceLog(LOG_WARNING,
                                 "Player weapon config '%s': invalid supported fire mode '%s'",
                                 cfg.equipmentSetId.c_str(),
                                 modeStr.c_str());
                        continue;
                    }

                    bool alreadyPresent = false;
                    for (TopdownFireMode existing : cfg.supportedFireModes) {
                        if (existing == mode) {
                            alreadyPresent = true;
                            break;
                        }
                    }

                    if (!alreadyPresent) {
                        cfg.supportedFireModes.push_back(mode);
                    }
                }
            }
        }

        if (cfg.supportedFireModes.empty()) {
            cfg.supportedFireModes.push_back(TopdownFireMode::SemiAuto);
        }

        {
            const std::string defaultModeStr =
                    entry.value("defaultFireMode", std::string("semi"));

            TopdownFireMode parsedDefault{};
            if (!ParseFireModeString(defaultModeStr, parsedDefault)) {
                TraceLog(LOG_WARNING,
                         "Player weapon config '%s': invalid defaultFireMode '%s', using first supported mode",
                         cfg.equipmentSetId.c_str(),
                         defaultModeStr.c_str());
                cfg.defaultFireMode = cfg.supportedFireModes.front();
            } else {
                bool supported = false;
                for (TopdownFireMode mode : cfg.supportedFireModes) {
                    if (mode == parsedDefault) {
                        supported = true;
                        break;
                    }
                }

                if (!supported) {
                    TraceLog(LOG_WARNING,
                             "Player weapon config '%s': defaultFireMode not in supportedFireModes, using first supported mode",
                             cfg.equipmentSetId.c_str());
                    cfg.defaultFireMode = cfg.supportedFireModes.front();
                } else {
                    cfg.defaultFireMode = parsedDefault;
                }
            }
        }

        cfg.burstCount = entry.value("burstCount", 3);
        if (cfg.burstCount < 1) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': burstCount < 1, clamping to 1",
                     cfg.equipmentSetId.c_str());
            cfg.burstCount = 1;
        }

        cfg.burstIntervalMs = entry.value("burstIntervalMs", 70.0f);
        if (cfg.burstIntervalMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': burstIntervalMs < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.burstIntervalMs = 0.0f;
        }

        asset.weaponConfigs.push_back(cfg);

        TraceLog(LOG_INFO,
                 "Loaded player weapon config '%s' slot=%d primary=%d secondary=%d fireModes=%d",
                 cfg.equipmentSetId.c_str(),
                 cfg.slot,
                 static_cast<int>(cfg.primaryAttackType),
                 static_cast<int>(cfg.secondaryAttackType),
                 static_cast<int>(cfg.supportedFireModes.size()));
    }
}

static SpriteAssetHandle LoadPlayerAnimationAsset(
        ResourceData& resources,
        const char* asepriteJsonPath,
        float baseDrawScale,
        const PlayerAnimationOriginOverride& originOverride)
{
    if (originOverride.hasOrigin) {
        return LoadSpriteAssetFromAsepriteJsonWithOrigin(
                resources,
                asepriteJsonPath,
                baseDrawScale,
                originOverride.origin,
                ResourceScope::Global);
    }

    return LoadSpriteAssetFromAsepriteJson(
            resources,
            asepriteJsonPath,
            baseDrawScale,
            ResourceScope::Global);
}

static bool HasJsonExtension(const fs::path& path)
{
    return path.has_extension() && path.extension() == ".json";
}

static bool IsPlayerConfigFile(const fs::path& path)
{
    return path.filename() == "player.json";
}

static std::string BuildAnimationIdFromPath(const fs::path& path)
{
    return path.stem().string();
}

static bool SortPathsByFilename(const fs::path& a, const fs::path& b)
{
    return a.filename().string() < b.filename().string();
}

SpriteAssetHandle FindTopdownPlayerAnimationHandle(
        const GameState& state,
        const std::string& animationId)
{
    for (const TopdownPlayerAnimationEntry& entry : state.topdown.playerCharacterAsset.animations) {
        if (entry.id == animationId) {
            return entry.spriteHandle;
        }
    }

    return -1;
}

bool HasTopdownPlayerAnimation(
        const GameState& state,
        const std::string& animationId)
{
    return FindTopdownPlayerAnimationHandle(state, animationId) >= 0;
}

SpriteAssetHandle FindTopdownPlayerFeetAnimationHandle(
        const GameState& state,
        const std::string& suffix)
{
    if (suffix.empty()) {
        return -1;
    }

    return FindTopdownPlayerAnimationHandle(
            state,
            std::string("feet_") + suffix);
}

SpriteAssetHandle FindTopdownPlayerEquipmentAnimationHandle(
        const GameState& state,
        const std::string& equipmentSetId,
        const std::string& suffix)
{
    if (equipmentSetId.empty() || suffix.empty()) {
        return -1;
    }

    return FindTopdownPlayerAnimationHandle(
            state,
            equipmentSetId + "_" + suffix);
}

bool HasTopdownPlayerEquipmentAnimationSet(
        const GameState& state,
        const std::string& equipmentSetId)
{
    if (equipmentSetId.empty()) {
        return false;
    }

    for (const TopdownPlayerAnimationEntry& entry : state.topdown.playerCharacterAsset.animations) {
        const std::string prefix = equipmentSetId + "_";
        if (entry.id.rfind(prefix, 0) == 0) {
            return true;
        }
    }

    return false;
}

const TopdownPlayerAnimationEntry* FindTopdownPlayerAnimationEntry(
        const GameState& state,
        const std::string& animationId)
{
    if (animationId.empty()) {
        return nullptr;
    }

    for (const TopdownPlayerAnimationEntry& entry : state.topdown.playerCharacterAsset.animations) {
        if (entry.id == animationId) {
            return &entry;
        }
    }

    return nullptr;
}

static std::string FindTopdownPlayerEquipmentAnimationIdAny(
        const GameState& state,
        const std::string& equipmentSetId,
        const std::vector<std::string>& suffixes)
{
    if (equipmentSetId.empty()) {
        return {};
    }

    for (const std::string& suffix : suffixes) {
        const std::string animationId = equipmentSetId + "_" + suffix;
        if (HasTopdownPlayerAnimation(state, animationId)) {
            return animationId;
        }
    }

    return {};
}

std::string FindTopdownPlayerEquipmentAttackAnimationId(
        const GameState& state,
        const std::string& equipmentSetId,
        TopdownAttackType attackType)
{
    switch (attackType) {
        case TopdownAttackType::Ranged:
            return FindTopdownPlayerEquipmentAnimationIdAny(
                    state,
                    equipmentSetId,
                    { "rangedattack", "RangedAttack", "shoot", "Shoot" });

        case TopdownAttackType::Melee:
            return FindTopdownPlayerEquipmentAnimationIdAny(
                    state,
                    equipmentSetId,
                    { "meleeattack", "MeleeAttack" });

        case TopdownAttackType::None:
        default:
            return {};
    }
}

static SpriteAssetHandle FindTopdownPlayerEquipmentAnimationHandleAny(
        const GameState& state,
        const std::string& equipmentSetId,
        const std::vector<std::string>& suffixes)
{
    for (const std::string& suffix : suffixes) {
        const SpriteAssetHandle handle =
                FindTopdownPlayerEquipmentAnimationHandle(state, equipmentSetId, suffix);
        if (handle >= 0) {
            return handle;
        }
    }

    return -1;
}

SpriteAssetHandle FindTopdownPlayerEquipmentAttackAnimationHandle(
        const GameState& state,
        const std::string& equipmentSetId,
        TopdownAttackType attackType)
{
    if (equipmentSetId.empty()) {
        return -1;
    }

    switch (attackType) {
        case TopdownAttackType::Ranged:
            return FindTopdownPlayerEquipmentAnimationHandleAny(
                    state,
                    equipmentSetId,
                    { "rangedattack", "RangedAttack", "shoot", "Shoot" });

        case TopdownAttackType::Melee:
            return FindTopdownPlayerEquipmentAnimationHandleAny(
                    state,
                    equipmentSetId,
                    { "meleeattack", "MeleeAttack" });

        case TopdownAttackType::None:
        default:
            return -1;
    }
}

const TopdownPlayerWeaponConfig* FindTopdownPlayerWeaponConfigByEquipmentSetId(
        const GameState& state,
        const std::string& equipmentSetId)
{
    if (equipmentSetId.empty()) {
        return nullptr;
    }

    for (const TopdownPlayerWeaponConfig& cfg : state.topdown.playerCharacterAsset.weaponConfigs) {
        if (cfg.equipmentSetId == equipmentSetId) {
            return &cfg;
        }
    }

    return nullptr;
}

const TopdownPlayerWeaponConfig* FindTopdownPlayerWeaponConfigBySlot(
        const GameState& state,
        int slot)
{
    if (slot <= 0) {
        return nullptr;
    }

    for (const TopdownPlayerWeaponConfig& cfg : state.topdown.playerCharacterAsset.weaponConfigs) {
        if (cfg.slot == slot) {
            return &cfg;
        }
    }

    return nullptr;
}

bool LoadTopdownPlayerCharacterAssets(GameState& state)
{
    TopdownCharacterAssetData& asset = state.topdown.playerCharacterAsset;

    if (asset.loaded) {
        return true;
    }

    asset = {};
    asset.id = "player";

    static constexpr float kCharacterBaseDrawScale = 0.5f;

    const fs::path playerDir = fs::path(ASSETS_PATH "characters/player");
    if (!fs::exists(playerDir) || !fs::is_directory(playerDir)) {
        TraceLog(LOG_ERROR,
                 "Topdown player directory missing: %s",
                 playerDir.string().c_str());
        return false;
    }

    json playerConfig;
    bool hasPlayerConfig = false;

    {
        const fs::path playerConfigPath = playerDir / "player.json";
        std::ifstream in(playerConfigPath);
        if (in.is_open()) {
            in >> playerConfig;
            hasPlayerConfig = true;
        }
    }

    if (hasPlayerConfig) {
        ReadPlayerWeaponConfigs(playerConfig, asset);
    }

    if (hasPlayerConfig) {
        asset.maxHealth = playerConfig.value("maxHealth", asset.maxHealth);
        asset.hurtCooldownMs = playerConfig.value("hurtCooldownMs", asset.hurtCooldownMs);
        asset.meleeHitSlowdownMs = playerConfig.value("meleeHitSlowdownMs", asset.meleeHitSlowdownMs);
        asset.meleeHitSlowdownMultiplier =
                playerConfig.value("meleeHitSlowdownMultiplier", asset.meleeHitSlowdownMultiplier);

        if (asset.maxHealth <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "Topdown player config: maxHealth <= 0, clamping to 1");
            asset.maxHealth = 1.0f;
        }

        if (asset.hurtCooldownMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Topdown player config: hurtCooldownMs < 0, clamping to 0");
            asset.hurtCooldownMs = 0.0f;
        }

        if (asset.meleeHitSlowdownMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Topdown player config: meleeHitSlowdownMs < 0, clamping to 0");
            asset.meleeHitSlowdownMs = 0.0f;
        }

        asset.meleeHitSlowdownMultiplier =
                Clamp(asset.meleeHitSlowdownMultiplier, 0.0f, 1.0f);
    }

    std::vector<fs::path> animationFiles;
    for (const auto& entry : fs::directory_iterator(playerDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path path = entry.path();
        if (!HasJsonExtension(path)) {
            continue;
        }

        if (IsPlayerConfigFile(path)) {
            continue;
        }

        animationFiles.push_back(path);
    }

    std::sort(animationFiles.begin(), animationFiles.end(), SortPathsByFilename);

    bool ok = true;

    for (const fs::path& path : animationFiles) {
        const std::string animationId = BuildAnimationIdFromPath(path);
        const PlayerAnimationOriginOverride originOverride =
                hasPlayerConfig
                ? ReadPlayerAnimationOriginOverride(playerConfig, animationId)
                : PlayerAnimationOriginOverride{};

        const PlayerAnimationMuzzleOverride muzzleOverride =
                hasPlayerConfig
                ? ReadPlayerAnimationMuzzleOverride(playerConfig, animationId)
                : PlayerAnimationMuzzleOverride{};

        const SpriteAssetHandle handle = LoadPlayerAnimationAsset(
                state.resources,
                NormalizePath(path).c_str(),
                kCharacterBaseDrawScale,
                originOverride);

        if (handle < 0) {
            TraceLog(LOG_ERROR,
                     "Failed loading topdown player animation '%s' from %s",
                     animationId.c_str(),
                     path.string().c_str());
            ok = false;
            continue;
        }

        TopdownPlayerAnimationEntry entry;
        entry.id = animationId;
        entry.spriteHandle = handle;
        entry.hasMuzzle = muzzleOverride.hasMuzzle;
        entry.muzzle = muzzleOverride.muzzle;
        asset.animations.push_back(entry);

        TraceLog(LOG_INFO,
                 "Loaded topdown player animation '%s' from %s",
                 animationId.c_str(),
                 path.string().c_str());
    }

    asset.loaded = ok && !asset.animations.empty();
    return asset.loaded;
}

void InitializeTopdownPlayerCharacterRuntime(GameState& state)
{
    TopdownCharacterRuntime& runtime = state.topdown.runtime.playerCharacter;
    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterAssetData& asset = state.topdown.playerCharacterAsset;

    runtime = {};
    runtime.active = asset.loaded;

    runtime.equippedSetId = "handgun";

    runtime.bodyFacingRadians = 0.0f;
    runtime.desiredAimRadians = 0.0f;
    runtime.feetRotationRadians = 0.0f;
    runtime.upperRotationRadians = 0.0f;

    runtime.turnSpeedRadians = 7.0f;
    runtime.maxUpperBodyTwistRadians = 85.0f * DEG2RAD;
    runtime.aimFrozen = false;

    runtime.feetAnimationTimeMs = 0.0f;
    runtime.upperAnimationTimeMs = 0.0f;

    runtime.currentFeetHandle = FindTopdownPlayerFeetAnimationHandle(state, "idle");
    runtime.currentUpperHandle = FindTopdownPlayerEquipmentAnimationHandle(state, runtime.equippedSetId, "idle");

    player.maxHealth = asset.maxHealth;
    player.health = asset.maxHealth;
    player.hurtCooldownRemainingMs = 0.0f;
    player.hitSlowdownRemainingMs = 0.0f;
    player.hitSlowdownMultiplier = 1.0f;
    player.damageFlashRemainingMs = 0.0f;
    player.lowHealthEffectWeight = 0.0f;
    player.lifeState = TopdownPlayerLifeState::Alive;

    const TopdownPlayerWeaponConfig* weaponConfig =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, runtime.equippedSetId);

    state.topdown.runtime.playerAttack = {};
    if (weaponConfig != nullptr) {
        state.topdown.runtime.playerAttack.equipmentSetId = weaponConfig->equipmentSetId;
        state.topdown.runtime.playerAttack.currentFireMode = weaponConfig->defaultFireMode;
    } else {
        state.topdown.runtime.playerAttack.equipmentSetId = runtime.equippedSetId;
        state.topdown.runtime.playerAttack.currentFireMode = TopdownFireMode::SemiAuto;
    }
}
