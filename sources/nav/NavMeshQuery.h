#pragma once

#include <vector>
#include "raylib.h"
#include "nav/NavMeshData.h"

int FindTriangleContainingPoint(const NavMeshData& navMesh, Vector2 p);

Vector2 ProjectPointToNavMesh(
        const NavMeshData& navMesh,
        Vector2 p,
        int* outTriangleIndex);

bool BuildNavPath(
        const NavMeshData& navMesh,
        Vector2 startPos,
        Vector2 endPos,
        std::vector<Vector2>& outPathPoints,
        std::vector<int>* outTrianglePath,
        Vector2* outResolvedEndPos);
