#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "resources/ResourceData.h"
#include "topdown/TopdownCoreData.h"
#include "topdown/TopdownRenderData.h"

enum class TopdownPlayerAttackState {
    Idle,
    Recover
};

enum class TopdownPlayerLifeState {
    Alive,
    Dying,
    Dead,
    GameOver
};

struct TopdownPlayerWeaponConfig {
    std::string equipmentSetId;
    int slot = 0;

    TopdownAttackType primaryAttackType = TopdownAttackType::None;
    TopdownAttackType secondaryAttackType = TopdownAttackType::None;

    float primaryCooldownMs = 0.0f;
    float secondaryCooldownMs = 0.0f;

    float rangedDamage = 0.0f;
    float meleeDamage = 0.0f;

    float maxRange = 0.0f;

    int pelletCount = 1;
    float spreadDegrees = 0.0f;

    float meleeRange = 0.0f;
    float meleeArcDegrees = 0.0f;

    float rangedKnockback = 0.0f;
    float meleeKnockback = 0.0f;

    float rangedDoorImpulse = 0.0f;
    float meleeDoorImpulse = 0.0f;
    float noiseRadius = 1200.0f;

    std::string ammoType;
    int magazineSize = 0;
    int ammoPerShot = 0;
    float reloadDurationMs = 0.0f;
    std::string reloadSoundId;

    TopdownTracerStyle tracerStyle = TopdownTracerStyle::None;

    TopdownBallisticImpactEffectConfig ballisticImpactEffects{};
    TopdownMuzzleEffectConfig muzzleEffects{};
    TopdownBloodEffectConfig bloodEffects{};

    std::vector<TopdownFireMode> supportedFireModes;
    TopdownFireMode defaultFireMode = TopdownFireMode::SemiAuto;

    int burstCount = 3;
    float burstIntervalMs = 70.0f;
};

struct TopdownPlayerRuntime {
    Vector2 position{};
    Vector2 velocity{};
    Vector2 desiredVelocity{};
    Vector2 facing{1.0f, 0.0f};

    float radius = 45.0f;

    float walkSpeed = 550.0f;
    float runSpeed = 750.0f;

    float acceleration = 2800.0f;
    float deceleration = 4500.0f;

    bool wantsRun = false;

    float moveInputForward = 0.0f;
    float moveInputRight = 0.0f;

    float health = 100.0f;
    float maxHealth = 100.0f;

    float hurtCooldownRemainingMs = 0.0f;

    float hitSlowdownRemainingMs = 0.0f;
    float hitSlowdownMultiplier = 1.0f;

    float damageFlashRemainingMs = 0.0f;
    float lowHealthEffectWeight = 0.0f;

    TopdownPlayerLifeState lifeState = TopdownPlayerLifeState::Alive;
};

struct TopdownCameraData {
    float viewportWidth = 1920.0f;
    float viewportHeight = 1080.0f;

    float deadzoneWidth = 160.0f;
    float deadzoneHeight = 40.0f;

    float aimMaxOffset = 1200.0f;
    float aimStrength = 0.30f;
    float aimResponse = 8.0f;

    float smoothing = 10.0f;
};

enum class TopdownCameraMode {
    Player,
    Scripted,
    Manual
};

struct TopdownCameraRuntime {
    Vector2 position{};
    Vector2 targetPosition{};
    Vector2 aimOffset{};

    TopdownCameraMode mode = TopdownCameraMode::Player;

    Vector2 scriptedTarget{};
    Vector2 panStart{};
    Vector2 panEnd{};
    float panTimerMs = 0.0f;
    float panDurationMs = 0.0f;
    bool isPanning = false;
};

enum class TopdownLocomotionType {
    Idle,
    Forward,
    Backward,
    StrafeLeft,
    StrafeRight
};

struct TopdownPlayerAnimationEntry {
    std::string id;
    SpriteAssetHandle spriteHandle = -1;

    bool hasMuzzle = false;
    Vector2 muzzle{};
};

struct TopdownCharacterAssetData {
    bool loaded = false;
    std::string id;

    float maxHealth = 100.0f;
    float hurtCooldownMs = 150.0f;
    float meleeHitSlowdownMs = 100.0f;
    float meleeHitSlowdownMultiplier = 0.65f;

    std::vector<TopdownPlayerAnimationEntry> animations;
    std::vector<TopdownPlayerWeaponConfig> weaponConfigs;
};

struct TopdownCharacterRuntime {
    bool active = false;

    std::string equippedSetId = "knife";

    TopdownLocomotionType locomotion = TopdownLocomotionType::Idle;
    bool running = false;

    float bodyFacingRadians = 0.0f;
    float desiredAimRadians = 0.0f;
    float feetRotationRadians = 0.0f;
    float upperRotationRadians = 0.0f;

    float turnSpeedRadians = 7.0f;
    float maxUpperBodyTwistRadians = 85.0f * DEG2RAD;

    float minAimDistanceEnter = 24.0f;
    float minAimDistanceExit = 40.0f;
    bool aimFrozen = false;

    float feetAnimationTimeMs = 0.0f;
    float upperAnimationTimeMs = 0.0f;

    SpriteAssetHandle currentFeetHandle = -1;
    SpriteAssetHandle currentUpperHandle = -1;
};

struct TopdownPlayerAttackRuntime {
    bool active = false;

    TopdownPlayerAttackState state = TopdownPlayerAttackState::Idle;
    TopdownAttackInput input = TopdownAttackInput::Primary;
    TopdownAttackType attackType = TopdownAttackType::None;

    float stateTimeMs = 0.0f;
    float animationDurationMs = 0.0f;
    float cooldownRemainingMs = 0.0f;

    std::string equipmentSetId;

    TopdownFireMode currentFireMode = TopdownFireMode::SemiAuto;

    bool triggerHeld = false;
    bool wantsTriggerRelease = false;

    int burstShotsRemaining = 0;
    float burstShotTimerMs = 0.0f;

    bool pendingPrimaryAttack = false;
    bool pendingSecondaryAttack = false;

    bool reloadActive = false;
    float reloadTimerMs = 0.0f;
    float reloadDurationMs = 0.0f;
    std::string reloadEquipmentSetId;

    float fullAutoShakeCooldownMs = 0.0f;

    bool meleeHitPending = false;
    bool meleeHitApplied = false;

    bool rifleLoopPlaying = false;
};

struct TopdownInventoryCount {
    std::string id;
    int count = 0;
};

struct TopdownPlayerInventoryRuntime {
    std::vector<std::string> ownedEquipmentSetIds;

    std::vector<TopdownInventoryCount> reserveAmmo;
    std::vector<TopdownInventoryCount> loadedAmmo;

    int carriedHealthItems = 0;
    int maxCarriedHealthItems = 3;
    float carriedHealthHealAmount = 0.0f;
    float carriedHealthConsumeMs = 0.0f;

    bool healthUseActive = false;
    float healthUseTimerMs = 0.0f;
    float healthUseDurationMs = 0.0f;
    float healthUseHealAmount = 0.0f;
};
