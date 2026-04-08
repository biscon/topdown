#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 uTexelSize;

out vec4 finalColor;

void main()
{
    vec2 uv = fragTexCoord;

    vec4 c = vec4(0.0);

    c += texture(texture0, uv + uTexelSize * vec2(-1.0, -1.0)) * 0.0400;
    c += texture(texture0, uv + uTexelSize * vec2( 0.0, -1.0)) * 0.0800;
    c += texture(texture0, uv + uTexelSize * vec2( 1.0, -1.0)) * 0.0400;

    c += texture(texture0, uv + uTexelSize * vec2(-1.0,  0.0)) * 0.0800;
    c += texture(texture0, uv + uTexelSize * vec2( 0.0,  0.0)) * 0.5200;
    c += texture(texture0, uv + uTexelSize * vec2( 1.0,  0.0)) * 0.0800;

    c += texture(texture0, uv + uTexelSize * vec2(-1.0,  1.0)) * 0.0400;
    c += texture(texture0, uv + uTexelSize * vec2( 0.0,  1.0)) * 0.0800;
    c += texture(texture0, uv + uTexelSize * vec2( 1.0,  1.0)) * 0.0400;

    finalColor = c * fragColor;
}