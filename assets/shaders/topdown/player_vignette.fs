#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 uResolution;
uniform float uTime;
uniform float uDamageFlash;
uniform float uLowHealthWeight;

out vec4 finalColor;

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

void main()
{
    vec4 base = texture(texture0, fragTexCoord) * fragColor;

    vec2 p = fragTexCoord * 2.0 - 1.0;

    float aspect = uResolution.x / max(uResolution.y, 1.0);
    p.x *= aspect * 0.45;

    float dist = length(p);

    // Big overall vignette shape.
    float bigMask = smoothstep(0.78, 1.18, dist);

    // Outer darkness spread over a wide range so it does not look flat.
    float outerMask = smoothstep(0.74, 1.18, dist);

    // Inner red shoulder, not a ring:
    // rises gently, then fades out before the center.
    float innerRise = smoothstep(0.74, 0.92, dist);
    float innerFade = 1.0 - smoothstep(0.96, 1.10, dist);
    float innerMask = innerRise * innerFade;

    float damagePulse = saturate(uDamageFlash);
    damagePulse = damagePulse * damagePulse;
    damagePulse = mix(damagePulse, 1.0, 0.12);

    float heartbeat = 0.5 + 0.5 * sin(uTime * 2.8);
    heartbeat = pow(heartbeat, 1.25);

    float lowHealthStart = 0.50;   // below 50% HP
    float lowHealthFull  = 0.85;   // full pulse near 15% HP
    float lowHealthActive =
        saturate((uLowHealthWeight - lowHealthStart) / (lowHealthFull - lowHealthStart));
    lowHealthActive = lowHealthActive * lowHealthActive;
    float lowHealthPulse = lowHealthActive * mix(0.68, 1.00, heartbeat);


    float pulseStrength = max(
        damagePulse * 0.40,
        lowHealthPulse * 0.95
    );
    pulseStrength = saturate(pulseStrength);

    // Make the outer fade stronger and broader.
    float outerAlpha = outerMask * pulseStrength * 0.72;

    // Reduce this so it reads as a shoulder, not a glowing seam.
    float innerAlpha = innerMask * pulseStrength * 0.10;

    // Broad red body to tie the layers together.
    float bodyAlpha = bigMask * pulseStrength * 0.20;

    vec3 darkRed = vec3(0.30, 0.01, 0.01);
    vec3 midRed    = vec3(0.45, 0.02, 0.02);
    vec3 brightRed = vec3(0.62, 0.03, 0.03);

    vec3 overlayPremul =
            darkRed   * outerAlpha +
            midRed    * bodyAlpha +
            brightRed * innerAlpha;

    float totalAlpha = saturate(outerAlpha + bodyAlpha + innerAlpha);

    vec3 outRgb = base.rgb * (1.0 - totalAlpha) + overlayPremul;

    finalColor = vec4(outRgb, base.a);
}