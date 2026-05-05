#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 uSceneSize;
uniform vec2 uRegionPos;
uniform vec2 uRegionSize;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform vec3 uTint;
uniform float uSoftness;

uniform int uUsePolygon;
uniform int uPolygonVertexCount;
uniform vec2 uPolygonPoints[32];

out vec4 finalColor;

#include "../include/polygon_region.glsl"

vec3 applySaturation(vec3 color, float saturation)
{
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(luma), color, saturation);
}

void main()
{
    vec2 pixelPos = vec2(gl_FragCoord.x, uSceneSize.y - gl_FragCoord.y);
    vec2 local = (pixelPos - uRegionPos) / uRegionSize;

    vec4 original = texture(texture0, fragTexCoord);

    float mask = regionMask(pixelPos, local, uSoftness);
    if (mask <= 0.0001) {
        finalColor = original;
        return;
    }

    vec3 graded = original.rgb;

    graded += vec3(uBrightness);
    graded = ((graded - 0.5) * uContrast) + 0.5;
    graded = applySaturation(graded, uSaturation);
    graded *= uTint;
    graded = clamp(graded, 0.0, 1.0);

    finalColor = vec4(mix(original.rgb, graded, mask), original.a);
}
