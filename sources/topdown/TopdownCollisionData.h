#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "topdown/TopdownCoreData.h"
#include "topdown/TopdownLevelObjectData.h"

struct TopdownRuntimeObstacle {
    TopdownObstacleHandle handle = -1;
    int tiledObjectId = -1;
    TopdownObstacleKind kind = TopdownObstacleKind::MovementAndVision;
    std::string name;

    std::vector<Vector2> polygon;
    std::vector<TopdownSegment> edges;

    Rectangle bounds{};
    bool visible = true;
};

struct TopdownCollisionSegmentGridCell {
    std::vector<int> segmentIndices;
};

struct TopdownCollisionSegmentGrid {
    bool built = false;
    Vector2 origin{};
    float cellSize = 256.0f;
    int width = 0;
    int height = 0;
    std::vector<TopdownCollisionSegmentGridCell> cells;
};

struct TopdownCollisionWorld {
    std::vector<TopdownRuntimeObstacle> obstacles;

    std::vector<TopdownSegment> movementSegments;
    std::vector<Rectangle> movementSegmentBounds;
    std::vector<TopdownSegment> visionSegments;
    std::vector<TopdownSegment> boundarySegments;

    TopdownCollisionSegmentGrid movementSegmentGrid;

    int nextObstacleHandle = 1;
};
