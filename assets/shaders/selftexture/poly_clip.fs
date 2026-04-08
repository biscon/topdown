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
uniform vec3 uTint;

uniform int uUsePolygon;
uniform int uPolygonVertexCount;
uniform vec2 uPolygonPoints[32];

uniform int uUseOcclusionPolygon;
uniform int uOcclusionPolygonVertexCount;
uniform vec2 uOcclusionPolygonPoints[64];

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

    vec4 texel = texture(texture0, fragTexCoord);
    texel.rgb *= uTint;
    texel.a *= mask;

    finalColor = texel * colDiffuse * fragColor;
    finalColor.rgb *= finalColor.a;
}
