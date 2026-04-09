#include "NavMeshBuild.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "clipper2/clipper.h"
#include "utils/earcut.h"
#include "raylib.h"

#include "recast/Recast.h"
#include "detour/DetourAlloc.h"
#include "detour/DetourNavMesh.h"
#include "detour/DetourNavMeshBuilder.h"
#include "detour/DetourNavMeshQuery.h"

using namespace Clipper2Lib;

namespace
{
    constexpr double CLIPPER_SCALE = 1000.0;

    // Recast config.
    constexpr float kCellSize = 12.0f;
    constexpr float kCellHeight = 4.0f;

    constexpr float kWalkableHeight = 2.0f;
    constexpr float kWalkableClimb = 0.0f;
    constexpr float kWalkableSlopeAngle = 45.0f;

    constexpr float kMaxSimplificationError = 3.0f;
    constexpr int   kMaxEdgeLenCells = 32;
    constexpr int   kMinRegionAreaCells = 8;
    constexpr int   kMergeRegionAreaCells = 20;
    constexpr int   kMaxVertsPerPoly = 6;

    constexpr float kDetailSampleDist = 0.0f;
    constexpr float kDetailSampleMaxError = 1.0f;
}

struct EdgeOwner {
    int triangleIndex = -1;
    int edgeSlot = -1;
};

struct EdgeKey {
    int a = -1;
    int b = -1;

    bool operator==(const EdgeKey& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const
    {
        const uint64_t ua = static_cast<uint32_t>(k.a);
        const uint64_t ub = static_cast<uint32_t>(k.b);
        return static_cast<size_t>((ua << 32u) ^ ub);
    }
};

struct TriangulationPolygon {
    Path64 outer;
    std::vector<Path64> holes;
};

struct RecastBuildContext : rcContext {
    RecastBuildContext()
            : rcContext(true)
    {
    }

protected:
    void doLog(const rcLogCategory category, const char* msg, const int /*len*/) override
    {
        switch (category) {
            case RC_LOG_ERROR:   TraceLog(LOG_ERROR,   "Recast: %s", msg); break;
            case RC_LOG_WARNING: TraceLog(LOG_WARNING, "Recast: %s", msg); break;
            case RC_LOG_PROGRESS:
            default:
                break;
        }
    }
};

static Rectangle ComputeTriangleBounds(const Vector2& a, const Vector2& b, const Vector2& c)
{
    const float minX = std::min(a.x, std::min(b.x, c.x));
    const float minY = std::min(a.y, std::min(b.y, c.y));
    const float maxX = std::max(a.x, std::max(b.x, c.x));
    const float maxY = std::max(a.y, std::max(b.y, c.y));

    Rectangle r{};
    r.x = minX;
    r.y = minY;
    r.width = maxX - minX;
    r.height = maxY - minY;
    return r;
}

static Vector2 ComputeTriangleCentroid(const Vector2& a, const Vector2& b, const Vector2& c)
{
    return Vector2{
            (a.x + b.x + c.x) / 3.0f,
            (a.y + b.y + c.y) / 3.0f
    };
}

static EdgeKey MakeEdgeKey(int a, int b)
{
    if (a < b) {
        return EdgeKey{a, b};
    }
    return EdgeKey{b, a};
}

static void RegisterTriangleEdge(
        std::unordered_map<EdgeKey, EdgeOwner, EdgeKeyHash>& edgeMap,
        std::vector<NavTriangle>& triangles,
        int triangleIndex,
        int edgeSlot,
        int v0,
        int v1)
{
    const EdgeKey key = MakeEdgeKey(v0, v1);

    auto it = edgeMap.find(key);
    if (it == edgeMap.end()) {
        edgeMap.emplace(key, EdgeOwner{triangleIndex, edgeSlot});
        return;
    }

    const EdgeOwner other = it->second;
    if (other.triangleIndex < 0 || other.edgeSlot < 0) {
        return;
    }

    NavTriangle& a = triangles[triangleIndex];
    NavTriangle& b = triangles[other.triangleIndex];

    switch (edgeSlot) {
        case 0: a.neighbor0 = other.triangleIndex; break;
        case 1: a.neighbor1 = other.triangleIndex; break;
        case 2: a.neighbor2 = other.triangleIndex; break;
        default: break;
    }

    switch (other.edgeSlot) {
        case 0: b.neighbor0 = triangleIndex; break;
        case 1: b.neighbor1 = triangleIndex; break;
        case 2: b.neighbor2 = triangleIndex; break;
        default: break;
    }
}

static Point64 ToPoint64(Vector2 p)
{
    return Point64(
            static_cast<int64_t>(std::llround(static_cast<double>(p.x) * CLIPPER_SCALE)),
            static_cast<int64_t>(std::llround(static_cast<double>(p.y) * CLIPPER_SCALE)));
}

static Vector2 ToVector2(const Point64& p)
{
    return Vector2{
            static_cast<float>(static_cast<double>(p.x) / CLIPPER_SCALE),
            static_cast<float>(static_cast<double>(p.y) / CLIPPER_SCALE)
    };
}

static Path64 ToPath64(const NavPolygon& poly)
{
    Path64 path;
    path.reserve(poly.vertices.size());

    for (const Vector2& v : poly.vertices) {
        path.push_back(ToPoint64(v));
    }

    return path;
}

static bool PointInPath64(const Path64& path, const Point64& p)
{
    bool inside = false;
    const int count = static_cast<int>(path.size());
    if (count < 3) {
        return false;
    }

    for (int i = 0, j = count - 1; i < count; j = i++) {
        const Point64& a = path[i];
        const Point64& b = path[j];

        const bool intersects =
                ((a.y > p.y) != (b.y > p.y)) &&
                (static_cast<double>(p.x) <
                 (static_cast<double>(b.x - a.x) * static_cast<double>(p.y - a.y)) /
                 ((b.y == a.y) ? 1.0 : static_cast<double>(b.y - a.y)) +
                 static_cast<double>(a.x));

        if (intersects) {
            inside = !inside;
        }
    }

    return inside;
}

static EndType GetClosedPolygonEndType()
{
    return EndType::Polygon;
}

static JoinType GetPolygonJoinType()
{
    return JoinType::Miter;
}

static Paths64 OffsetPaths64(
        const Paths64& input,
        double delta)
{
    if (input.empty()) {
        return {};
    }

    ClipperOffset co;
    co.AddPaths(input, GetPolygonJoinType(), GetClosedPolygonEndType());

    Paths64 output;
    co.Execute(delta, output);
    return output;
}

static Paths64 BuildAdjustedWalkPaths(
        const NavMeshData& navMesh,
        float agentRadius)
{
    Paths64 walkPaths;
    walkPaths.reserve(navMesh.sourcePolygons.size());

    for (const NavPolygon& poly : navMesh.sourcePolygons) {
        if (poly.vertices.size() >= 3) {
            walkPaths.push_back(ToPath64(poly));
        }
    }

    if (walkPaths.empty()) {
        return {};
    }

    if (agentRadius <= 0.0f) {
        return walkPaths;
    }

    const double delta = -static_cast<double>(agentRadius) * CLIPPER_SCALE;
    return OffsetPaths64(walkPaths, delta);
}

static Paths64 BuildAdjustedBlockerPaths(
        const NavMeshData& navMesh,
        float agentRadius)
{
    Paths64 blockerPaths;
    blockerPaths.reserve(navMesh.blockerPolygons.size());

    for (const NavPolygon& poly : navMesh.blockerPolygons) {
        if (poly.vertices.size() >= 3) {
            blockerPaths.push_back(ToPath64(poly));
        }
    }

    if (blockerPaths.empty()) {
        return {};
    }

    if (agentRadius <= 0.0f) {
        return blockerPaths;
    }

    const double delta = static_cast<double>(agentRadius) * CLIPPER_SCALE;
    return OffsetPaths64(blockerPaths, delta);
}

static std::vector<TriangulationPolygon> BuildFinalWalkablePolygons(
        const NavMeshData& navMesh,
        float agentRadius)
{
    const Paths64 adjustedWalkPaths = BuildAdjustedWalkPaths(navMesh, agentRadius);
    if (adjustedWalkPaths.empty()) {
        return {};
    }

    Paths64 mergedWalk = Union(adjustedWalkPaths, FillRule::NonZero);

    const Paths64 adjustedBlockerPaths = BuildAdjustedBlockerPaths(navMesh, agentRadius);

    Paths64 finalPaths = mergedWalk;
    if (!adjustedBlockerPaths.empty()) {
        const Paths64 mergedBlockers = Union(adjustedBlockerPaths, FillRule::NonZero);
        finalPaths = Difference(mergedWalk, mergedBlockers, FillRule::NonZero);
    }

    std::vector<Path64> outers;
    std::vector<Path64> holes;

    for (const Path64& path : finalPaths) {
        if (path.size() < 3) {
            continue;
        }

        if (IsPositive(path)) {
            outers.push_back(path);
        } else {
            holes.push_back(path);
        }
    }

    std::vector<TriangulationPolygon> result;
    result.reserve(outers.size());

    for (const Path64& outer : outers) {
        TriangulationPolygon poly;
        poly.outer = outer;
        result.push_back(poly);
    }

    for (const Path64& hole : holes) {
        if (hole.empty()) {
            continue;
        }

        const Point64 sample = hole[0];
        int bestOuterIndex = -1;
        double bestArea = std::numeric_limits<double>::max();

        for (int i = 0; i < static_cast<int>(result.size()); ++i) {
            const Path64& outer = result[i].outer;
            if (!PointInPath64(outer, sample)) {
                continue;
            }

            const double area = std::fabs(Area(outer));
            if (area < bestArea) {
                bestArea = area;
                bestOuterIndex = i;
            }
        }

        if (bestOuterIndex >= 0) {
            result[bestOuterIndex].holes.push_back(hole);
        }
    }

    return result;
}

static void AppendTriangulatedGeometry(
        const TriangulationPolygon& poly,
        std::vector<float>& outVerts,
        std::vector<int>& outTris)
{
    if (poly.outer.size() < 3) {
        return;
    }

    std::vector<std::vector<std::array<double, 2>>> earcutInput;
    earcutInput.emplace_back();

    const int baseVertexIndex = static_cast<int>(outVerts.size() / 3u);

    earcutInput[0].reserve(poly.outer.size());
    for (const Point64& p : poly.outer) {
        const Vector2 v = ToVector2(p);
        earcutInput[0].push_back(std::array<double, 2>{
                static_cast<double>(v.x),
                static_cast<double>(v.y)
        });

        outVerts.push_back(v.x);
        outVerts.push_back(0.0f);
        outVerts.push_back(v.y);
    }

    for (const Path64& hole : poly.holes) {
        if (hole.size() < 3) {
            continue;
        }

        earcutInput.emplace_back();
        earcutInput.back().reserve(hole.size());

        for (const Point64& p : hole) {
            const Vector2 v = ToVector2(p);
            earcutInput.back().push_back(std::array<double, 2>{
                    static_cast<double>(v.x),
                    static_cast<double>(v.y)
            });

            outVerts.push_back(v.x);
            outVerts.push_back(0.0f);
            outVerts.push_back(v.y);
        }
    }

    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(earcutInput);
    if (indices.size() % 3 != 0) {
        return;
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        const int ia = baseVertexIndex + static_cast<int>(indices[i + 0]);
        const int ib = baseVertexIndex + static_cast<int>(indices[i + 1]);
        const int ic = baseVertexIndex + static_cast<int>(indices[i + 2]);

        if (ia == ib || ib == ic || ic == ia) {
            continue;
        }

        // Recast expects walkable triangle normals to point upward (+Y).
        // Our 2D nav data is mapped as:
        //   world.x -> recast.x
        //   world.y -> recast.z
        //   recast.y = 0
        //
        // For triangles on the XZ plane, the winding needs to be reversed
        // relative to ordinary 2D XY winding, otherwise the normal points
        // downward and rcMarkWalkableTriangles marks it non-walkable.
        outTris.push_back(ia);
        outTris.push_back(ic);
        outTris.push_back(ib);
    }
}

static std::shared_ptr<dtNavMesh> MakeNavMeshPtr(dtNavMesh* mesh)
{
    return std::shared_ptr<dtNavMesh>(
            mesh,
            [](dtNavMesh* p) {
                if (p != nullptr) {
                    dtFreeNavMesh(p);
                }
            });
}

static std::shared_ptr<dtNavMeshQuery> MakeNavMeshQueryPtr(dtNavMeshQuery* query)
{
    return std::shared_ptr<dtNavMeshQuery>(
            query,
            [](dtNavMeshQuery* p) {
                if (p != nullptr) {
                    dtFreeNavMeshQuery(p);
                }
            });
}

static void AppendDebugTriangle(
        std::unordered_map<EdgeKey, EdgeOwner, EdgeKeyHash>& edgeMap,
        std::vector<NavTriangle>& triangles,
        const std::vector<Vector2>& vertices,
        int ia,
        int ib,
        int ic)
{
    if (ia == ib || ib == ic || ic == ia) {
        return;
    }

    if (ia < 0 || ib < 0 || ic < 0 ||
        ia >= static_cast<int>(vertices.size()) ||
        ib >= static_cast<int>(vertices.size()) ||
        ic >= static_cast<int>(vertices.size())) {
        return;
    }

    const Vector2 a = vertices[ia];
    const Vector2 b = vertices[ib];
    const Vector2 c = vertices[ic];

    const float area2 = std::fabs(
            (b.x - a.x) * (c.y - a.y) -
            (b.y - a.y) * (c.x - a.x));

    if (area2 <= 0.0001f) {
        return;
    }

    NavTriangle tri;
    tri.vertexIndex0 = ia;
    tri.vertexIndex1 = ib;
    tri.vertexIndex2 = ic;
    tri.centroid = ComputeTriangleCentroid(a, b, c);
    tri.bounds = ComputeTriangleBounds(a, b, c);

    const int triIndex = static_cast<int>(triangles.size());
    triangles.push_back(tri);

    RegisterTriangleEdge(edgeMap, triangles, triIndex, 0, ia, ib);
    RegisterTriangleEdge(edgeMap, triangles, triIndex, 1, ib, ic);
    RegisterTriangleEdge(edgeMap, triangles, triIndex, 2, ic, ia);
}

static bool BuildDebugTrianglesFromPolyMesh(
        const rcPolyMesh& pmesh,
        NavMeshData& navMesh)
{
    navMesh.vertices.clear();
    navMesh.triangles.clear();

    if (pmesh.nverts <= 0 || pmesh.npolys <= 0 || pmesh.nvp <= 0) {
        return false;
    }

    navMesh.vertices.reserve(static_cast<size_t>(pmesh.nverts));
    for (int i = 0; i < pmesh.nverts; ++i) {
        const unsigned short* v = &pmesh.verts[static_cast<size_t>(i) * 3u];

        Vector2 world{
                pmesh.bmin[0] + static_cast<float>(v[0]) * pmesh.cs,
                pmesh.bmin[2] + static_cast<float>(v[2]) * pmesh.cs
        };

        navMesh.vertices.push_back(world);
    }

    std::unordered_map<EdgeKey, EdgeOwner, EdgeKeyHash> edgeMap;
    navMesh.triangles.reserve(static_cast<size_t>(pmesh.npolys) * 4u);

    for (int i = 0; i < pmesh.npolys; ++i) {
        const unsigned short* poly =
                &pmesh.polys[static_cast<size_t>(i) * static_cast<size_t>(pmesh.nvp * 2)];

        std::vector<int> polyVerts;
        polyVerts.reserve(static_cast<size_t>(pmesh.nvp));

        for (int j = 0; j < pmesh.nvp; ++j) {
            const unsigned short idx = poly[j];
            if (idx == RC_MESH_NULL_IDX) {
                break;
            }

            const int vi = static_cast<int>(idx);
            if (vi < 0 || vi >= static_cast<int>(navMesh.vertices.size())) {
                polyVerts.clear();
                break;
            }

            polyVerts.push_back(vi);
        }

        if (polyVerts.size() < 3) {
            continue;
        }

        if (polyVerts.size() == 3) {
            AppendDebugTriangle(
                    edgeMap,
                    navMesh.triangles,
                    navMesh.vertices,
                    polyVerts[0],
                    polyVerts[1],
                    polyVerts[2]);
            continue;
        }

        std::vector<std::array<double, 2>> ring;
        ring.reserve(polyVerts.size());

        for (int vi : polyVerts) {
            const Vector2& v = navMesh.vertices[vi];
            ring.push_back(std::array<double, 2>{
                    static_cast<double>(v.x),
                    static_cast<double>(v.y)
            });
        }

        std::vector<std::vector<std::array<double, 2>>> earcutInput;
        earcutInput.push_back(ring);

        const std::vector<uint32_t> localIndices =
                mapbox::earcut<uint32_t>(earcutInput);

        if (localIndices.size() < 3 || (localIndices.size() % 3) != 0) {
            continue;
        }

        for (size_t t = 0; t < localIndices.size(); t += 3) {
            const uint32_t la = localIndices[t + 0];
            const uint32_t lb = localIndices[t + 1];
            const uint32_t lc = localIndices[t + 2];

            if (la >= polyVerts.size() ||
                lb >= polyVerts.size() ||
                lc >= polyVerts.size()) {
                continue;
            }

            AppendDebugTriangle(
                    edgeMap,
                    navMesh.triangles,
                    navMesh.vertices,
                    polyVerts[static_cast<size_t>(la)],
                    polyVerts[static_cast<size_t>(lb)],
                    polyVerts[static_cast<size_t>(lc)]);
        }
    }

    return !navMesh.triangles.empty();
}

static bool BuildRecastDetourNavMesh(
        NavMeshData& navMesh,
        const std::vector<float>& geomVerts,
        const std::vector<int>& geomTris)
{
    navMesh.detourNavMesh.reset();
    navMesh.detourQuery.reset();

    if (geomVerts.empty() || geomTris.empty()) {
        return false;
    }

    const int geomVertCount = static_cast<int>(geomVerts.size() / 3u);
    const int geomTriCount = static_cast<int>(geomTris.size() / 3u);

    if (geomVertCount <= 0 || geomTriCount <= 0) {
        return false;
    }

    RecastBuildContext ctx;

    float bmin[3]{};
    float bmax[3]{};
    rcCalcBounds(geomVerts.data(), geomVertCount, bmin, bmax);

    rcConfig cfg{};
    cfg.cs = kCellSize;
    cfg.ch = kCellHeight;
    cfg.walkableSlopeAngle = kWalkableSlopeAngle;
    cfg.walkableHeight = static_cast<int>(std::ceil(kWalkableHeight / cfg.ch));
    cfg.walkableClimb = static_cast<int>(std::floor(kWalkableClimb / cfg.ch));
    cfg.walkableRadius = 0; // already handled by clipper offset
    cfg.maxEdgeLen = static_cast<int>(kMaxEdgeLenCells);
    cfg.maxSimplificationError = kMaxSimplificationError;
    cfg.minRegionArea = kMinRegionAreaCells;
    cfg.mergeRegionArea = kMergeRegionAreaCells;
    cfg.maxVertsPerPoly = kMaxVertsPerPoly;
    cfg.detailSampleDist = kDetailSampleDist;
    cfg.detailSampleMaxError = kDetailSampleMaxError;
    cfg.borderSize = 0;

    rcVcopy(cfg.bmin, bmin);
    rcVcopy(cfg.bmax, bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    if (cfg.width <= 0 || cfg.height <= 0) {
        TraceLog(LOG_ERROR, "Recast build failed: invalid grid size");
        return false;
    }

    using HeightfieldPtr = std::unique_ptr<rcHeightfield, void(*)(rcHeightfield*)>;
    using CompactHeightfieldPtr = std::unique_ptr<rcCompactHeightfield, void(*)(rcCompactHeightfield*)>;
    using ContourSetPtr = std::unique_ptr<rcContourSet, void(*)(rcContourSet*)>;
    using PolyMeshPtr = std::unique_ptr<rcPolyMesh, void(*)(rcPolyMesh*)>;

    HeightfieldPtr solid(rcAllocHeightfield(), rcFreeHeightField);
    CompactHeightfieldPtr chf(rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
    ContourSetPtr cset(rcAllocContourSet(), rcFreeContourSet);
    PolyMeshPtr pmesh(rcAllocPolyMesh(), rcFreePolyMesh);

    if (!solid || !chf || !cset || !pmesh) {
        TraceLog(LOG_ERROR, "Recast build failed: allocation failed");
        return false;
    }

    if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
        TraceLog(LOG_ERROR, "rcCreateHeightfield failed");
        return false;
    }

    std::vector<unsigned char> triAreas(static_cast<size_t>(geomTriCount), 0);
    rcMarkWalkableTriangles(&ctx,
                            cfg.walkableSlopeAngle,
                            geomVerts.data(),
                            geomVertCount,
                            geomTris.data(),
                            geomTriCount,
                            triAreas.data());

    if (!rcRasterizeTriangles(&ctx,
                              geomVerts.data(),
                              geomVertCount,
                              geomTris.data(),
                              triAreas.data(),
                              geomTriCount,
                              *solid,
                              cfg.walkableClimb)) {
        TraceLog(LOG_ERROR, "rcRasterizeTriangles failed");
        return false;
    }

    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    if (!rcBuildCompactHeightfield(&ctx,
                                   cfg.walkableHeight,
                                   cfg.walkableClimb,
                                   *solid,
                                   *chf)) {
        TraceLog(LOG_ERROR, "rcBuildCompactHeightfield failed");
        return false;
    }

    if (cfg.walkableRadius > 0) {
        if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) {
            TraceLog(LOG_ERROR, "rcErodeWalkableArea failed");
            return false;
        }
    }

    if (!rcBuildDistanceField(&ctx, *chf)) {
        TraceLog(LOG_ERROR, "rcBuildDistanceField failed");
        return false;
    }

    if (!rcBuildRegions(&ctx,
                        *chf,
                        cfg.borderSize,
                        cfg.minRegionArea,
                        cfg.mergeRegionArea)) {
        TraceLog(LOG_ERROR, "rcBuildRegions failed");
        return false;
    }

    if (!rcBuildContours(&ctx,
                         *chf,
                         cfg.maxSimplificationError,
                         cfg.maxEdgeLen,
                         *cset)) {
        TraceLog(LOG_ERROR, "rcBuildContours failed");
        return false;
    }

    if (cset->nconts <= 0) {
        TraceLog(LOG_ERROR, "Recast build failed: no contours");
        return false;
    }

    if (!rcBuildPolyMesh(&ctx,
                         *cset,
                         cfg.maxVertsPerPoly,
                         *pmesh)) {
        TraceLog(LOG_ERROR, "rcBuildPolyMesh failed");
        return false;
    }

    if (pmesh->npolys <= 0 || pmesh->nverts <= 0) {
        TraceLog(LOG_ERROR, "Recast build failed: empty poly mesh");
        return false;
    }

    for (int i = 0; i < pmesh->npolys; ++i) {
        pmesh->flags[i] = 1;
        if (pmesh->areas[i] == RC_NULL_AREA) {
            pmesh->areas[i] = 0;
        }
    }

    dtNavMeshCreateParams params{};
    params.verts = pmesh->verts;
    params.vertCount = pmesh->nverts;
    params.polys = pmesh->polys;
    params.polyAreas = pmesh->areas;
    params.polyFlags = pmesh->flags;
    params.polyCount = pmesh->npolys;
    params.nvp = pmesh->nvp;

    params.detailMeshes = nullptr;
    params.detailVerts = nullptr;
    params.detailVertsCount = 0;
    params.detailTris = nullptr;
    params.detailTriCount = 0;

    params.walkableHeight = kWalkableHeight;
    params.walkableRadius = 0.0f;
    params.walkableClimb = kWalkableClimb;

    rcVcopy(params.bmin, pmesh->bmin);
    rcVcopy(params.bmax, pmesh->bmax);

    params.cs = pmesh->cs;
    params.ch = pmesh->ch;

    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;

    if (!dtCreateNavMeshData(&params, &navData, &navDataSize) ||
        navData == nullptr ||
        navDataSize <= 0) {
        TraceLog(LOG_ERROR, "dtCreateNavMeshData failed");
        return false;
    }

    dtNavMesh* rawNavMesh = dtAllocNavMesh();
    if (rawNavMesh == nullptr) {
        dtFree(navData);
        TraceLog(LOG_ERROR, "dtAllocNavMesh failed");
        return false;
    }

    if (dtStatusFailed(rawNavMesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
        dtFreeNavMesh(rawNavMesh);
        TraceLog(LOG_ERROR, "dtNavMesh::init failed");
        return false;
    }

    dtNavMeshQuery* rawQuery = dtAllocNavMeshQuery();
    if (rawQuery == nullptr) {
        dtFreeNavMesh(rawNavMesh);
        TraceLog(LOG_ERROR, "dtAllocNavMeshQuery failed");
        return false;
    }

    if (dtStatusFailed(rawQuery->init(rawNavMesh, std::max(2048, pmesh->npolys * 8)))) {
        dtFreeNavMeshQuery(rawQuery);
        dtFreeNavMesh(rawNavMesh);
        TraceLog(LOG_ERROR, "dtNavMeshQuery::init failed");
        return false;
    }

    navMesh.detourNavMesh = MakeNavMeshPtr(rawNavMesh);
    navMesh.detourQuery = MakeNavMeshQueryPtr(rawQuery);

    if (!BuildDebugTrianglesFromPolyMesh(*pmesh, navMesh)) {
        navMesh.detourNavMesh.reset();
        navMesh.detourQuery.reset();
        TraceLog(LOG_ERROR, "Failed to build debug triangles from Recast poly mesh");
        return false;
    }

    return true;
}

bool BuildNavMesh(NavMeshData& navMesh, float agentRadius)
{
    navMesh.vertices.clear();
    navMesh.triangles.clear();
    navMesh.detourNavMesh.reset();
    navMesh.detourQuery.reset();
    navMesh.built = false;

    const std::vector<TriangulationPolygon> finalPolys =
            BuildFinalWalkablePolygons(navMesh, std::max(0.0f, agentRadius));

    if (finalPolys.empty()) {
        return false;
    }

    std::vector<float> geomVerts;
    std::vector<int> geomTris;

    for (const TriangulationPolygon& poly : finalPolys) {
        AppendTriangulatedGeometry(poly, geomVerts, geomTris);
    }

    if (geomVerts.empty() || geomTris.empty()) {
        return false;
    }

    if (!BuildRecastDetourNavMesh(navMesh, geomVerts, geomTris)) {
        navMesh.vertices.clear();
        navMesh.triangles.clear();
        navMesh.detourNavMesh.reset();
        navMesh.detourQuery.reset();
        navMesh.built = false;
        return false;
    }

    navMesh.built = true;
    return true;
}

bool BuildNavMesh(NavMeshData& navMesh)
{
    return BuildNavMesh(navMesh, 0.0f);
}