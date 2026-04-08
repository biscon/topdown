#pragma once

#include <vector>
#include "raylib.h"

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
    // Authored walkable polygons from Tiled
    std::vector<NavPolygon> sourcePolygons;

    // Authored blockers from Tiled
    std::vector<NavPolygon> blockerPolygons;

    // Final triangulated runtime mesh
    std::vector<Vector2> vertices;
    std::vector<NavTriangle> triangles;

    bool built = false;
};
