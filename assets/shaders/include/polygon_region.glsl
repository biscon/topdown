bool pointInPolygon(vec2 p)
{
    if (uUsePolygon == 0 || uPolygonVertexCount < 3) {
        return true;
    }

    bool inside = false;

    for (int i = 0, j = uPolygonVertexCount - 1; i < uPolygonVertexCount; j = i, ++i) {
        vec2 a = uPolygonPoints[i];
        vec2 b = uPolygonPoints[j];

        float denom = b.y - a.y;
        if (abs(denom) < 0.00001) {
            denom = (denom < 0.0) ? -0.00001 : 0.00001;
        }

        bool intersect =
        ((a.y > p.y) != (b.y > p.y)) &&
        (p.x < ((b.x - a.x) * (p.y - a.y) / denom) + a.x);

        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

float distanceToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 ab = b - a;
    float abLenSq = dot(ab, ab);
    if (abLenSq <= 0.00001) {
        return length(p - a);
    }

    float t = clamp(dot(p - a, ab) / abLenSq, 0.0, 1.0);
    vec2 closest = a + ab * t;
    return length(p - closest);
}

float polygonEdgeFade(vec2 pixelPos, float softnessPixels)
{
    if (uUsePolygon == 0 || uPolygonVertexCount < 3) {
        return 1.0;
    }

    float minDist = 1e20;

    for (int i = 0; i < uPolygonVertexCount; ++i) {
        int j = (i + 1) % uPolygonVertexCount;
        float d = distanceToSegment(pixelPos, uPolygonPoints[i], uPolygonPoints[j]);
        minDist = min(minDist, d);
    }

    return clamp(minDist / max(softnessPixels, 0.0001), 0.0, 1.0);
}

float rectEdgeFade(vec2 local, float softness)
{
    float s = clamp(softness, 0.0001, 0.49);

    float fadeX =
    smoothstep(0.0, s, local.x) *
    (1.0 - smoothstep(1.0 - s, 1.0, local.x));

    float fadeY =
    smoothstep(0.0, s, local.y) *
    (1.0 - smoothstep(1.0 - s, 1.0, local.y));

    return fadeX * fadeY;
}

float authoredRegionMask(vec2 pixelPos, vec2 local, float softness)
{
    if (uUsePolygon != 0 && uPolygonVertexCount >= 3) {
        if (!pointInPolygon(pixelPos)) {
            return 0.0;
        }

        float softnessPixels = max(uRegionSize.x, uRegionSize.y) * clamp(softness, 0.0001, 1.0);
        return polygonEdgeFade(pixelPos, softnessPixels);
    }

    if (local.x < 0.0 || local.x > 1.0 || local.y < 0.0 || local.y > 1.0) {
        return 0.0;
    }

    return rectEdgeFade(local, softness);
}

float regionMask(vec2 pixelPos, vec2 local, float softness)
{
    return authoredRegionMask(pixelPos, local, softness);
}
