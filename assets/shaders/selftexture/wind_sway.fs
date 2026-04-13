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

uniform int uUseOcclusionPolygon;
uniform int uOcclusionPolygonVertexCount;
uniform vec2 uOcclusionPolygonPoints[256];

out vec4 finalColor;

#include "../include/polygon_region.glsl"

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    float mask = regionMask(pixelPos, local, uSoftness);
    if (mask <= 0.0001) {
        discard;
    }

    vec2 uv = fragTexCoord;

    vec2 safeScale = max(uUvScale, vec2(0.0001));

    float speed = uNoiseScrollSpeed.x;
    float phase =
        uTime * speed +
        local.y * safeScale.y * 6.28318 +
        local.x * safeScale.x * 3.14159 +
        uPhaseOffset;

    float waveA = sin(phase);
    float waveB = sin(phase * 1.73 + local.y * 2.1);
    float wave = waveA * 0.7 + waveB * 0.3;

    //float topBias = smoothstep(0.05, 1.0, local.y);
    //float sway = wave * uDistortionAmount.x * topBias * mask * uIntensity;
    //uv.x += sway;

    // 1. Get the direction from the center to the current pixel
    vec2 dirFromCenter = local - vec2(0.5);
    float distFromCenter = length(dirFromCenter);

    // 2. Create the bias (0.0 at center, increases towards edges)
    // We multiply by 2.0 so the edges of the sprite get full distortion
    float edgeBias = smoothstep(0.0, 0.5, distFromCenter);

    // 3. Calculate the sway amount based on your sine waves
    float swayAmount = wave * uDistortionAmount.x * edgeBias * mask * uIntensity;

    // 4. Apply the sway
    // For top-down, swaying looks best if it pushes along the X axis
    // or follows the wind direction.
    uv.x += swayAmount;
    //uv += (dirFromCenter * swayAmount);

    vec4 texel = texture(texture0, uv);
    texel.a *= mask;

    finalColor = texel * colDiffuse * fragColor;
    finalColor.rgb *= finalColor.a;
}
