#pragma once

#include <vector>

#include "raylib.h"
#include "nav/NavMeshData.h"
#include "rvo2/RVOSimulator.h"

struct TopdownNavWorld {
    bool valid = false;

    std::vector<Vector2> levelBoundary;
    std::vector<std::vector<Vector2>> holePolygons;

    NavMeshData navMesh;
    float agentRadius = 0.0f;
};

struct TopdownRvoAgent {
    int npcHandle = -1;
    size_t rvoId = RVO::RVO_ERROR;
};

struct TopdownRvoState {
    bool initialized = false;
    RVO::RVOSimulator* sim = nullptr;

    std::vector<TopdownRvoAgent> agents;

    bool obstaclesBuilt = false;
    bool rebuildRequested = false;

    bool hasPlayerAgent = false;
    size_t playerRvoId = RVO::RVO_ERROR;
};
