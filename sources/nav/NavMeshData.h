#pragma once

#include <memory>
#include <vector>

#include "raylib.h"
#include "detour/DetourNavMesh.h"
#include "detour/DetourNavMeshQuery.h"

struct NavPolygon {
    std::vector<Vector2> vertices;
};

struct NavTriangle {
    int vertexIndex0 = -1;
    int vertexIndex1 = -1;
    int vertexIndex2 = -1;

    int neighbor0 = -1;
    int neighbor1 = -1;
    int neighbor2 = -1;

    Vector2 centroid{};
    Rectangle bounds{};
};

struct NavMeshData {
    // Authored walkable polygons from Tiled.
    std::vector<NavPolygon> sourcePolygons;

    // Authored blockers from Tiled.
    std::vector<NavPolygon> blockerPolygons;

    // Debug/inspection mesh generated from the final Recast poly mesh.
    // We build the Recast poly mesh with nvp=3, so these are triangles.
    std::vector<Vector2> vertices;
    std::vector<NavTriangle> triangles;

    // Runtime Detour navmesh + query.
    std::shared_ptr<dtNavMesh> detourNavMesh;
    std::shared_ptr<dtNavMeshQuery> detourQuery;

    bool built = false;
};