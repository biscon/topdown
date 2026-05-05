#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float uTime;
uniform vec2 uScrollSpeed;
uniform vec2 uUvScale;
uniform vec2 uDistortionAmount;
uniform vec2 uNoiseScrollSpeed;
uniform float uIntensity;
uniform float uPhaseOffset;
uniform float uSoftness;

uniform vec2 uSceneSize;
uniform vec2 uRegionPos;
uniform vec2 uRegionSize;

uniform int uUsePolygon;
uniform int uPolygonVertexCount;
uniform vec2 uPolygonPoints[32];

out vec4 finalColor;

#include "../include/polygon_region.glsl"
#include "../include/noise.glsl"

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    vec4 original = texture(texture0, fragTexCoord);

    float baseMask = regionMask(pixelPos, local, uSoftness);
    if (baseMask <= 0.0001) {
        finalColor = original;
        return;
    }

    vec2 safeScale = max(uUvScale, vec2(0.0001));
    vec2 noiseUv = local * safeScale;
    noiseUv += uNoiseScrollSpeed * uTime;
    noiseUv += vec2(uPhaseOffset, uPhaseOffset);

    float n1 = noise(noiseUv + vec2(0.0, 0.0));
    float n2 = noise(noiseUv + vec2(17.3, 9.1));

    vec2 offsetPixels = vec2(
        (n1 * 2.0 - 1.0) * uDistortionAmount.x,
        (n2 * 2.0 - 1.0) * uDistortionAmount.y
    );

    float verticalBias = smoothstep(0.08, 0.60, local.y);
    float shimmerMask = baseMask * verticalBias * uIntensity;

    vec2 offsetUv = (offsetPixels / uSceneSize) * shimmerMask;
    vec2 sampleUv = fragTexCoord + offsetUv;

    finalColor = texture(texture0, sampleUv);
}
