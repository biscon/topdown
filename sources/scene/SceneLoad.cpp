#include "SceneLoad.h"

#include <filesystem>
#include <fstream>

#include "utils/json.hpp"
#include "scene/TiledImport.h"
#include "resources/AsepriteAsset.h"
#include "nav/NavMeshBuild.h"
#include "raylib.h"
#include "scripting/ScriptSystem.h"
#include "adventure/AdventureHelpers.h"
#include "adventure/ActorDefinitionAsset.h"
#include "adventure/Inventory.h"
#include "resources/Resources.h"
#include "audio/Audio.h"
#include "adventure/AdventureCamera.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static const SceneSpawnPoint* FindSpawnById(const SceneData& scene, const std::string& spawnId)
{
    for (const auto& spawn : scene.spawns) {
        if (spawn.id == spawnId) {
            return &spawn;
        }
    }
    return nullptr;
}

static void ApplyActorFacingFromSceneFacing(ActorInstance& actor, SceneFacing facing)
{
    switch (facing) {
        case SceneFacing::Left:
            actor.facing = ActorFacing::Left;
            actor.currentAnimation = "idle_right";
            actor.flipX = true;
            break;

        case SceneFacing::Right:
            actor.facing = ActorFacing::Right;
            actor.currentAnimation = "idle_right";
            actor.flipX = false;
            break;

        case SceneFacing::Back:
            actor.facing = ActorFacing::Back;
            actor.currentAnimation = "idle_back";
            actor.flipX = false;
            break;

        case SceneFacing::Front:
        default:
            actor.facing = ActorFacing::Front;
            actor.currentAnimation = "idle_front";
            actor.flipX = false;
            break;
    }

    actor.animationTimeMs = 0.0f;
    actor.inIdleState = true;
    actor.stoppedTimeMs = actor.idleDelayMs;
}

static void InitializeActorInstanceFromDefinition(
        ActorInstance& actor,
        const ActorDefinitionData& def)
{
    actor.actorId = def.actorId;
    actor.actorDefIndex = -1; // caller sets this correctly after creation if needed
    actor.activeInScene = true;
    actor.visible = true;

    actor.walkSpeed = def.walkSpeed;
    actor.fastMoveMultiplier = def.fastMoveMultiplier;
    actor.idleDelayMs = def.idleDelayMs;

    actor.currentAnimation = "idle_front";
    actor.flipX = false;
    actor.animationTimeMs = 0.0f;
    actor.stoppedTimeMs = 0.0f;
    actor.inIdleState = true;
    actor.path = {};
    actor.scriptAnimationActive = false;
    actor.scriptAnimationDurationMs = 0.0f;
}

void UnloadCurrentScene(GameState& state)
{
    state.adventure.currentScene = {};
    state.adventure.props.clear();
    state.adventure.effectSprites.clear();
    state.adventure.effectRegions.clear();
    state.adventure.camera = {};
    state.adventure.pendingInteraction = {};
    state.adventure.debugTrianglePath.clear();
    state.adventure.hasLastClickWorldPos = false;
    state.adventure.hasLastResolvedTargetPos = false;
    state.adventure.actors.clear();
    state.adventure.controlledActorIndex = -1;
    state.adventure.actorDefinitions.clear();
    state.adventure.dialogueUi = {};
    state.adventure.speechUi = {};
    state.adventure.ambientSpeechUis.clear();

    ClearSceneAudio(state);
    UnloadSceneResources(state.resources);
}

bool LoadSceneById(GameState& state, const char* sceneId, SceneLoadMode loadMode)
{
    if (state.adventure.currentScene.loaded) {
        ScriptSystemCallHook(state, "Scene_onExit");
        ScriptSystemShutdown(state.script);
    }

    UnloadCurrentScene(state);

    const fs::path sceneFilePath = fs::path(ASSETS_PATH "scenes") / sceneId / "scene.json";
    const std::string sceneFileNorm = NormalizePath(sceneFilePath);

    json root;
    {
        std::ifstream in(sceneFileNorm);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open scene file: %s", sceneFileNorm.c_str());
            return false;
        }
        in >> root;
    }

    SceneData scene;
    scene.sceneId = root.value("sceneId", std::string(sceneId));
    scene.saveName = root.value("saveName", scene.sceneId);
    scene.sceneFilePath = sceneFileNorm;
    scene.baseAssetScale = root.value("baseAssetScale", 1);
    scene.worldWidth = root.value("worldWidth", 1920.0f);
    scene.worldHeight = root.value("worldHeight", 1080.0f);

    if(!root.contains("script")) {
        TraceLog(LOG_ERROR, "Scene missing script: %s", sceneFileNorm.c_str());
        return false;
    }

    scene.script = root.value("script", "");
    if(scene.script.empty()) {
        TraceLog(LOG_ERROR, "Scene script attribute must not be empty: %s", sceneFileNorm.c_str());
        return false;
    }

    if (root.contains("playerSpawn")) {
        scene.playerSpawn.x = root["playerSpawn"].value("x", 0.0f);
        scene.playerSpawn.y = root["playerSpawn"].value("y", 0.0f);
    }

    if (root.contains("scale")) {
        const auto& s = root["scale"];
        scene.scaleConfig.nearY = s.value("nearY", 0.0f);
        scene.scaleConfig.farY = s.value("farY", 1080.0f);
        scene.scaleConfig.nearScale = s.value("nearScale", 1.0f);
        scene.scaleConfig.farScale = s.value("farScale", 1.0f);
    }

    const fs::path sceneDir = fs::path(sceneFileNorm).parent_path();

    const std::string tiledFileRel = root.value("tiledFile", "");
    if (tiledFileRel.empty()) {
        TraceLog(LOG_ERROR, "Scene missing tiledFile: %s", sceneFileNorm.c_str());
        return false;
    }

    const fs::path tiledPath = (sceneDir / tiledFileRel).lexically_normal();
    if (!ImportTiledSceneIntoSceneData(scene, state.resources, tiledPath.string().c_str())) {
        TraceLog(LOG_ERROR, "Failed to import Tiled scene: %s", tiledPath.string().c_str());
        return false;
    }

    if (!BuildNavMesh(scene.navMesh)) {
        TraceLog(LOG_ERROR, "Failed to build navmesh for scene: %s", scene.sceneId.c_str());
        return false;
    }

    const SceneSpawnPoint* chosenSpawn = nullptr;

    if (!state.adventure.pendingSpawnId.empty()) {
        chosenSpawn = FindSpawnById(scene, state.adventure.pendingSpawnId);
        if (chosenSpawn == nullptr) {
            TraceLog(LOG_WARNING,
                     "Requested spawn '%s' not found in scene '%s'",
                     state.adventure.pendingSpawnId.c_str(),
                     scene.sceneId.c_str());
        }
    }

    if (chosenSpawn == nullptr) {
        chosenSpawn = FindSpawnById(scene, "default");
    }

    scene.loaded = true;
    state.adventure.controlsEnabled = true;
    state.adventure.currentScene = scene;

    if (!LoadSceneAudioDefinitions(state, sceneDir.string())) {
        TraceLog(LOG_ERROR, "Failed to load scene audio definitions for scene: %s", scene.sceneId.c_str());
        return false;
    }

    BuildSceneSoundEmitters(state);

    state.adventure.props.clear();
    state.adventure.props.reserve(state.adventure.currentScene.props.size());

    for (int i = 0; i < static_cast<int>(state.adventure.currentScene.props.size()); ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];

        PropInstance prop;
        prop.handle = state.adventure.nextPropHandle++;
        prop.scenePropIndex = i;
        prop.feetPos = sceneProp.feetPos;
        prop.visible = sceneProp.visible;
        prop.flipX = sceneProp.flipX;
        prop.currentAnimation = sceneProp.defaultAnimation;
        prop.animationTimeMs = 0.0f;
        prop.oneShotActive = false;
        prop.oneShotDurationMs = 0.0f;
        prop.moveActive = false;
        prop.moveStartPos = prop.feetPos;
        prop.moveTargetPos = prop.feetPos;
        prop.moveElapsedMs = 0.0f;
        prop.moveDurationMs = 0.0f;
        prop.moveInterpolation = MoveInterpolation::Linear;

        state.adventure.props.push_back(prop);
    }

    state.adventure.effectSprites.clear();
    state.adventure.effectSprites.reserve(state.adventure.currentScene.effectSprites.size());

    for (int i = 0; i < static_cast<int>(state.adventure.currentScene.effectSprites.size()); ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];

        EffectSpriteInstance effect;
        effect.handle = state.adventure.nextEffectSpriteHandle++;
        effect.sceneEffectSpriteIndex = i;
        effect.visible = sceneEffect.visible;
        effect.opacity = sceneEffect.opacity;
        effect.tint = sceneEffect.tint;
        effect.shaderType = sceneEffect.shaderType;
        effect.shaderParams = sceneEffect.shaderParams;

        state.adventure.effectSprites.push_back(effect);
    }

    state.adventure.effectRegions.clear();
    state.adventure.effectRegions.reserve(state.adventure.currentScene.effectRegions.size());

    for (int i = 0; i < static_cast<int>(state.adventure.currentScene.effectRegions.size()); ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];

        EffectRegionInstance effect;
        effect.handle = state.adventure.nextEffectRegionHandle++;
        effect.sceneEffectRegionIndex = i;
        effect.visible = sceneEffect.visible;
        effect.opacity = sceneEffect.opacity;
        effect.tint = sceneEffect.tint;
        effect.shaderType = sceneEffect.shaderType;
        effect.shaderParams = sceneEffect.shaderParams;

        state.adventure.effectRegions.push_back(effect);
    }

    state.adventure.actors.clear();
    state.adventure.controlledActorIndex = -1;

    // Controlled actor: currently hardcoded to main_actor.
    {
        int controlledDefIndex = -1;
        if (!EnsureActorDefinitionLoaded(state, "template_actor", &controlledDefIndex)) {
            TraceLog(LOG_ERROR, "Failed to load controlled actor definition: main_actor");
            return false;
        }

        const ActorDefinitionData* controlledDef =
                FindActorDefinitionByIndex(state, controlledDefIndex);
        if (controlledDef == nullptr) {
            TraceLog(LOG_ERROR, "Controlled actor definition missing after load: main_actor");
            return false;
        }

        ActorInstance actor{};
        actor.handle = state.adventure.nextActorHandle++;
        InitializeActorInstanceFromDefinition(actor, *controlledDef);
        actor.actorDefIndex = controlledDefIndex;

        if (chosenSpawn != nullptr) {
            actor.feetPos = chosenSpawn->position;
            ApplyActorFacingFromSceneFacing(actor, chosenSpawn->facing);
        } else {
            actor.feetPos = scene.playerSpawn;
            ApplyActorFacingFromSceneFacing(actor, SceneFacing::Front);
        }

        state.adventure.actors.push_back(actor);
        state.adventure.controlledActorIndex = 0;
        if (controlledDef->controllable &&
            FindActorInventoryByActorId(state, actor.actorId) == nullptr) {
            ActorInventoryData inv;
            inv.actorId = actor.actorId;
            state.adventure.actorInventories.push_back(inv);
        }
    }

    // Scene NPC actors from Tiled placements.
    for (const SceneActorPlacement& placement : state.adventure.currentScene.actorPlacements) {
        if (placement.actorId == "main_actor") {
            TraceLog(LOG_WARNING,
                     "Ignoring actor placement for controlled actor '%s'; player uses scene spawns",
                     placement.actorId.c_str());
            continue;
        }

        int defIndex = -1;
        if (!EnsureActorDefinitionLoaded(state, placement.actorId, &defIndex)) {
            TraceLog(LOG_ERROR,
                     "Failed to load actor definition for scene actor: %s",
                     placement.actorId.c_str());
            continue;
        }

        const ActorDefinitionData* def = FindActorDefinitionByIndex(state, defIndex);
        if (def == nullptr) {
            TraceLog(LOG_ERROR,
                     "Missing actor definition after load for scene actor: %s",
                     placement.actorId.c_str());
            continue;
        }

        ActorInstance actor{};
        actor.handle = state.adventure.nextActorHandle++;
        InitializeActorInstanceFromDefinition(actor, *def);
        actor.actorDefIndex = defIndex;
        actor.feetPos = placement.position;
        actor.visible = placement.visible;
        actor.activeInScene = true;
        ApplyActorFacingFromSceneFacing(actor, placement.facing);

        state.adventure.actors.push_back(actor);
        if (def->controllable &&
            FindActorInventoryByActorId(state, actor.actorId) == nullptr) {
            ActorInventoryData inv;
            inv.actorId = actor.actorId;
            state.adventure.actorInventories.push_back(inv);
        }
    }

    state.adventure.camera = {};
    state.adventure.camera.viewportWidth = 1920.0f;
    state.adventure.camera.viewportHeight = 1080.0f;
    state.adventure.camera.position = { 0.0f, 0.0f };
    state.adventure.camera.biasLatch = CameraBiasLatch::None;
    state.adventure.camera.currentBiasShiftX = 0.0f;

    const ActorInstance* controlledActor = GetControlledActor(state);
    if (controlledActor != nullptr) {
        state.adventure.camera.position =
                GetImmediateCenteredCameraPosition(state, *controlledActor);
    }

    ScriptSystemInit(state);

    const fs::path scriptPath = (sceneDir / scene.script).lexically_normal();
    TraceLog(LOG_INFO, "Running scene script: %s", scriptPath.string().c_str());
    if (!ScriptSystemRunFile(state.script, scriptPath.string())) {
        TraceLog(LOG_ERROR, "Failed to run scene script: %s", scriptPath.string().c_str());
        return false;
    }

    ScriptSystemCallHook(state, "Scene_onEnter");

    TraceLog(LOG_INFO, "Loaded scene: %s", scene.sceneId.c_str());
    TraceLog(LOG_INFO,
             "Navmesh built: vertices=%d triangles=%d",
             static_cast<int>(scene.navMesh.vertices.size()),
             static_cast<int>(scene.navMesh.triangles.size()));

    return true;
}
