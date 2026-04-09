#include "TopdownRvo.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "topdown/TopdownHelpers.h"
#include "topdown/NpcRegistry.h"

#include "rvo2/RVO.h"
#include "rvo2/Vector2.h"

static Vector2 ToRay(const RVO::Vector2& v)
{
    return Vector2{ v.x(), v.y() };
}

static RVO::Vector2 ToRvo(Vector2 v)
{
    return RVO::Vector2(v.x, v.y);
}

static TopdownNpcRuntime* FindNpcByHandle(GameState& state, int handle)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.handle == handle) {
            return &npc;
        }
    }

    return nullptr;
}

static const TopdownNpcAssetRuntime* FindNpcAssetForRvo(
        GameState& state,
        const TopdownNpcRuntime& npc)
{
    return FindTopdownNpcAssetRuntime(state, npc.assetId);
}

static bool FindRvoAgentIdByHandle(
        const TopdownRvoState& rvo,
        int npcHandle,
        size_t& outRvoId)
{
    for (const TopdownRvoAgent& agent : rvo.agents) {
        if (agent.npcHandle == npcHandle) {
            outRvoId = agent.rvoId;
            return true;
        }
    }

    return false;
}

static void DestroySimulator(TopdownRvoState& rvo)
{
    if (rvo.sim != nullptr) {
        delete rvo.sim;
        rvo.sim = nullptr;
    }

    rvo.agents.clear();
    rvo.obstaclesBuilt = false;

    // Player is intentionally NOT an RVO agent anymore.
    rvo.hasPlayerAgent = false;
    rvo.playerRvoId = RVO::RVO_ERROR;
}

static void ConfigureSimulatorDefaults(RVO::RVOSimulator& sim)
{
    sim.setTimeStep(1.0f / 60.0f);
    sim.setAgentDefaults(
            180.0f, // neighborDist
            12,     // maxNeighbors
            0.8f,   // timeHorizon
            0.8f,   // timeHorizonObst
            32.0f,  // radius
            700.0f  // maxSpeed
    );
}

static void CreateSimulator(TopdownRvoState& rvo)
{
    DestroySimulator(rvo);

    rvo.sim = new RVO::RVOSimulator();
    ConfigureSimulatorDefaults(*rvo.sim);
    rvo.initialized = true;
}

static std::vector<RVO::Vector2> BuildRvoObstacleVertices(
        const std::vector<Vector2>& polygon)
{
    std::vector<RVO::Vector2> out;
    out.reserve(polygon.size());

    for (auto it = polygon.rbegin(); it != polygon.rend(); ++it) {
        out.push_back(ToRvo(*it));
    }

    return out;
}

static void BuildStaticObstacles(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized || rvo.sim == nullptr) {
        return;
    }

    RVO::RVOSimulator& sim = *rvo.sim;

    if (state.topdown.authored.levelBoundary.size() >= 3) {
        std::vector<RVO::Vector2> verts =
                BuildRvoObstacleVertices(state.topdown.authored.levelBoundary);

        if (verts.size() >= 2) {
            sim.addObstacle(verts);
        }
    }

    for (const TopdownAuthoredPolygon& obstacle : state.topdown.authored.obstacles) {
        if (obstacle.points.size() < 3) {
            continue;
        }

        std::vector<RVO::Vector2> verts =
                BuildRvoObstacleVertices(obstacle.points);

        if (verts.size() >= 2) {
            sim.addObstacle(verts);
        }
    }

    sim.processObstacles();
    rvo.obstaclesBuilt = true;
}

static void AddNpcAgents(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;
    if (!rvo.initialized || rvo.sim == nullptr) {
        return;
    }

    rvo.agents.clear();
    rvo.agents.reserve(state.topdown.runtime.npcs.size());

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || npc.dead || npc.corpse) {
            continue;
        }

        const size_t id = rvo.sim->addAgent(ToRvo(npc.position));
        if (id == RVO::RVO_ERROR) {
            continue;
        }

        rvo.sim->setAgentRadius(id, npc.collisionRadius);

        TopdownRvoAgent agent;
        agent.npcHandle = npc.handle;
        agent.rvoId = id;
        rvo.agents.push_back(agent);
    }
}

static void RebuildSimulator(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized) {
        TopdownRvoInit(state);
    }

    CreateSimulator(rvo);
    BuildStaticObstacles(state);
    AddNpcAgents(state);

    // Player is intentionally NOT in the sim.
    rvo.hasPlayerAgent = false;
    rvo.playerRvoId = RVO::RVO_ERROR;

    rvo.rebuildRequested = false;
}

static Vector2 BuildNpcFallbackPreferredVelocity(
        GameState& state,
        const TopdownNpcRuntime& npc)
{
    if (!npc.move.active ||
        npc.move.currentPoint < 0 ||
        npc.move.currentPoint >= static_cast<int>(npc.move.pathPoints.size())) {
        return Vector2{};
    }

    const Vector2 target = npc.move.pathPoints[npc.move.currentPoint];
    Vector2 toTarget = TopdownSub(target, npc.position);
    const float dist = TopdownLength(toTarget);

    if (dist <= std::max(1.0f, npc.move.arrivalRadius)) {
        return Vector2{};
    }

    const Vector2 dir = TopdownMul(toTarget, 1.0f / dist);

    const TopdownNpcAssetRuntime* asset = FindNpcAssetForRvo(state, npc);
    const float walkSpeed = asset ? asset->walkSpeed : 450.0f;
    const float runSpeed = asset ? asset->runSpeed : 700.0f;
    const float maxSpeed = npc.move.running ? runSpeed : walkSpeed;

    const float desiredSpeed = std::min(
            std::max(0.0f, npc.move.currentSpeed),
            maxSpeed);

    return TopdownMul(dir, desiredSpeed);
}

void TopdownRvoInit(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (rvo.initialized && rvo.sim != nullptr) {
        return;
    }

    CreateSimulator(rvo);
}

void TopdownRvoShutdown(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    DestroySimulator(rvo);
    rvo.initialized = false;
    rvo.rebuildRequested = false;
}

void TopdownRvoRequestRebuild(GameState& state)
{
    state.topdown.runtime.rvo.rebuildRequested = true;
}

void TopdownRvoEnsureReady(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized || rvo.sim == nullptr) {
        TopdownRvoInit(state);
        rvo.rebuildRequested = true;
    }

    if (rvo.rebuildRequested) {
        RebuildSimulator(state);
    }
}

void TopdownRvoSync(GameState& state)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized || rvo.sim == nullptr) {
        return;
    }

    RVO::RVOSimulator& sim = *rvo.sim;

    for (const TopdownRvoAgent& agent : rvo.agents) {
        TopdownNpcRuntime* npc = FindNpcByHandle(state, agent.npcHandle);
        if (npc == nullptr) {
            continue;
        }

        sim.setAgentPosition(agent.rvoId, ToRvo(npc->position));
        sim.setAgentRadius(agent.rvoId, npc->collisionRadius);

        if (!npc->active ||
            npc->dead ||
            npc->corpse ||
            npc->hurtStunRemainingMs > 0.0f ||
            TopdownLengthSqr(npc->knockbackVelocity) > 0.000001f ||
            !npc->move.active) {
            sim.setAgentVelocity(agent.rvoId, RVO::Vector2(0.0f, 0.0f));
            sim.setAgentPrefVelocity(agent.rvoId, RVO::Vector2(0.0f, 0.0f));
            sim.setAgentMaxSpeed(agent.rvoId, 0.0f);
            continue;
        }

        const TopdownNpcAssetRuntime* asset = FindNpcAssetForRvo(state, *npc);
        const float walkSpeed = asset ? asset->walkSpeed : 450.0f;
        const float runSpeed = asset ? asset->runSpeed : 700.0f;
        const float maxSpeed = npc->move.running ? runSpeed : walkSpeed;

        Vector2 pref = BuildNpcFallbackPreferredVelocity(state, *npc);

        sim.setAgentVelocity(agent.rvoId, ToRvo(npc->currentVelocity));
        sim.setAgentPrefVelocity(agent.rvoId, ToRvo(pref));
        sim.setAgentMaxSpeed(agent.rvoId, maxSpeed);
    }
}

void TopdownRvoStep(GameState& state, float dt)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized || rvo.sim == nullptr) {
        return;
    }

    if (dt <= 0.0f) {
        return;
    }

    rvo.sim->setTimeStep(dt);
    rvo.sim->doStep();
}

bool TopdownRvoHasAgent(GameState& state, int npcHandle)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized || rvo.sim == nullptr) {
        return false;
    }

    size_t dummy = RVO::RVO_ERROR;
    return FindRvoAgentIdByHandle(rvo, npcHandle, dummy);
}

Vector2 TopdownRvoGetVelocity(GameState& state, int npcHandle)
{
    TopdownRvoState& rvo = state.topdown.runtime.rvo;

    if (!rvo.initialized || rvo.sim == nullptr) {
        return Vector2{};
    }

    size_t rvoId = RVO::RVO_ERROR;
    if (!FindRvoAgentIdByHandle(rvo, npcHandle, rvoId)) {
        return Vector2{};
    }

    return ToRay(rvo.sim->getAgentVelocity(rvoId));
}
