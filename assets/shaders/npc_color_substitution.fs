#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

uniform vec3 uSkinSrc[3];
uniform vec3 uHairSrc[3];
uniform vec3 uChestSrc[5];
uniform vec3 uLegsSrc[4];

uniform vec3 uSkinDst[3];
uniform vec3 uHairDst[3];
uniform vec3 uChestDst[5];
uniform vec3 uLegsDst[4];

uniform float uTolerance;

out vec4 finalColor;

bool matchColor(vec3 a, vec3 b)
{
    return distance(a, b) <= uTolerance;
}

void main()
{
    vec4 tex = texture(texture0, fragTexCoord);
    vec3 rgb = tex.rgb;

    for (int i = 0; i < 3; ++i) {
        if (matchColor(rgb, uSkinSrc[i])) {
            rgb = uSkinDst[i];
            finalColor = vec4(rgb, tex.a) * fragColor;
            return;
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (matchColor(rgb, uHairSrc[i])) {
            rgb = uHairDst[i];
            finalColor = vec4(rgb, tex.a) * fragColor;
            return;
        }
    }

    for (int i = 0; i < 5; ++i) {
        if (matchColor(rgb, uChestSrc[i])) {
            rgb = uChestDst[i];
            finalColor = vec4(rgb, tex.a) * fragColor;
            return;
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (matchColor(rgb, uLegsSrc[i])) {
            rgb = uLegsDst[i];
            finalColor = vec4(rgb, tex.a) * fragColor;
            return;
        }
    }

    finalColor = tex * fragColor;
}
