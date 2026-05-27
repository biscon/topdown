#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "resources/ResourceData.h"
#include "topdown/TopdownCoreData.h"
#include "topdown/TopdownPlayerData.h"
#include "topdown/TopdownRenderData.h"

enum class TopdownNpcAiMode {
    None,
    SeekAndDestroy,
    HoldAndFire
};

enum class TopdownNpcEngagementState {
    Unaware,
    Guarding,
    Reacting,
    Investigating,
    Engaged,
    ReturningToGuardPost
};

struct TopdownNpcPerceptionResult {
    bool seesPlayer = false;
    bool hearsPlayer = false;
    bool detectsPlayer = false;

    bool heardGunshot = false;
    Vector2 detectedPlayerPosition{};
    Vector2 heardGunshotPosition{};
};

enum class TopdownNpcCombatState {
    None,
    Chase,
    Investigation,
    Attack,
    Recover,
    Search
};

enum class TopdownNpcAnimationMode {
    AutomaticLocomotion,
    ScriptLoop
};

enum class TopdownNpcMoveOwner {
    None,
    Ai,
    ScriptCommand,
    Patrol
};

enum class TopdownNpcScriptBehaviorMode {
    None,
    PatrolRoute
};

struct TopdownNpcMoveState {
    bool active = false;
    bool running = false;
    TopdownNpcMoveOwner owner = TopdownNpcMoveOwner::None;

    std::vector<int> debugTrianglePath;
    std::vector<Vector2> pathPoints;
    int currentPoint = 0;

    Vector2 finalTarget{};
    bool hasFinalTarget = false;

    float currentSpeed = 0.0f;
    float acceleration = 1800.0f;
    float deceleration = 2200.0f;
    float arrivalRadius = 6.0f;
    float stopDistance = 140.0f;
};

struct TopdownNpcPatrolState {
    bool active = false;
    bool paused = false;
    bool interrupted = false;
    bool loop = true;
    bool running = false;

    std::vector<std::string> spawnIds;
    int currentPointIndex = 0;
    int contextHandle = -1;
    int slotIndex = -1;

    float waitDurationMs = 0.0f;
    float waitTimerMs = 0.0f;
};

struct TopdownNpcScriptBehaviorState {
    TopdownNpcScriptBehaviorMode mode = TopdownNpcScriptBehaviorMode::None;
    TopdownNpcPatrolState patrol;
};

struct TopdownNpcColorSet3 {
    Color colors[3]{};
};

struct TopdownNpcColorSet4 {
    Color colors[4]{};
};

struct TopdownNpcColorSet5 {
    Color colors[5]{};
};

struct TopdownNamedNpcColorSet3 {
    std::string name;
    TopdownNpcColorSet3 set;
};

struct TopdownNamedNpcColorSet4 {
    std::string name;
    TopdownNpcColorSet4 set;
};

struct TopdownNamedNpcColorSet5 {
    std::string name;
    TopdownNpcColorSet5 set;
};

struct TopdownNpcColorPresetRegistry {
    bool loaded = false;

    std::vector<TopdownNamedNpcColorSet3> skinPresets;
    std::vector<TopdownNamedNpcColorSet3> hairPresets;
    std::vector<TopdownNamedNpcColorSet5> chestPresets;
    std::vector<TopdownNamedNpcColorSet4> legsPresets;
};

struct TopdownNpcColorSubstitutionDefinition {
    bool active = false;

    TopdownNpcColorSet3 sourceSkin;
    TopdownNpcColorSet3 sourceHair;
    TopdownNpcColorSet5 sourceChest;
    TopdownNpcColorSet4 sourceLegs;

    std::string skinPresetId;
    std::string hairPresetId;
    std::string chestPresetId;
    std::string legsPresetId;
};

struct TopdownNpcResolvedColorSubstitution {
    bool active = false;

    TopdownNpcColorSet3 dstSkin;
    TopdownNpcColorSet3 dstHair;
    TopdownNpcColorSet5 dstChest;
    TopdownNpcColorSet4 dstLegs;

    std::string resolvedSkinPresetId;
    std::string resolvedHairPresetId;
    std::string resolvedChestPresetId;
    std::string resolvedLegsPresetId;
};

struct TopdownNpcAnimationSourceDefinition {
    std::string asepriteJsonPath;
    bool hasOrigin = false;
    Vector2 origin{};
};

struct TopdownNpcAssetDefinition {
    std::string assetId;
    float baseDrawScale = 1.0f;
    float collisionRadius = 32.0f;

    float walkSpeed = 450.0f;
    float runSpeed = 700.0f;
    float hurtStunMs = 0.0f;
    float maxHealth = 100.0f;
    float corpseExpirationMs = -1.0f;

    bool hostile = true;
    TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;

    TopdownAttackType attackType;
    TopdownTracerStyle rangedTracerStyle = TopdownTracerStyle::Handgun;
    int rangedPelletCount = 1;
    float rangedSpreadDegrees = 6.0f;
    float rangedMaxRange = 800.0f;
    TopdownBallisticImpactEffectConfig ballisticImpactEffects{};
    TopdownMuzzleEffectConfig muzzleEffects{};
    float reactionTimeMs = 180.0f;
    float aimInaccuracyMinDegrees = 2.0f;
    float aimInaccuracyMaxDegrees = 10.0f;

    float visionRange = 700.0f;
    float hearingRange = 220.0f;
    float gunshotHearingRange = 1000.0f;
    float visionHalfAngleDegrees = 65.0f;

    float attackRange = 95.0f;
    float attackCooldownMs = 900.0f;
    float attackDamage = 25.0f;
    float attackHitNormalizedTime = 0.7f;
    float attackRecoverMs = 250.0f;

    float chaseRepathIntervalMs = 250.0f;

    float meleeHitPosX = 0.0f;
    float meleeHitPosY = 0.0f;
    std::string attackStartSoundId;
    std::string attackConnectSoundId;
    std::vector<std::string> hitReactionSoundIds;

    TopdownNpcAttackEffectsConfig attackEffects;

    std::vector<TopdownNpcAnimationSourceDefinition> animations;

    TopdownNpcColorSubstitutionDefinition colorSubstitution;
};

struct TopdownNpcClipRef {
    SpriteAssetHandle spriteHandle = -1;
    int clipIndex = -1;
    std::string clipName;
};

struct TopdownNpcAssetRuntime {
    bool loaded = false;

    std::string assetId;

    TopdownNpcClipRef idleClip;
    TopdownNpcClipRef walkClip;
    TopdownNpcClipRef runClip;
    TopdownNpcClipRef hurtClip;
    TopdownNpcClipRef deathClip;
    TopdownNpcClipRef rangedAttackClip;
    TopdownNpcClipRef meleeAttackClip;

    std::vector<SpriteAssetHandle> spriteHandles;

    float baseDrawScale = 1.0f;
    float collisionRadius = 32.0f;

    float walkSpeed = 450.0f;
    float runSpeed = 700.0f;
    float hurtStunMs = 0.0f;
    float maxHealth = 100.0f;
    float corpseExpirationMs = -1.0f;

    bool hostile = true;
    TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;

    TopdownAttackType attackType;
    TopdownTracerStyle rangedTracerStyle = TopdownTracerStyle::Handgun;
    int rangedPelletCount = 1;
    float rangedSpreadDegrees = 6.0f;
    float rangedMaxRange = 800.0f;
    TopdownBallisticImpactEffectConfig ballisticImpactEffects{};
    TopdownMuzzleEffectConfig muzzleEffects{};
    float reactionTimeMs = 180.0f;
    float aimInaccuracyMinDegrees = 2.0f;
    float aimInaccuracyMaxDegrees = 10.0f;

    float visionRange = 700.0f;
    float hearingRange = 220.0f;
    float gunshotHearingRange = 1000.0f;
    float visionHalfAngleDegrees = 65.0f;

    float attackRange = 95.0f;
    float attackCooldownMs = 900.0f;
    float attackDamage = 25.0f;
    float attackHitNormalizedTime = 0.7f;
    float attackRecoverMs = 250.0f;

    float chaseRepathIntervalMs = 250.0f;

    float meleeHitPosX = 0.0f;
    float meleeHitPosY = 0.0f;
    std::string attackStartSoundId;
    std::string attackConnectSoundId;
    std::vector<std::string> hitReactionSoundIds;

    TopdownNpcAttackEffectsConfig attackEffects;

    TopdownNpcColorSubstitutionDefinition colorSubstitution;
};

struct TopdownNpcRuntime {
    TopdownCharacterHandle handle = -1;

    std::string id;
    std::string assetId;

    bool active = false;
    bool visible = true;
    bool dead = false;
    bool corpse = false;
    bool hostile = true;
    bool persistentChase = false;
    bool guard = false;

    TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;

    TopdownNpcEngagementState engagementState = TopdownNpcEngagementState::Unaware;
    TopdownNpcCombatState combatState = TopdownNpcCombatState::None;

    float health = 100.0f;
    float corpseExpirationMs = -1.0f;
    float corpseElapsedMs = 0.0f;

    float visionRange = 700.0f;
    float hearingRange = 220.0f;
    float gunshotHearingRange = 1000.0f;
    float visionHalfAngleDegrees = 65.0f;

    float attackRange = 95.0f;
    float preferredAttackRangeFactor = 0.9f;
    float attackCooldownMs = 900.0f;
    float attackCooldownRemainingMs = 0.0f;
    float attackDamage = 25.0f;
    float attackHitNormalizedTime = 0.7f;
    float attackRecoverMs = 250.0f;

    float chaseRepathIntervalMs = 250.0f;

    bool hasPlayerTarget = false;
    Vector2 lastKnownPlayerPosition{};
    Vector2 investigationPosition{};

    float repathTimerMs = 0.0f;

    bool attackHitPending = false;
    bool attackHitApplied = false;
    float attackStateTimeMs = 0.0f;
    float attackAnimationDurationMs = 0.0f;

    float searchStateTimeMs = 0.0f;
    float searchDurationMs = 1600.0f;
    float searchBaseFacingRadians = 0.0f;
    float searchSweepDegrees = 300.0f;

    float renderOpacity = 1.0f;

    Vector2 position{};
    Vector2 facing{1.0f, 0.0f};
    Vector2 currentVelocity{};

    float collisionRadius = 32.0f;

    float rotationRadians = 0.0f;

    TopdownNpcAnimationMode animationMode = TopdownNpcAnimationMode::AutomaticLocomotion;

    TopdownNpcClipRef automaticLoopClip;
    float automaticLoopTimeMs = 0.0f;

    TopdownNpcClipRef scriptLoopClip;
    float scriptLoopTimeMs = 0.0f;

    bool oneShotActive = false;
    TopdownNpcClipRef oneShotClip;
    float oneShotTimeMs = 0.0f;

    float hurtStunRemainingMs = 0.0f;

    Vector2 knockbackVelocity{};
    float knockbackDeceleration = 5000.0f;

    TopdownNpcMoveState move;
    TopdownNpcScriptBehaviorState scriptBehavior;

    bool moving = false;
    bool running = false;
    float painSoundCooldownMs = 0.0f;

    float meleeHitPosX = 0.0f;
    float meleeHitPosY = 0.0f;
    std::string attackStartSoundId;
    std::string attackConnectSoundId;
    std::vector<std::string> hitReactionSoundIds;

    TopdownNpcAttackEffectsConfig attackEffects;

    float chaseStuckTimerMs = 0.0f;
    Vector2 chaseStuckLastPosition{};

    Vector2 patrolLastProgressPosition{};
    float patrolStuckTimerMs = 0.0f;
    float patrolYieldTimerMs = 0.0f;
    float patrolRetryDelayMs = 0.0f;
    int patrolStuckCount = 0;
    bool patrolIsYielding = false;
    bool patrolIsRetryDelay = false;

    int investigationContextHandle = -1;
    int investigationSlotIndex = -1;
    float investigationProgressTimerMs = 0.0f;
    Vector2 investigationLastPosition{};
    float investigationRetargetCooldownMs = 300;

    float reactionTimerMs = 0.0f;
    bool hasReactedToPlayer = false;

    int strafeDir = 1;
    float strafeTimerMs = 0.0f;
    float engagedLostTargetTimerMs = 0.0f;

    Vector2 guardHomePosition{};
    bool hasGuardHomePosition = false;
    float guardLookAtSoundTimerMs = 0.0f;
    float guardLookAtSoundRadians = 0.0f;

    TopdownNpcResolvedColorSubstitution colorSubstitution;
};

struct TopdownNpcInvestigationSlot {
    Vector2 position{};
    int claimedByNpcHandle = -1;
};

struct TopdownNpcPatrolSlot {
    Vector2 position{};
    int claimedByNpcHandle = -1;
};

struct TopdownNpcPatrolContext {
    bool active = false;
    int handle = -1;
    std::string waypointSpawnId;
    Vector2 origin{};
    std::vector<TopdownNpcPatrolSlot> slots;
};

struct TopdownNpcInvestigationContext {
    bool active = false;
    int handle = -1;
    Vector2 origin{};
    std::vector<TopdownNpcInvestigationSlot> slots;
};
