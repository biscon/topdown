#include <algorithm>
#include <cmath>
#include "LevelCollision.h"
#include "TopdownHelpers.h"

static constexpr float kCollisionEpsilon = 0.001f;
static constexpr float kMovementGridPadding = 1.0f;

static int MovementGridFloorToInt(float value)
{
    return static_cast<int>(std::floor(value));
}

static int MovementGridClampInt(int value, int minValue, int maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

static bool MovementGridRectsOverlap(const Rectangle& a, const Rectangle& b)
{
    if (a.x + a.width < b.x || b.x + b.width < a.x) {
        return false;
    }
    if (a.y + a.height < b.y || b.y + b.height < a.y) {
        return false;
    }
    return true;
}

static int GetMovementGridCellIndex(const TopdownCollisionSegmentGrid& grid, int x, int y)
{
    return y * grid.width + x;
}

static void GetMovementGridRangeForBounds(
        const TopdownCollisionSegmentGrid& grid,
        const Rectangle& bounds,
        int& outMinX,
        int& outMaxX,
        int& outMinY,
        int& outMaxY)
{
    const float minX = (bounds.x - grid.origin.x) / grid.cellSize;
    const float maxX = (bounds.x + bounds.width - grid.origin.x) / grid.cellSize;
    const float minY = (bounds.y - grid.origin.y) / grid.cellSize;
    const float maxY = (bounds.y + bounds.height - grid.origin.y) / grid.cellSize;

    outMinX = MovementGridClampInt(MovementGridFloorToInt(minX), 0, grid.width - 1);
    outMaxX = MovementGridClampInt(MovementGridFloorToInt(maxX), 0, grid.width - 1);
    outMinY = MovementGridClampInt(MovementGridFloorToInt(minY), 0, grid.height - 1);
    outMaxY = MovementGridClampInt(MovementGridFloorToInt(maxY), 0, grid.height - 1);
}

Rectangle TopdownBuildSegmentBounds(const TopdownSegment& seg)
{
    const float minX = std::min(seg.a.x, seg.b.x);
    const float minY = std::min(seg.a.y, seg.b.y);
    const float maxX = std::max(seg.a.x, seg.b.x);
    const float maxY = std::max(seg.a.y, seg.b.y);

    return Rectangle{minX, minY, maxX - minX, maxY - minY};
}

void TopdownRebuildMovementSegmentGrid(TopdownCollisionWorld& collision)
{
    TopdownCollisionSegmentGrid& grid = collision.movementSegmentGrid;
    grid = {};
    grid.cellSize = 256.0f;

    collision.movementSegmentBounds.clear();
    collision.movementSegmentBounds.reserve(collision.movementSegments.size());

    if (collision.movementSegments.empty()) {
        return;
    }

    Rectangle totalBounds{};
    bool hasBounds = false;

    for (const TopdownSegment& seg : collision.movementSegments) {
        const Rectangle segBounds = TopdownBuildSegmentBounds(seg);
        collision.movementSegmentBounds.push_back(segBounds);

        if (!hasBounds) {
            totalBounds = segBounds;
            hasBounds = true;
            continue;
        }

        const float minX = std::min(totalBounds.x, segBounds.x);
        const float minY = std::min(totalBounds.y, segBounds.y);
        const float maxX = std::max(totalBounds.x + totalBounds.width, segBounds.x + segBounds.width);
        const float maxY = std::max(totalBounds.y + totalBounds.height, segBounds.y + segBounds.height);
        totalBounds = Rectangle{minX, minY, maxX - minX, maxY - minY};
    }

    totalBounds.x -= kMovementGridPadding;
    totalBounds.y -= kMovementGridPadding;
    totalBounds.width += kMovementGridPadding * 2.0f;
    totalBounds.height += kMovementGridPadding * 2.0f;

    grid.origin = Vector2{totalBounds.x, totalBounds.y};
    grid.width = std::max(1, static_cast<int>(std::ceil(totalBounds.width / grid.cellSize)));
    grid.height = std::max(1, static_cast<int>(std::ceil(totalBounds.height / grid.cellSize)));
    grid.cells.assign(grid.width * grid.height, {});

    for (int i = 0; i < static_cast<int>(collision.movementSegmentBounds.size()); ++i) {
        const Rectangle& segBounds = collision.movementSegmentBounds[i];

        int minX = 0;
        int maxX = 0;
        int minY = 0;
        int maxY = 0;
        GetMovementGridRangeForBounds(grid, segBounds, minX, maxX, minY, maxY);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                grid.cells[GetMovementGridCellIndex(grid, x, y)].segmentIndices.push_back(i);
            }
        }
    }

    grid.built = true;
}

void TopdownQueryMovementSegmentGrid(
        const TopdownCollisionWorld& collision,
        Rectangle queryBounds,
        std::vector<int>& outSegmentIndices)
{
    outSegmentIndices.clear();

    const size_t segmentCount = collision.movementSegments.size();
    const bool hasMatchingBounds = collision.movementSegmentBounds.size() == segmentCount;

    if (segmentCount == 0) {
        return;
    }

    auto appendBoundsOverlaps = [&](auto boundsGetter) {
        for (int i = 0; i < static_cast<int>(segmentCount); ++i) {
            if (MovementGridRectsOverlap(boundsGetter(i), queryBounds)) {
                outSegmentIndices.push_back(i);
            }
        }
    };

    if (!collision.movementSegmentGrid.built || !hasMatchingBounds) {
        if (hasMatchingBounds) {
            appendBoundsOverlaps([&](int i) -> const Rectangle& {
                return collision.movementSegmentBounds[i];
            });
        } else {
            for (int i = 0; i < static_cast<int>(segmentCount); ++i) {
                if (MovementGridRectsOverlap(TopdownBuildSegmentBounds(collision.movementSegments[i]), queryBounds)) {
                    outSegmentIndices.push_back(i);
                }
            }
        }
        return;
    }

    const TopdownCollisionSegmentGrid& grid = collision.movementSegmentGrid;
    if (grid.width <= 0 || grid.height <= 0 || grid.cells.empty()) {
        appendBoundsOverlaps([&](int i) -> const Rectangle& {
            return collision.movementSegmentBounds[i];
        });
        return;
    }

    static thread_local std::vector<int> visitedGeneration;
    static thread_local int generation = 1;

    if (visitedGeneration.size() < segmentCount) {
        visitedGeneration.assign(segmentCount, 0);
        generation = 1;
    }

    ++generation;
    if (generation == 0) {
        std::fill(visitedGeneration.begin(), visitedGeneration.end(), 0);
        generation = 1;
    }

    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    GetMovementGridRangeForBounds(grid, queryBounds, minX, maxX, minY, maxY);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const TopdownCollisionSegmentGridCell& cell =
                    grid.cells[GetMovementGridCellIndex(grid, x, y)];

            for (int segmentIndex : cell.segmentIndices) {
                if (segmentIndex < 0 || segmentIndex >= static_cast<int>(segmentCount)) {
                    continue;
                }
                if (visitedGeneration[segmentIndex] == generation) {
                    continue;
                }
                visitedGeneration[segmentIndex] = generation;

                if (MovementGridRectsOverlap(
                        collision.movementSegmentBounds[segmentIndex],
                        queryBounds)) {
                    outSegmentIndices.push_back(segmentIndex);
                }
            }
        }
    }
}


Vector2 MoveTowardsVector(Vector2 current, Vector2 target, float maxDelta)
{
    const Vector2 delta = TopdownSub(target, current);
    const float dist = TopdownLength(delta);

    if (dist <= maxDelta || dist <= 0.000001f) {
        return target;
    }

    const Vector2 dir = TopdownMul(delta, 1.0f / dist);
    return TopdownAdd(current, TopdownMul(dir, maxDelta));
}

float MoveTowardsFloat(float current, float target, float maxDelta)
{
    if (current < target) {
        return std::min(current + maxDelta, target);
    }
    return std::max(current - maxDelta, target);
}

float MoveTowardsAngle(float current, float target, float maxDelta)
{
    float delta = TopdownNormalizeAngleRadians(target - current);

    if (std::fabs(delta) <= maxDelta) {
        return target;
    }

    if (delta > 0.0f) {
        return TopdownNormalizeAngleRadians(current + maxDelta);
    }

    return TopdownNormalizeAngleRadians(current - maxDelta);
}

void ResolveCircleVsCircle(
        Vector2& position,
        Vector2& velocity,
        float radius,
        Vector2 otherPosition,
        float otherRadius,
        Vector2 preferredSeparationDir)
{
    Vector2 delta = TopdownSub(position, otherPosition);
    float dist = TopdownLength(delta);
    const float minDist = radius + otherRadius;

    if (dist >= minDist || minDist <= 0.0f) {
        return;
    }

    Vector2 normal{};

    if (dist > 0.000001f) {
        normal = TopdownMul(delta, 1.0f / dist);

        // soft directional bias:
        // if caller provided a preferred separation direction and it points
        // roughly away from the other body, blend a little toward it.
        if (TopdownLengthSqr(preferredSeparationDir) > 0.000001f) {
            Vector2 preferred = TopdownNormalizeOrZero(preferredSeparationDir);

            if (TopdownDot(normal, preferred) > -0.25f) {
                normal = TopdownNormalizeOrZero(
                        TopdownAdd(
                                TopdownMul(normal, 0.75f),
                                TopdownMul(preferred, 0.25f)));
            }
        }
    } else {
        if (TopdownLengthSqr(preferredSeparationDir) > 0.000001f) {
            normal = TopdownNormalizeOrZero(preferredSeparationDir);
        } else if (TopdownLengthSqr(velocity) > 0.000001f) {
            // sideways bias instead of forward bias looks better for crowds
            normal = Vector2{ -velocity.y, velocity.x };
            normal = TopdownNormalizeOrZero(normal);

            if (TopdownLengthSqr(normal) <= 0.000001f) {
                normal = Vector2{1.0f, 0.0f};
            }
        } else {
            normal = Vector2{1.0f, 0.0f};
        }

        dist = 0.0f;
    }

    const float push = minDist - dist + kCollisionEpsilon;
    position = TopdownAdd(position, TopdownMul(normal, push));

    const float vn = TopdownDot(velocity, normal);
    if (vn < 0.0f) {
        velocity = TopdownSub(velocity, TopdownMul(normal, vn));
    }
}

void ResolveCircleVsSegment(
        Vector2& position,
        Vector2& velocity,
        float radius,
        const TopdownSegment& seg)
{
    const Vector2 closest = TopdownClosestPointOnSegment(position, seg);
    Vector2 delta = TopdownSub(position, closest);
    float dist = TopdownLength(delta);

    if (dist >= radius) {
        return;
    }

    Vector2 normal{};

    if (dist > 0.000001f) {
        normal = TopdownMul(delta, 1.0f / dist);
    } else {
        const Vector2 ab = TopdownSub(seg.b, seg.a);
        const Vector2 perp{-ab.y, ab.x};
        normal = TopdownNormalizeOrZero(perp);

        if (TopdownLengthSqr(normal) <= 0.000001f) {
            if (TopdownLengthSqr(velocity) > 0.000001f) {
                normal = TopdownNormalizeOrZero(velocity);
            } else {
                normal = Vector2{1.0f, 0.0f};
            }
        }
    }

    position = TopdownAdd(closest, TopdownMul(normal, radius + kCollisionEpsilon));

    const float vn = TopdownDot(velocity, normal);
    if (vn < 0.0f) {
        velocity = TopdownSub(velocity, TopdownMul(normal, vn));
    }
}

bool RaycastCircleDetailed(
        Vector2 rayOrigin,
        Vector2 rayDir,
        Vector2 circleCenter,
        float radius,
        float maxDistance,
        float& outDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal)
{
    const Vector2 originToCenter = TopdownSub(circleCenter, rayOrigin);
    const float radiusSqr = radius * radius;
    const float originDistSqr = TopdownLengthSqr(originToCenter);

    // Special case: shot starts inside the circle.
    if (originDistSqr <= radiusSqr) {
        outDistance = 0.0f;
        outHitPoint = rayOrigin;
        outHitNormal = TopdownMul(rayDir, -1.0f);

        if (TopdownLengthSqr(outHitNormal) <= 0.000001f) {
            outHitNormal = Vector2{1.0f, 0.0f};
        }

        return true;
    }

    const float tClosest = TopdownDot(originToCenter, rayDir);
    if (tClosest < 0.0f || tClosest > maxDistance) {
        return false;
    }

    const Vector2 closestPoint = TopdownAdd(rayOrigin, TopdownMul(rayDir, tClosest));
    const Vector2 diff = TopdownSub(circleCenter, closestPoint);

    const float distSqr = TopdownLengthSqr(diff);
    if (distSqr > radiusSqr) {
        return false;
    }

    float thc = std::sqrt(radiusSqr - distSqr);
    float tHit = tClosest - thc;

    if (tHit < 0.0f) {
        tHit = tClosest + thc;
    }

    if (tHit < 0.0f || tHit > maxDistance) {
        return false;
    }

    outDistance = tHit;
    outHitPoint = TopdownAdd(rayOrigin, TopdownMul(rayDir, tHit));
    outHitNormal = TopdownNormalizeOrZero(TopdownSub(outHitPoint, circleCenter));

    if (TopdownLengthSqr(outHitNormal) <= 0.000001f) {
        outHitNormal = TopdownMul(rayDir, -1.0f);
    }

    return true;
}

static Vector2 ComputeSegmentHitNormal(
        const TopdownSegment& seg,
        Vector2 rayDir)
{
    const Vector2 edge = TopdownSub(seg.b, seg.a);

    Vector2 n0{ -edge.y, edge.x };
    Vector2 n1{ edge.y, -edge.x };

    n0 = TopdownNormalizeOrZero(n0);
    n1 = TopdownNormalizeOrZero(n1);

    if (TopdownLengthSqr(n0) <= 0.000001f) {
        return TopdownMul(rayDir, -1.0f);
    }

    // Pick the normal that faces against the incoming shot direction.
    if (TopdownDot(n0, rayDir) < TopdownDot(n1, rayDir)) {
        return n0;
    }

    return n1;
}


static float Cross2D(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
}

static bool RaycastSingleSegment(
        Vector2 origin,
        Vector2 dir,
        const TopdownSegment& seg,
        float maxDistance,
        Vector2& outHitPoint,
        float& outHitDistance)
{
    const Vector2 segDir = TopdownSub(seg.b, seg.a);
    const float denom = Cross2D(dir, segDir);

    if (std::fabs(denom) <= 0.000001f) {
        return false;
    }

    const Vector2 toSegStart = TopdownSub(seg.a, origin);

    const float rayT = Cross2D(toSegStart, segDir) / denom;
    const float segT = Cross2D(toSegStart, dir) / denom;

    if (rayT < 0.0f || rayT > maxDistance) {
        return false;
    }

    if (segT < 0.0f || segT > 1.0f) {
        return false;
    }

    outHitDistance = rayT;
    outHitPoint = TopdownAdd(origin, TopdownMul(dir, rayT));
    return true;
}

bool RaycastClosestSegmentWithNormal(
        Vector2 origin,
        Vector2 dir,
        const std::vector<TopdownSegment>& segments,
        float maxDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance)
{
    Vector2 bestPoint{};
    Vector2 bestNormal{};
    float bestDistance = maxDistance;
    bool hit = false;

    for (const TopdownSegment& seg : segments) {
        Vector2 point{};
        float distance = bestDistance;

        if (!RaycastSingleSegment(origin, dir, seg, bestDistance, point, distance)) {
            continue;
        }

        bestDistance = distance;
        bestPoint = point;
        bestNormal = ComputeSegmentHitNormal(seg, dir);
        hit = true;
    }

    if (!hit) {
        return false;
    }

    outHitPoint = bestPoint;
    outHitNormal = bestNormal;
    outHitDistance = bestDistance;
    return true;
}
