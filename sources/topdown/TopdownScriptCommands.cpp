#include "topdown/TopdownScriptCommands.h"

#include <cmath>

#include "nav/NavMeshQuery.h"
#include "raymath.h"
#include "topdown/NpcRegistry.h"
#include "topdown/TopdownNpcPatrol.h"
#include "audio/Audio.h"
#include "ui/NarrationPopups.h"

static TopdownRuntimeImageLayer* FindLayer(GameState& state, const std::string& name)
{
    for (auto& l : state.topdown.runtime.render.bottomLayers) {
        if (state.topdown.authored.imageLayers[l.authoredIndex].name == name) return &l;
    }
    for (auto& l : state.topdown.runtime.render.topLayers) {
        if (state.topdown.authored.imageLayers[l.authoredIndex].name == name) return &l;
    }
    return nullptr;
}

static TopdownRuntimeEffectRegion* FindEffect(GameState& state, const std::string& id)
{
    for (auto& e : state.topdown.runtime.render.effectRegions) {
        if (state.topdown.authored.effectRegions[e.authoredIndex].id == id) return &e;
    }
    return nullptr;
}

static TopdownRuntimeTrigger* FindTrigger(GameState& state, const std::string& id)
{
    for (TopdownRuntimeTrigger& trigger : state.topdown.runtime.triggers) {
        if (trigger.authoredIndex < 0 ||
            trigger.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
            continue;
        }

        if (state.topdown.authored.triggers[trigger.authoredIndex].id == id) {
            return &trigger;
        }
    }

    return nullptr;
}

static TopdownNpcRuntime* FindNpc(GameState& state, const std::string& npcId)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.active && npc.id == npcId) {
            return &npc;
        }
    }
    return nullptr;
}

static const TopdownAuthoredSpawn* FindSpawn(GameState& state, const std::string& spawnId)
{
    for (const TopdownAuthoredSpawn& spawn : state.topdown.authored.spawns) {
        if (spawn.id == spawnId) {
            return &spawn;
        }
    }
    return nullptr;
}

static bool BuildPath(
        const NavMeshData& navMesh,
        Vector2 start,
        Vector2 target,
        std::vector<Vector2>& outPathPoints)
{
    std::vector<int> trianglePath;
    Vector2 resolvedEnd{};

    outPathPoints.clear();

    const bool ok = BuildNavPath(
            navMesh,
            start,
            target,
            outPathPoints,
            &trianglePath,
            &resolvedEnd);

    return ok && !outPathPoints.empty();
}

static bool StartPlayerPathMove(GameState& state, Vector2 target, bool running)
{
    if (!state.topdown.runtime.levelActive) {
        return false;
    }

    const NavMeshData& navMesh = state.topdown.runtime.nav.navMesh;
    if (!navMesh.built) {
        return false;
    }

    std::vector<Vector2> pathPoints;
    if (!BuildPath(navMesh, state.topdown.runtime.player.position, target, pathPoints)) {
        state.topdown.runtime.scriptedMove = {};
        return false;
    }

    TopdownScriptMoveState& move = state.topdown.runtime.scriptedMove;
    move = {};
    move.active = true;
    move.running = running;
    move.pathPoints = pathPoints;
    move.currentPoint = 0;
    move.currentSpeed = 0.0f;
    move.acceleration = 1800.0f;
    move.deceleration = 2200.0f;
    move.arrivalRadius = 6.0f;
    move.stopDistance = running ? 180.0f : 120.0f;

    state.topdown.runtime.player.velocity = Vector2{};
    state.topdown.runtime.player.desiredVelocity = Vector2{};
    state.topdown.runtime.player.moveInputForward = 0.0f;
    state.topdown.runtime.player.moveInputRight = 0.0f;
    state.topdown.runtime.player.wantsRun = running;

    return true;
}

static bool StartNpcPathMove(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 target,
        bool running)
{
    if (!state.topdown.runtime.levelActive) {
        return false;
    }

    const NavMeshData& navMesh = state.topdown.runtime.nav.navMesh;
    if (!navMesh.built) {
        return false;
    }

    std::vector<Vector2> pathPoints;
    if (!BuildPath(navMesh, npc.position, target, pathPoints)) {
        npc.move = {};
        npc.moving = false;
        npc.running = false;
        return false;
    }

    // Clear any ongoing patrol
    TopdownClearNpcPatrol(state, npc);

    npc.move = {};
    npc.move.active = true;
    npc.move.owner = TopdownNpcMoveOwner::ScriptCommand;
    npc.move.running = running;
    npc.move.pathPoints = pathPoints;
    npc.move.currentPoint = 0;
    npc.move.currentSpeed = 0.0f;
    npc.move.acceleration = 1800.0f;
    npc.move.deceleration = 2200.0f;
    npc.move.arrivalRadius = 6.0f;
    npc.move.stopDistance = running ? 180.0f : 120.0f;

    npc.moving = true;
    npc.running = running;

    return true;
}

// --------------------------------------------------
// Controls
// --------------------------------------------------

bool TopdownScriptSetControlsEnabled(GameState& state, bool enabled)
{
    state.topdown.runtime.controlsEnabled = enabled;

    if (!enabled) {
        state.topdown.runtime.player.desiredVelocity = Vector2{};
        state.topdown.runtime.player.moveInputForward = 0.0f;
        state.topdown.runtime.player.moveInputRight = 0.0f;
        state.topdown.runtime.player.wantsRun = false;
    }

    return true;
}

bool TopdownScriptShowNarration(
        GameState& state,
        const std::string& title,
        const std::string& body,
        float durationSeconds)
{
    return TopdownQueueNarrationPopup(state, title, body, durationSeconds);
}

bool TopdownScriptEnableScriptCamera(GameState& state)
{
    TopdownCameraRuntime& cameraRuntime = state.topdown.runtime.camera;
    const TopdownCameraData& camera = state.topdown.camera;

    cameraRuntime.mode = TopdownCameraMode::Scripted;
    cameraRuntime.scriptedTarget = Vector2{
            cameraRuntime.position.x + camera.viewportWidth * 0.5f,
            cameraRuntime.position.y + camera.viewportHeight * 0.5f
    };
    cameraRuntime.isPanning = false;
    return true;
}

bool TopdownScriptDisableScriptCamera(GameState& state)
{
    state.topdown.runtime.camera.mode = TopdownCameraMode::Player;
    state.topdown.runtime.camera.isPanning = false;
    state.topdown.runtime.camera.aimOffset = Vector2{};
    return true;
}

bool TopdownScriptSetCameraTarget(GameState& state, const std::string& spawnId)
{
    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        return false;
    }

    TopdownCameraRuntime& camera = state.topdown.runtime.camera;
    camera.scriptedTarget = spawn->position;
    camera.isPanning = false;
    return true;
}

bool TopdownScriptPanCameraTarget(GameState& state, const std::string& spawnId, float durationMs)
{
    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        return false;
    }

    TopdownCameraRuntime& cameraRuntime = state.topdown.runtime.camera;
    const TopdownCameraData& camera = state.topdown.camera;

    cameraRuntime.panStart = Vector2{
            cameraRuntime.position.x + camera.viewportWidth * 0.5f,
            cameraRuntime.position.y + camera.viewportHeight * 0.5f};
    cameraRuntime.panEnd = spawn->position;
    cameraRuntime.panDurationMs = durationMs;
    cameraRuntime.panTimerMs = 0.0f;
    cameraRuntime.isPanning = true;
    return true;
}

// --------------------------------------------------
// Scripted player movement
// --------------------------------------------------

bool TopdownScriptStartWalkTo(GameState& state, Vector2 target)
{
    return StartPlayerPathMove(state, target, false);
}

bool TopdownScriptStartRunTo(GameState& state, Vector2 target)
{
    return StartPlayerPathMove(state, target, true);
}

bool TopdownScriptStartWalkToSpawn(GameState& state, const std::string& spawnId)
{
    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        return false;
    }

    return StartPlayerPathMove(state, spawn->position, false);
}

bool TopdownScriptStartRunToSpawn(GameState& state, const std::string& spawnId)
{
    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        return false;
    }

    return StartPlayerPathMove(state, spawn->position, true);
}

// --------------------------------------------------
// NPC lifecycle
// --------------------------------------------------

bool TopdownScriptSpawnNpc(
        GameState& state,
        const std::string& npcId,
        const std::string& assetId,
        const std::string& spawnId,
        bool persistentChase)
{
    if (npcId.empty() || assetId.empty() || spawnId.empty()) {
        return false;
    }

    if (FindNpc(state, npcId) != nullptr) {
        TraceLog(LOG_WARNING, "NPC with id '%s' already exists", npcId.c_str());
        return false;
    }

    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        TraceLog(LOG_WARNING, "Spawn '%s' not found for NPC '%s'", spawnId.c_str(), npcId.c_str());
        return false;
    }

    return TopdownSpawnNpcRuntime(
            state,
            npcId,
            assetId,
            spawn->position,
            spawn->orientationDegrees,
            true,
            persistentChase);
}

bool TopdownScriptSpawnNpcSmart(
        GameState& state,
        const std::string& npcId,
        const std::string& assetId,
        const std::string& spawnId,
        bool persistentChase)
{
    if (npcId.empty() || assetId.empty() || spawnId.empty()) {
        return false;
    }

    if (FindNpc(state, npcId) != nullptr) {
        TraceLog(LOG_WARNING, "NPC with id '%s' already exists", npcId.c_str());
        return false;
    }

    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        TraceLog(LOG_WARNING, "Spawn '%s' not found for NPC '%s'", spawnId.c_str(), npcId.c_str());
        return false;
    }

    return TopdownSpawnNpcRuntime(
            state,
            npcId,
            assetId,
            spawn->position,
            spawn->orientationDegrees,
            true,
            persistentChase,
            true);
}

bool TopdownScriptRemoveNpc(GameState& state, const std::string& npcId)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.active && npc.id == npcId) {
            npc.active = false;
            npc.visible = false;
            npc.move = {};
            npc.moving = false;
            npc.running = false;
            return true;
        }
    }

    return false;
}

// --------------------------------------------------
// NPC scripted movement
// --------------------------------------------------

bool TopdownScriptStartWalkNpcTo(GameState& state, const std::string& npcId, Vector2 target)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    return StartNpcPathMove(state, *npc, target, false);
}

bool TopdownScriptStartRunNpcTo(GameState& state, const std::string& npcId, Vector2 target)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    return StartNpcPathMove(state, *npc, target, true);
}

bool TopdownScriptStartWalkNpcToSpawn(GameState& state, const std::string& npcId, const std::string& spawnId)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        return false;
    }

    return StartNpcPathMove(state, *npc, spawn->position, false);
}

bool TopdownScriptStartRunNpcToSpawn(GameState& state, const std::string& npcId, const std::string& spawnId)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    const TopdownAuthoredSpawn* spawn = FindSpawn(state, spawnId);
    if (spawn == nullptr) {
        return false;
    }

    return StartNpcPathMove(state, *npc, spawn->position, true);
}

bool TopdownScriptAssignNpcPatrolRoute(
        GameState& state,
        const std::string& npcId,
        const std::vector<std::string>& spawnIds,
        bool loop,
        bool running,
        float waitMs)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        TraceLog(LOG_WARNING, "assignNpcPatrolRoute: npc '%s' not found", npcId.c_str());
        return false;
    }

    TopdownNpcPatrolRouteOptions options;
    options.loop = loop;
    options.running = running;
    options.waitMs = waitMs;

    return TopdownAssignNpcPatrolRoute(state, *npc, spawnIds, options);
}

bool TopdownScriptClearNpcPatrol(GameState& state, const std::string& npcId)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        TraceLog(LOG_WARNING, "clearNpcPatrol: npc '%s' not found", npcId.c_str());
        return false;
    }

    TopdownClearNpcPatrol(state, *npc);
    return true;
}

bool TopdownScriptPauseNpcPatrol(GameState& state, const std::string& npcId)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        TraceLog(LOG_WARNING, "pauseNpcPatrol: npc '%s' not found", npcId.c_str());
        return false;
    }

    return TopdownPauseNpcPatrol(state, *npc);
}

bool TopdownScriptResumeNpcPatrol(GameState& state, const std::string& npcId)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        TraceLog(LOG_WARNING, "resumeNpcPatrol: npc '%s' not found", npcId.c_str());
        return false;
    }

    return TopdownResumeNpcPatrol(*npc);
}

// --------------------------------------------------
// NPC animation
// --------------------------------------------------

bool TopdownScriptSetNpcAnimation(GameState& state, const std::string& npcId, const std::string& animationName)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    const TopdownNpcAssetRuntime* asset = FindTopdownNpcAssetRuntime(state, npc->assetId);
    if (asset == nullptr || !asset->loaded) {
        return false;
    }

    const TopdownNpcClipRef clipRef = FindTopdownNpcClipByName(state, *asset, animationName);
    if (!TopdownNpcClipRefIsValid(clipRef)) {
        TraceLog(LOG_WARNING,
                 "NPC '%s' asset '%s' does not have animation '%s'",
                 npcId.c_str(),
                 npc->assetId.c_str(),
                 animationName.c_str());
        return false;
    }

    TopdownSetNpcScriptLoopAnimation(*npc, clipRef);
    return true;
}

bool TopdownScriptClearNpcAnimation(GameState& state, const std::string& npcId)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    TopdownClearNpcScriptLoopAnimation(*npc);
    return true;
}

bool TopdownScriptPlayNpcAnimation(GameState& state, const std::string& npcId, const std::string& animationName)
{
    TopdownNpcRuntime* npc = FindNpc(state, npcId);
    if (npc == nullptr) {
        return false;
    }

    const TopdownNpcAssetRuntime* asset = FindTopdownNpcAssetRuntime(state, npc->assetId);
    if (asset == nullptr || !asset->loaded) {
        return false;
    }

    const TopdownNpcClipRef clipRef = FindTopdownNpcClipByName(state, *asset, animationName);
    if (!TopdownNpcClipRefIsValid(clipRef)) {
        TraceLog(LOG_WARNING,
                 "NPC '%s' asset '%s' does not have animation '%s'",
                 npcId.c_str(),
                 npc->assetId.c_str(),
                 animationName.c_str());
        return false;
    }

    TopdownPlayNpcOneShotAnimation(*npc, clipRef);
    return true;
}

// --------------------------------------------------
// Image layers
// --------------------------------------------------

bool TopdownScriptSetImageLayerVisible(GameState& state, const std::string& name, bool visible)
{
    if (auto* l = FindLayer(state, name)) {
        l->visible = visible;
        return true;
    }
    return false;
}

bool TopdownScriptIsImageLayerVisible(GameState& state, const std::string& name, bool& outVisible)
{
    if (auto* l = FindLayer(state, name)) {
        outVisible = l->visible;
        return true;
    }
    return false;
}

bool TopdownScriptSetImageLayerOpacity(GameState& state, const std::string& name, float opacity)
{
    if (auto* l = FindLayer(state, name)) {
        l->opacity = opacity;
        return true;
    }
    return false;
}

bool TopdownScriptGetImageLayerOpacity(GameState& state, const std::string& name, float& outOpacity)
{
    if (auto* l = FindLayer(state, name)) {
        outOpacity = l->opacity;
        return true;
    }
    return false;
}

// --------------------------------------------------
// Effect regions
// --------------------------------------------------

bool TopdownScriptSetEffectRegionVisible(GameState& state, const std::string& id, bool visible)
{
    if (auto* e = FindEffect(state, id)) {
        e->visible = visible;
        return true;
    }
    return false;
}

bool TopdownScriptIsEffectRegionVisible(GameState& state, const std::string& id, bool& outVisible)
{
    if (auto* e = FindEffect(state, id)) {
        outVisible = e->visible;
        return true;
    }
    return false;
}

bool TopdownScriptSetEffectRegionOpacity(GameState& state, const std::string& id, float opacity)
{
    if (auto* e = FindEffect(state, id)) {
        e->opacity = opacity;
        return true;
    }
    return false;
}

bool TopdownScriptGetEffectRegionOpacity(GameState& state, const std::string& id, float& outOpacity)
{
    if (auto* e = FindEffect(state, id)) {
        outOpacity = e->opacity;
        return true;
    }
    return false;
}

// --------------------------------------------------
// Triggers
// --------------------------------------------------

bool TopdownScriptSetTriggerEnabled(GameState& state, const std::string& triggerId, bool enabled)
{
    TopdownRuntimeTrigger* trigger = FindTrigger(state, triggerId);
    if (trigger == nullptr) {
        return false;
    }

    trigger->enabled = enabled;
    if (!enabled) {
        trigger->playerInside = false;
        trigger->npcHandlesInside.clear();
        trigger->pendingCalls.clear();
    } else {
        trigger->fired = false;
    }

    return true;
}

bool TopdownScriptSetTriggerRepeat(GameState& state, const std::string& triggerId, bool repeat)
{
    TopdownRuntimeTrigger* trigger = FindTrigger(state, triggerId);
    if (trigger == nullptr) {
        return false;
    }

    trigger->repeat = repeat;
    return true;
}

// Audio

static int FindSoundEmitterIndexById(const GameState& state, const std::string& emitterId)
{
    for (int i = 0; i < static_cast<int>(state.audio.levelEmitters.size()); ++i) {
        if (state.audio.levelEmitters[i].id == emitterId) {
            return i;
        }
    }

    return -1;
}

bool TopdownScriptPlaySound(GameState& state, const std::string& audioId)
{
    if (audioId.empty()) {
        return false;
    }

    return PlaySoundById(state, audioId);
}

bool TopdownScriptStopSound(GameState& state, const std::string& audioId)
{
    if (audioId.empty()) {
        return false;
    }
    return StopSoundById(state, audioId);
}

bool TopdownScriptPlayMusic(GameState& state, const std::string& audioId, float fadeMs)
{
    if (audioId.empty()) {
        return false;
    }

    return PlayMusicById(state, audioId, fadeMs);
}

bool TopdownScriptStopMusic(GameState& state, float fadeMs)
{
    StopMusic(state, fadeMs);
    return true;
}

bool TopdownScriptSetSoundEmitterEnabled(GameState& state, const std::string& emitterId, bool enabled)
{
    const int emitterIndex = FindSoundEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.levelEmitters.size())) {
        return false;
    }

    state.audio.levelEmitters[emitterIndex].enabled = enabled;
    return true;
}

bool TopdownScriptGetSoundEmitterEnabled(const GameState& state, const std::string& emitterId, bool& outEnabled)
{
    const int emitterIndex = FindSoundEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.levelEmitters.size())) {
        return false;
    }

    outEnabled = state.audio.levelEmitters[emitterIndex].enabled;
    return true;
}

bool TopdownScriptSetSoundEmitterVolume(GameState& state, const std::string& emitterId, float volume)
{
    const int emitterIndex = FindSoundEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.levelEmitters.size())) {
        return false;
    }

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    state.audio.levelEmitters[emitterIndex].volume = volume;
    return true;
}

bool TopdownScriptPlayEmitter(GameState& state, const std::string& emitterId)
{
    if (emitterId.empty()) {
        return false;
    }

    return PlaySoundEmitterById(state, emitterId);
}

bool TopdownScriptStopEmitter(GameState& state, const std::string& emitterId)
{
    if (emitterId.empty()) {
        return false;
    }

    return StopSoundEmitterById(state, emitterId);
}



bool TopdownScriptShakeScreen(GameState& state,
                                float durationMs,
                                float strengthPx,
                                float frequencyHz,
                                bool smooth)
{
    if (durationMs <= 0.0f) {
        return false;
    }

    if (strengthPx <= 0.0f) {
        return false;
    }

    if (frequencyHz <= 0.0f) {
        frequencyHz = 30.0f;
    }

    TopdownScreenShakeState& shake = state.topdown.runtime.screenShake;

    if (!shake.active) {
        shake.active = true;
        shake.durationMs = durationMs;
        shake.elapsedMs = 0.0f;
        shake.strengthX = strengthPx;
        shake.strengthY = strengthPx;
        shake.frequencyHz = frequencyHz;
        shake.sampleTimerMs = 0.0f;
        shake.smooth = smooth;
        shake.previousOffset = Vector2{0.0f, 0.0f};
        shake.sampledOffset = Vector2{0.0f, 0.0f};
        shake.currentOffset = Vector2{0.0f, 0.0f};
        return true;
    }

    shake.active = true;
    shake.durationMs = durationMs;
    shake.elapsedMs = 0.0f;
    shake.strengthX = std::max(shake.strengthX, strengthPx);
    shake.strengthY = std::max(shake.strengthY, strengthPx);
    shake.frequencyHz = std::max(shake.frequencyHz, frequencyHz);
    shake.sampleTimerMs = 0.0f;
    shake.smooth = smooth;

    return true;
}
