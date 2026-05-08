#include "topdown/PlayerRegistry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#include "resources/AsepriteAsset.h"
#include "audio/Audio.h"
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

        cfg.noiseRadius = entry.value("noiseRadius", cfg.noiseRadius);
        if (cfg.noiseRadius < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': noiseRadius < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.noiseRadius = 0.0f;
        }

        cfg.ammoType = entry.value("ammoType", std::string());
        cfg.magazineSize = std::max(0, entry.value("magazineSize", 0));
        cfg.ammoPerShot = std::max(0, entry.value("ammoPerShot", 0));
        cfg.reloadDurationMs = std::max(0.0f, entry.value("reloadDurationMs", 0.0f));

        if (cfg.ammoType.empty()) {
            cfg.magazineSize = 0;
            cfg.ammoPerShot = 0;
            cfg.reloadDurationMs = 0.0f;
        } else {
            if (cfg.magazineSize <= 0) {
                TraceLog(LOG_WARNING,
                         "Player weapon config '%s': ammoType '%s' has no magazineSize",
                         cfg.equipmentSetId.c_str(),
                         cfg.ammoType.c_str());
            }

            if (cfg.ammoPerShot <= 0) {
                TraceLog(LOG_WARNING,
                         "Player weapon config '%s': ammoType '%s' has invalid ammoPerShot, defaulting to 1",
                         cfg.equipmentSetId.c_str(),
                         cfg.ammoType.c_str());
                cfg.ammoPerShot = 1;
            }
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

        float defaultRangedDoorImpulse = 0.0f;
        float defaultMeleeDoorImpulse = 0.0f;

        // Sensible defaults so existing json keeps working immediately.
        // NOTE:
        // shotgun/rifle ranged values are per pellet / per bullet hit,
        // so they are intentionally lower than the pistol value.
        if (cfg.equipmentSetId == "handgun") {
            defaultRangedDoorImpulse = 420.0f;
            defaultMeleeDoorImpulse = 0.0f;
        } else if (cfg.equipmentSetId == "shotgun") {
            defaultRangedDoorImpulse = 150.0f;
            defaultMeleeDoorImpulse = 900.0f;
        } else if (cfg.equipmentSetId == "rifle") {
            defaultRangedDoorImpulse = 115.0f;
            defaultMeleeDoorImpulse = 760.0f;
        } else if (cfg.equipmentSetId == "knife") {
            defaultRangedDoorImpulse = 0.0f;
            defaultMeleeDoorImpulse = 0.0f;
        }

        cfg.rangedDoorImpulse =
                entry.value("rangedDoorImpulse", defaultRangedDoorImpulse);
        cfg.meleeDoorImpulse =
                entry.value("meleeDoorImpulse", defaultMeleeDoorImpulse);

        if (cfg.rangedDoorImpulse < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': rangedDoorImpulse < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.rangedDoorImpulse = 0.0f;
        }

        if (cfg.meleeDoorImpulse < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': meleeDoorImpulse < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.meleeDoorImpulse = 0.0f;
        }

        if (entry.contains("muzzleOriginX") && entry["muzzleOriginX"].is_number()) {
            cfg.muzzleEffects.muzzleX = entry["muzzleOriginX"].get<float>();
        }

        if (entry.contains("muzzleOriginY") && entry["muzzleOriginY"].is_number()) {
            cfg.muzzleEffects.muzzleY = entry["muzzleOriginY"].get<float>();
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
        cfg.ballisticImpactEffects.wallImpactParticleCount = entry.value("wallImpactParticleCount", cfg.ballisticImpactEffects.wallImpactParticleCount);
        if (cfg.ballisticImpactEffects.wallImpactParticleCount < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleCount < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleCount = 0;
        }

        cfg.ballisticImpactEffects.wallImpactParticleSpeedMin = entry.value("wallImpactParticleSpeedMin", cfg.ballisticImpactEffects.wallImpactParticleSpeedMin);
        cfg.ballisticImpactEffects.wallImpactParticleSpeedMax = entry.value("wallImpactParticleSpeedMax", cfg.ballisticImpactEffects.wallImpactParticleSpeedMax);

        if (cfg.ballisticImpactEffects.wallImpactParticleSpeedMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSpeedMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleSpeedMin = 0.0f;
        }

        if (cfg.ballisticImpactEffects.wallImpactParticleSpeedMax < cfg.ballisticImpactEffects.wallImpactParticleSpeedMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSpeedMax < wallImpactParticleSpeedMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleSpeedMax = cfg.ballisticImpactEffects.wallImpactParticleSpeedMin;
        }

        cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMin =
                entry.value("wallImpactParticleLifetimeMsMin", cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMin);
        cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMax =
                entry.value("wallImpactParticleLifetimeMsMax", cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMax);

        if (cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleLifetimeMsMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMin = 0.0f;
        }

        if (cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMax < cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleLifetimeMsMax < wallImpactParticleLifetimeMsMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMax = cfg.ballisticImpactEffects.wallImpactParticleLifetimeMsMin;
        }

        cfg.ballisticImpactEffects.wallImpactParticleSizeMin =
                entry.value("wallImpactParticleSizeMin", cfg.ballisticImpactEffects.wallImpactParticleSizeMin);
        cfg.ballisticImpactEffects.wallImpactParticleSizeMax =
                entry.value("wallImpactParticleSizeMax", cfg.ballisticImpactEffects.wallImpactParticleSizeMax);

        if (cfg.ballisticImpactEffects.wallImpactParticleSizeMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSizeMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleSizeMin = 0.0f;
        }

        if (cfg.ballisticImpactEffects.wallImpactParticleSizeMax < cfg.ballisticImpactEffects.wallImpactParticleSizeMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactParticleSizeMax < wallImpactParticleSizeMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactParticleSizeMax = cfg.ballisticImpactEffects.wallImpactParticleSizeMin;
        }

        cfg.ballisticImpactEffects.wallImpactSpreadDegrees =
                entry.value("wallImpactSpreadDegrees", cfg.ballisticImpactEffects.wallImpactSpreadDegrees);

        if (cfg.ballisticImpactEffects.wallImpactSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': wallImpactSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.ballisticImpactEffects.wallImpactSpreadDegrees = 0.0f;
        }

        cfg.muzzleEffects.muzzleFlashLifetimeMs =
                entry.value("muzzleFlashLifetimeMs", cfg.muzzleEffects.muzzleFlashLifetimeMs);
        if (cfg.muzzleEffects.muzzleFlashLifetimeMs < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleFlashLifetimeMs < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleFlashLifetimeMs = 0.0f;
        }

        cfg.muzzleEffects.muzzleFlashForwardLength =
                entry.value("muzzleFlashForwardLength", cfg.muzzleEffects.muzzleFlashForwardLength);
        if (cfg.muzzleEffects.muzzleFlashForwardLength < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleFlashForwardLength < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleFlashForwardLength = 0.0f;
        }

        cfg.muzzleEffects.muzzleFlashSideWidth =
                entry.value("muzzleFlashSideWidth", cfg.muzzleEffects.muzzleFlashSideWidth);
        if (cfg.muzzleEffects.muzzleFlashSideWidth < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleFlashSideWidth < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleFlashSideWidth = 0.0f;
        }

        cfg.muzzleEffects.muzzleSmokeParticleCount =
                entry.value("muzzleSmokeParticleCount", cfg.muzzleEffects.muzzleSmokeParticleCount);
        if (cfg.muzzleEffects.muzzleSmokeParticleCount < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeParticleCount < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeParticleCount = 0;
        }

        cfg.muzzleEffects.muzzleSmokeSpeedMin =
                entry.value("muzzleSmokeSpeedMin", cfg.muzzleEffects.muzzleSmokeSpeedMin);
        cfg.muzzleEffects.muzzleSmokeSpeedMax =
                entry.value("muzzleSmokeSpeedMax", cfg.muzzleEffects.muzzleSmokeSpeedMax);

        if (cfg.muzzleEffects.muzzleSmokeSpeedMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSpeedMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeSpeedMin = 0.0f;
        }

        if (cfg.muzzleEffects.muzzleSmokeSpeedMax < cfg.muzzleEffects.muzzleSmokeSpeedMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSpeedMax < muzzleSmokeSpeedMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeSpeedMax = cfg.muzzleEffects.muzzleSmokeSpeedMin;
        }

        cfg.muzzleEffects.muzzleSmokeLifetimeMsMin =
                entry.value("muzzleSmokeLifetimeMsMin", cfg.muzzleEffects.muzzleSmokeLifetimeMsMin);
        cfg.muzzleEffects.muzzleSmokeLifetimeMsMax =
                entry.value("muzzleSmokeLifetimeMsMax", cfg.muzzleEffects.muzzleSmokeLifetimeMsMax);

        if (cfg.muzzleEffects.muzzleSmokeLifetimeMsMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeLifetimeMsMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeLifetimeMsMin = 0.0f;
        }

        if (cfg.muzzleEffects.muzzleSmokeLifetimeMsMax < cfg.muzzleEffects.muzzleSmokeLifetimeMsMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeLifetimeMsMax < muzzleSmokeLifetimeMsMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeLifetimeMsMax = cfg.muzzleEffects.muzzleSmokeLifetimeMsMin;
        }

        cfg.muzzleEffects.muzzleSmokeSizeMin =
                entry.value("muzzleSmokeSizeMin", cfg.muzzleEffects.muzzleSmokeSizeMin);
        cfg.muzzleEffects.muzzleSmokeSizeMax =
                entry.value("muzzleSmokeSizeMax", cfg.muzzleEffects.muzzleSmokeSizeMax);

        if (cfg.muzzleEffects.muzzleSmokeSizeMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSizeMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeSizeMin = 0.0f;
        }

        if (cfg.muzzleEffects.muzzleSmokeSizeMax < cfg.muzzleEffects.muzzleSmokeSizeMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSizeMax < muzzleSmokeSizeMin, clamping",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeSizeMax = cfg.muzzleEffects.muzzleSmokeSizeMin;
        }

        cfg.muzzleEffects.muzzleSmokeSpreadDegrees =
                entry.value("muzzleSmokeSpreadDegrees", cfg.muzzleEffects.muzzleSmokeSpreadDegrees);
        if (cfg.muzzleEffects.muzzleSmokeSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeSpreadDegrees = 0.0f;
        }

        cfg.muzzleEffects.muzzleSmokeForwardBias =
                entry.value("muzzleSmokeForwardBias", cfg.muzzleEffects.muzzleSmokeForwardBias);
        if (cfg.muzzleEffects.muzzleSmokeForwardBias < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeForwardBias < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeForwardBias = 0.0f;
        } else if (cfg.muzzleEffects.muzzleSmokeForwardBias > 1.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': muzzleSmokeForwardBias > 1, clamping to 1",
                     cfg.equipmentSetId.c_str());
            cfg.muzzleEffects.muzzleSmokeForwardBias = 1.0f;
        }

        // blood
        TopdownBloodEffectConfig& bloodCfg = cfg.bloodEffects;
        bloodCfg.bloodImpactParticleCount =
                entry.value("bloodImpactParticleCount", bloodCfg.bloodImpactParticleCount);
        if (bloodCfg.bloodImpactParticleCount < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleCount < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleCount = 0;
        }

        bloodCfg.bloodImpactParticleSpeedMin =
                entry.value("bloodImpactParticleSpeedMin", bloodCfg.bloodImpactParticleSpeedMin);
        bloodCfg.bloodImpactParticleSpeedMax =
                entry.value("bloodImpactParticleSpeedMax", bloodCfg.bloodImpactParticleSpeedMax);

        if (bloodCfg.bloodImpactParticleSpeedMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSpeedMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleSpeedMin = 0.0f;
        }
        if (bloodCfg.bloodImpactParticleSpeedMax < bloodCfg.bloodImpactParticleSpeedMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSpeedMax < bloodImpactParticleSpeedMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleSpeedMax = bloodCfg.bloodImpactParticleSpeedMin;
        }

        bloodCfg.bloodImpactParticleLifetimeMsMin =
                entry.value("bloodImpactParticleLifetimeMsMin", bloodCfg.bloodImpactParticleLifetimeMsMin);
        bloodCfg.bloodImpactParticleLifetimeMsMax =
                entry.value("bloodImpactParticleLifetimeMsMax", bloodCfg.bloodImpactParticleLifetimeMsMax);

        if (bloodCfg.bloodImpactParticleLifetimeMsMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleLifetimeMsMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleLifetimeMsMin = 0.0f;
        }
        if (bloodCfg.bloodImpactParticleLifetimeMsMax < bloodCfg.bloodImpactParticleLifetimeMsMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleLifetimeMsMax < bloodImpactParticleLifetimeMsMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleLifetimeMsMax = bloodCfg.bloodImpactParticleLifetimeMsMin;
        }

        bloodCfg.bloodImpactParticleSizeMin =
                entry.value("bloodImpactParticleSizeMin", bloodCfg.bloodImpactParticleSizeMin);
        bloodCfg.bloodImpactParticleSizeMax =
                entry.value("bloodImpactParticleSizeMax", bloodCfg.bloodImpactParticleSizeMax);

        if (bloodCfg.bloodImpactParticleSizeMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSizeMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleSizeMin = 0.0f;
        }
        if (bloodCfg.bloodImpactParticleSizeMax < bloodCfg.bloodImpactParticleSizeMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactParticleSizeMax < bloodImpactParticleSizeMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactParticleSizeMax = bloodCfg.bloodImpactParticleSizeMin;
        }

        bloodCfg.bloodImpactSpreadDegrees =
                entry.value("bloodImpactSpreadDegrees", bloodCfg.bloodImpactSpreadDegrees);
        if (bloodCfg.bloodImpactSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodImpactSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodImpactSpreadDegrees = 0.0f;
        }

        bloodCfg.bloodDecalCountMin =
                entry.value("bloodDecalCountMin", bloodCfg.bloodDecalCountMin);
        bloodCfg.bloodDecalCountMax =
                entry.value("bloodDecalCountMax", bloodCfg.bloodDecalCountMax);

        if (bloodCfg.bloodDecalCountMin < 0) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalCountMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalCountMin = 0;
        }
        if (bloodCfg.bloodDecalCountMax < bloodCfg.bloodDecalCountMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalCountMax < bloodDecalCountMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalCountMax = bloodCfg.bloodDecalCountMin;
        }

        bloodCfg.bloodDecalDistanceMin =
                entry.value("bloodDecalDistanceMin", bloodCfg.bloodDecalDistanceMin);
        bloodCfg.bloodDecalDistanceMax =
                entry.value("bloodDecalDistanceMax", bloodCfg.bloodDecalDistanceMax);

        if (bloodCfg.bloodDecalDistanceMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalDistanceMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalDistanceMin = 0.0f;
        }
        if (bloodCfg.bloodDecalDistanceMax < bloodCfg.bloodDecalDistanceMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalDistanceMax < bloodDecalDistanceMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalDistanceMax = bloodCfg.bloodDecalDistanceMin;
        }

        bloodCfg.bloodDecalRadiusMin =
                entry.value("bloodDecalRadiusMin", bloodCfg.bloodDecalRadiusMin);
        bloodCfg.bloodDecalRadiusMax =
                entry.value("bloodDecalRadiusMax", bloodCfg.bloodDecalRadiusMax);

        if (bloodCfg.bloodDecalRadiusMin < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalRadiusMin < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalRadiusMin = 0.0f;
        }
        if (bloodCfg.bloodDecalRadiusMax < bloodCfg.bloodDecalRadiusMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalRadiusMax < bloodDecalRadiusMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalRadiusMax = bloodCfg.bloodDecalRadiusMin;
        }

        bloodCfg.bloodDecalSpreadDegrees =
                entry.value("bloodDecalSpreadDegrees", bloodCfg.bloodDecalSpreadDegrees);
        if (bloodCfg.bloodDecalSpreadDegrees < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalSpreadDegrees < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalSpreadDegrees = 0.0f;
        }

        bloodCfg.bloodDecalWallPadding =
                entry.value("bloodDecalWallPadding", bloodCfg.bloodDecalWallPadding);
        if (bloodCfg.bloodDecalWallPadding < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalWallPadding < 0, clamping to 0",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalWallPadding = 0.0f;
        }

        bloodCfg.bloodDecalOpacityMin =
                entry.value("bloodDecalOpacityMin", bloodCfg.bloodDecalOpacityMin);
        bloodCfg.bloodDecalOpacityMax =
                entry.value("bloodDecalOpacityMax", bloodCfg.bloodDecalOpacityMax);

        bloodCfg.bloodDecalOpacityMin = Clamp(bloodCfg.bloodDecalOpacityMin, 0.0f, 1.0f);
        bloodCfg.bloodDecalOpacityMax = Clamp(bloodCfg.bloodDecalOpacityMax, 0.0f, 1.0f);

        if (bloodCfg.bloodDecalOpacityMax < bloodCfg.bloodDecalOpacityMin) {
            TraceLog(LOG_WARNING,
                     "Player weapon config '%s': bloodDecalOpacityMax < bloodDecalOpacityMin, clamping",
                     cfg.equipmentSetId.c_str());
            bloodCfg.bloodDecalOpacityMax = bloodCfg.bloodDecalOpacityMin;
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

const TopdownPlayerWeaponConfig* TopdownPlayerGetCurrentWeaponConfig(
        const GameState& state)
{
    return FindTopdownPlayerWeaponConfigByEquipmentSetId(
            state,
            state.topdown.runtime.playerCharacter.equippedSetId);
}

bool TopdownPlayerWeaponUsesAmmo(const TopdownPlayerWeaponConfig& config)
{
    return !config.ammoType.empty() &&
           config.magazineSize > 0 &&
           config.ammoPerShot > 0;
}

bool TopdownPlayerCurrentWeaponUsesAmmo(const GameState& state)
{
    const TopdownPlayerWeaponConfig* config = TopdownPlayerGetCurrentWeaponConfig(state);
    return config != nullptr && TopdownPlayerWeaponUsesAmmo(*config);
}

namespace {
    constexpr const char* kFallbackEquipmentSetId = "knife";

    bool IsTopdownPlayerEquipmentSetOwned(
            const TopdownPlayerInventoryRuntime& inventory,
            const std::string& equipmentSetId)
    {
        return std::find(
                inventory.ownedEquipmentSetIds.begin(),
                inventory.ownedEquipmentSetIds.end(),
                equipmentSetId) != inventory.ownedEquipmentSetIds.end();
    }

    bool IsTopdownPlayerEquipmentSetUsable(
            const GameState& state,
            const std::string& equipmentSetId)
    {
        return FindTopdownPlayerWeaponConfigByEquipmentSetId(state, equipmentSetId) != nullptr &&
               HasTopdownPlayerEquipmentAnimationSet(state, equipmentSetId);
    }

    const TopdownInventoryCount* FindTopdownInventoryCount(
            const std::vector<TopdownInventoryCount>& counts,
            const std::string& id)
    {
        for (const TopdownInventoryCount& entry : counts) {
            if (entry.id == id) {
                return &entry;
            }
        }

        return nullptr;
    }

    TopdownInventoryCount* FindTopdownInventoryCount(
            std::vector<TopdownInventoryCount>& counts,
            const std::string& id)
    {
        for (TopdownInventoryCount& entry : counts) {
            if (entry.id == id) {
                return &entry;
            }
        }

        return nullptr;
    }

    TopdownInventoryCount& FindOrCreateTopdownInventoryCount(
            std::vector<TopdownInventoryCount>& counts,
            const std::string& id)
    {
        if (TopdownInventoryCount* entry = FindTopdownInventoryCount(counts, id)) {
            return *entry;
        }

        TopdownInventoryCount entry;
        entry.id = id;
        entry.count = 0;
        counts.push_back(entry);
        return counts.back();
    }

    bool PlayerWeaponCanUseReload(const TopdownPlayerWeaponConfig& config)
    {
        return TopdownPlayerWeaponUsesAmmo(config) && config.reloadDurationMs > 0.0f;
    }

    int ClampLoadedAmmoForEquipmentSet(
            const GameState& state,
            const std::string& equipmentSetId,
            int count)
    {
        const TopdownPlayerWeaponConfig* config =
                FindTopdownPlayerWeaponConfigByEquipmentSetId(state, equipmentSetId);
        if (config != nullptr && config->magazineSize > 0) {
            return std::clamp(count, 0, config->magazineSize);
        }

        return std::max(0, count);
    }

    void ValidateTopdownPlayerLoadedAmmoRuntime(GameState& state)
    {
        std::vector<TopdownInventoryCount>& loadedAmmo =
                state.topdown.runtime.playerInventory.loadedAmmo;

        loadedAmmo.erase(
                std::remove_if(
                        loadedAmmo.begin(),
                        loadedAmmo.end(),
                        [](const TopdownInventoryCount& entry) {
                            return entry.id.empty();
                        }),
                loadedAmmo.end());

        for (TopdownInventoryCount& entry : loadedAmmo) {
            entry.count = ClampLoadedAmmoForEquipmentSet(state, entry.id, entry.count);
        }
    }

    void StopPlayerRifleLoopIfPlaying(GameState& state)
    {
        TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
        if (!attack.rifleLoopPlaying) {
            return;
        }

        StopSoundById(state, "rifle_full_auto_loop");
        PlaySoundById(state, "rifle_full_auto_end");
        attack.rifleLoopPlaying = false;
    }

    void ClearPlayerReloadState(TopdownPlayerAttackRuntime& attack)
    {
        attack.reloadActive = false;
        attack.reloadTimerMs = 0.0f;
        attack.reloadDurationMs = 0.0f;
        attack.reloadEquipmentSetId.clear();
    }

    bool CompletePlayerReload(GameState& state, const TopdownPlayerWeaponConfig& config)
    {
        if (!PlayerWeaponCanUseReload(config)) {
            TraceLog(LOG_WARNING,
                     "Cannot complete reload for player equipment '%s': invalid ammo config",
                     config.equipmentSetId.c_str());
            return false;
        }

        const int loadedAmmo = std::clamp(
                TopdownPlayerGetLoadedAmmo(state, config.equipmentSetId),
                0,
                config.magazineSize);
        const int reserveAmmo = std::max(0, TopdownPlayerGetReserveAmmo(state, config.ammoType));
        const int needed = std::max(0, config.magazineSize - loadedAmmo);
        const int taken = std::min(needed, reserveAmmo);

        TopdownPlayerSetLoadedAmmo(state, config.equipmentSetId, loadedAmmo + taken);
        TopdownPlayerSetReserveAmmo(state, config.ammoType, reserveAmmo - taken);
        return taken > 0;
    }

    void ApplyTopdownPlayerEquipmentConfig(
            GameState& state,
            const TopdownPlayerWeaponConfig& config)
    {
        TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;
        TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

        StopPlayerRifleLoopIfPlaying(state);

        character.equippedSetId = config.equipmentSetId;
        character.currentUpperHandle = FindTopdownPlayerEquipmentAnimationHandle(
                state,
                character.equippedSetId,
                "idle");

        attack.equipmentSetId = config.equipmentSetId;
        attack.currentFireMode = config.defaultFireMode;
        attack.active = false;
        attack.state = TopdownPlayerAttackState::Idle;
        attack.input = TopdownAttackInput::Primary;
        attack.attackType = TopdownAttackType::None;
        attack.stateTimeMs = 0.0f;
        attack.animationDurationMs = 0.0f;
        attack.cooldownRemainingMs = 0.0f;
        attack.triggerHeld = false;
        attack.wantsTriggerRelease = false;
        attack.burstShotsRemaining = 0;
        attack.burstShotTimerMs = 0.0f;
        attack.pendingPrimaryAttack = false;
        attack.pendingSecondaryAttack = false;
        ClearPlayerReloadState(attack);
        attack.fullAutoShakeCooldownMs = 0.0f;
        attack.meleeHitPending = false;
        attack.meleeHitApplied = false;
        attack.rifleLoopPlaying = false;
    }
}

bool TopdownPlayerHasEquipmentSet(
        const GameState& state,
        const std::string& equipmentSetId)
{
    return IsTopdownPlayerEquipmentSetOwned(
            state.topdown.runtime.playerInventory,
            equipmentSetId);
}

bool TopdownPlayerAddEquipmentSet(
        GameState& state,
        const std::string& equipmentSetId)
{
    TopdownPlayerInventoryRuntime& inventory = state.topdown.runtime.playerInventory;
    if (IsTopdownPlayerEquipmentSetOwned(inventory, equipmentSetId)) {
        return true;
    }

    if (!IsTopdownPlayerEquipmentSetUsable(state, equipmentSetId)) {
        TraceLog(LOG_WARNING,
                 "Cannot add player equipment set '%s': missing weapon config or animation set",
                 equipmentSetId.c_str());
        return false;
    }

    inventory.ownedEquipmentSetIds.push_back(equipmentSetId);
    return true;
}

bool TopdownPlayerRemoveEquipmentSet(
        GameState& state,
        const std::string& equipmentSetId)
{
    if (equipmentSetId == kFallbackEquipmentSetId) {
        TraceLog(LOG_WARNING, "Cannot remove fallback player equipment set '%s'", kFallbackEquipmentSetId);
        return false;
    }

    TopdownPlayerInventoryRuntime& inventory = state.topdown.runtime.playerInventory;
    auto it = std::find(
            inventory.ownedEquipmentSetIds.begin(),
            inventory.ownedEquipmentSetIds.end(),
            equipmentSetId);
    if (it == inventory.ownedEquipmentSetIds.end()) {
        return true;
    }

    inventory.ownedEquipmentSetIds.erase(it);

    if (state.topdown.runtime.playerCharacter.equippedSetId == equipmentSetId ||
        state.topdown.runtime.playerAttack.equipmentSetId == equipmentSetId) {
        if (!TopdownPlayerEquipEquipmentSet(state, kFallbackEquipmentSetId)) {
            TopdownValidatePlayerEquipmentRuntime(state);
        }
    }

    return true;
}

bool TopdownPlayerEquipEquipmentSet(
        GameState& state,
        const std::string& equipmentSetId)
{
    const TopdownPlayerWeaponConfig* config =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, equipmentSetId);
    if (config == nullptr) {
        TraceLog(LOG_WARNING,
                 "Cannot equip player equipment set '%s': missing weapon config",
                 equipmentSetId.c_str());
        return false;
    }

    if (!TopdownPlayerHasEquipmentSet(state, equipmentSetId)) {
        TraceLog(LOG_WARNING,
                 "Cannot equip player equipment set '%s': not owned",
                 equipmentSetId.c_str());
        return false;
    }

    if (!HasTopdownPlayerEquipmentAnimationSet(state, equipmentSetId)) {
        TraceLog(LOG_WARNING,
                 "Cannot equip player equipment set '%s': missing animation set",
                 equipmentSetId.c_str());
        return false;
    }

    ApplyTopdownPlayerEquipmentConfig(state, *config);
    return true;
}

bool TopdownPlayerEquipSlot(GameState& state, int slot)
{
    const TopdownPlayerWeaponConfig* config = FindTopdownPlayerWeaponConfigBySlot(state, slot);
    if (config == nullptr) {
        return false;
    }

    return TopdownPlayerEquipEquipmentSet(state, config->equipmentSetId);
}

int TopdownPlayerGetReserveAmmo(
        const GameState& state,
        const std::string& ammoType)
{
    if (ammoType.empty()) {
        return 0;
    }

    const TopdownInventoryCount* entry = FindTopdownInventoryCount(
            state.topdown.runtime.playerInventory.reserveAmmo,
            ammoType);
    return entry != nullptr ? std::max(0, entry->count) : 0;
}

int TopdownPlayerGetLoadedAmmo(
        const GameState& state,
        const std::string& equipmentSetId)
{
    if (equipmentSetId.empty()) {
        return 0;
    }

    const TopdownInventoryCount* entry = FindTopdownInventoryCount(
            state.topdown.runtime.playerInventory.loadedAmmo,
            equipmentSetId);
    return entry != nullptr
            ? ClampLoadedAmmoForEquipmentSet(state, equipmentSetId, entry->count)
            : 0;
}

bool TopdownPlayerSetReserveAmmo(
        GameState& state,
        const std::string& ammoType,
        int count)
{
    if (ammoType.empty()) {
        return false;
    }

    TopdownInventoryCount& entry = FindOrCreateTopdownInventoryCount(
            state.topdown.runtime.playerInventory.reserveAmmo,
            ammoType);
    entry.count = std::max(0, count);
    return true;
}

bool TopdownPlayerSetLoadedAmmo(
        GameState& state,
        const std::string& equipmentSetId,
        int count)
{
    if (equipmentSetId.empty()) {
        return false;
    }

    TopdownInventoryCount& entry = FindOrCreateTopdownInventoryCount(
            state.topdown.runtime.playerInventory.loadedAmmo,
            equipmentSetId);
    entry.count = ClampLoadedAmmoForEquipmentSet(state, equipmentSetId, count);
    return true;
}

bool TopdownPlayerAddAmmo(
        GameState& state,
        const std::string& ammoType,
        int amount)
{
    if (ammoType.empty() || amount <= 0) {
        return false;
    }

    TopdownInventoryCount& entry = FindOrCreateTopdownInventoryCount(
            state.topdown.runtime.playerInventory.reserveAmmo,
            ammoType);
    entry.count = std::max(0, entry.count) + amount;
    return true;
}

bool TopdownPlayerRemoveAmmo(
        GameState& state,
        const std::string& ammoType,
        int amount)
{
    if (ammoType.empty() || amount <= 0) {
        return false;
    }

    TopdownInventoryCount& entry = FindOrCreateTopdownInventoryCount(
            state.topdown.runtime.playerInventory.reserveAmmo,
            ammoType);
    entry.count = std::max(0, entry.count - amount);
    return true;
}

bool TopdownPlayerCanReloadCurrentWeapon(const GameState& state)
{
    const TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    if (attack.active || attack.reloadActive) {
        return false;
    }

    const std::string& equipmentSetId = state.topdown.runtime.playerCharacter.equippedSetId;
    const TopdownPlayerWeaponConfig* config =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, equipmentSetId);
    if (config == nullptr || !PlayerWeaponCanUseReload(*config)) {
        return false;
    }

    if (!TopdownPlayerHasEquipmentSet(state, config->equipmentSetId)) {
        return false;
    }

    const int loadedAmmo = std::clamp(
            TopdownPlayerGetLoadedAmmo(state, config->equipmentSetId),
            0,
            config->magazineSize);
    if (loadedAmmo >= config->magazineSize) {
        return false;
    }

    return TopdownPlayerGetReserveAmmo(state, config->ammoType) > 0;
}

bool TopdownPlayerStartReload(GameState& state)
{
    if (!TopdownPlayerCanReloadCurrentWeapon(state)) {
        return false;
    }

    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    const std::string& equipmentSetId = state.topdown.runtime.playerCharacter.equippedSetId;
    const TopdownPlayerWeaponConfig* config =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, equipmentSetId);
    if (config == nullptr) {
        return false;
    }

    attack.pendingPrimaryAttack = false;
    attack.pendingSecondaryAttack = false;
    attack.triggerHeld = false;
    attack.wantsTriggerRelease = false;
    attack.burstShotsRemaining = 0;
    attack.burstShotTimerMs = 0.0f;
    StopPlayerRifleLoopIfPlaying(state);

    attack.reloadActive = true;
    attack.reloadTimerMs = 0.0f;
    attack.reloadDurationMs = config->reloadDurationMs;
    attack.reloadEquipmentSetId = config->equipmentSetId;
    return true;
}

void TopdownPlayerCancelReload(GameState& state)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    ClearPlayerReloadState(attack);
    attack.pendingPrimaryAttack = false;
    attack.pendingSecondaryAttack = false;
    attack.triggerHeld = false;
    attack.wantsTriggerRelease = false;
    StopPlayerRifleLoopIfPlaying(state);
}

void TopdownPlayerUpdateReload(GameState& state, float dt)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    if (!attack.reloadActive) {
        return;
    }

    attack.pendingPrimaryAttack = false;
    attack.pendingSecondaryAttack = false;
    attack.triggerHeld = false;
    attack.wantsTriggerRelease = false;
    StopPlayerRifleLoopIfPlaying(state);

    const TopdownPlayerWeaponConfig* config =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, attack.reloadEquipmentSetId);
    if (config == nullptr ||
        attack.reloadEquipmentSetId != state.topdown.runtime.playerCharacter.equippedSetId ||
        !TopdownPlayerHasEquipmentSet(state, attack.reloadEquipmentSetId) ||
        !PlayerWeaponCanUseReload(*config)) {
        TopdownPlayerCancelReload(state);
        return;
    }

    attack.reloadDurationMs = std::max(0.0f, attack.reloadDurationMs);
    if (attack.reloadDurationMs <= 0.0f) {
        attack.reloadDurationMs = config->reloadDurationMs;
    }

    attack.reloadTimerMs = std::max(0.0f, attack.reloadTimerMs + dt * 1000.0f);
    if (attack.reloadTimerMs < attack.reloadDurationMs) {
        return;
    }

    CompletePlayerReload(state, *config);
    ClearPlayerReloadState(attack);
}

void TopdownPlayerValidateReloadState(GameState& state)
{
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;
    if (!attack.reloadActive) {
        ClearPlayerReloadState(attack);
        return;
    }

    if (attack.reloadEquipmentSetId.empty()) {
        attack.reloadEquipmentSetId = attack.equipmentSetId;
    }

    const TopdownPlayerWeaponConfig* config =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, attack.reloadEquipmentSetId);
    if (config == nullptr ||
        attack.reloadEquipmentSetId != state.topdown.runtime.playerCharacter.equippedSetId ||
        !TopdownPlayerHasEquipmentSet(state, attack.reloadEquipmentSetId) ||
        !PlayerWeaponCanUseReload(*config)) {
        TopdownPlayerCancelReload(state);
        return;
    }

    attack.reloadTimerMs = std::max(0.0f, attack.reloadTimerMs);
    attack.reloadDurationMs = attack.reloadDurationMs > 0.0f
            ? attack.reloadDurationMs
            : config->reloadDurationMs;

    if (attack.reloadTimerMs >= attack.reloadDurationMs) {
        CompletePlayerReload(state, *config);
        ClearPlayerReloadState(attack);
    }
}

void TopdownValidatePlayerEquipmentRuntime(GameState& state)
{
    ValidateTopdownPlayerLoadedAmmoRuntime(state);

    TopdownPlayerInventoryRuntime& inventory = state.topdown.runtime.playerInventory;

    if (IsTopdownPlayerEquipmentSetUsable(state, kFallbackEquipmentSetId)) {
        if (!IsTopdownPlayerEquipmentSetOwned(inventory, kFallbackEquipmentSetId)) {
            inventory.ownedEquipmentSetIds.push_back(kFallbackEquipmentSetId);
        }
    } else {
        TraceLog(LOG_ERROR,
                 "Player fallback equipment set '%s' is missing weapon config or animation set",
                 kFallbackEquipmentSetId);
    }

    const std::string equippedSetId = state.topdown.runtime.playerCharacter.equippedSetId;
    if (!equippedSetId.empty() &&
        TopdownPlayerHasEquipmentSet(state, equippedSetId) &&
        IsTopdownPlayerEquipmentSetUsable(state, equippedSetId)) {
        const TopdownPlayerWeaponConfig* config =
                FindTopdownPlayerWeaponConfigByEquipmentSetId(state, equippedSetId);
        if (config != nullptr) {
            ApplyTopdownPlayerEquipmentConfig(state, *config);
            return;
        }
    }

    if (TopdownPlayerHasEquipmentSet(state, kFallbackEquipmentSetId) &&
        TopdownPlayerEquipEquipmentSet(state, kFallbackEquipmentSetId)) {
        return;
    }

    for (const std::string& ownedId : inventory.ownedEquipmentSetIds) {
        if (IsTopdownPlayerEquipmentSetUsable(state, ownedId)) {
            const TopdownPlayerWeaponConfig* config =
                    FindTopdownPlayerWeaponConfigByEquipmentSetId(state, ownedId);
            if (config != nullptr) {
                ApplyTopdownPlayerEquipmentConfig(state, *config);
                return;
            }
        }
    }
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

    state.topdown.runtime.playerInventory.ownedEquipmentSetIds.clear();
    state.topdown.runtime.playerInventory.ownedEquipmentSetIds.push_back(kFallbackEquipmentSetId);
    state.topdown.runtime.playerInventory.reserveAmmo.clear();
    state.topdown.runtime.playerInventory.loadedAmmo.clear();

    runtime.equippedSetId = kFallbackEquipmentSetId;

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

    TopdownValidatePlayerEquipmentRuntime(state);
}
