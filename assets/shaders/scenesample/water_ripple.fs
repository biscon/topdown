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

uniform vec2 uSceneSize;
uniform vec2 uRegionPos;
uniform vec2 uRegionSize;
uniform float uSoftness;

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
    vec2 rippleUv = local * safeScale;
    rippleUv += uNoiseScrollSpeed * uTime;
    rippleUv += vec2(uPhaseOffset, uPhaseOffset);

    float n1 = noise(rippleUv + vec2(0.0, 0.0));
    float n2 = noise(rippleUv + vec2(11.7, 5.3));

    float wave1 = sin((rippleUv.x + rippleUv.y * 0.35 + uTime * 0.9 + uPhaseOffset) * 6.28318);
    float wave2 = sin((rippleUv.x * 0.6 - rippleUv.y + uTime * 0.7 + uPhaseOffset * 1.7) * 6.28318);

    vec2 offsetPixels;
    offsetPixels.x =
        ((n1 * 2.0 - 1.0) * 0.55 + wave1 * 0.45) * uDistortionAmount.x;
    offsetPixels.y =
        ((n2 * 2.0 - 1.0) * 0.55 + wave2 * 0.45) * uDistortionAmount.y;

    float rippleMask = baseMask * uIntensity;

    vec2 offsetUv = (offsetPixels / uSceneSize) * rippleMask;
    vec2 sampleUv = fragTexCoord + offsetUv;

    finalColor = texture(texture0, sampleUv);
}

