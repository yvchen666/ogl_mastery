#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec4 FragPosLightSpace;
} fs_in;

out vec4 FragColor;

uniform sampler2D uShadowMap;
uniform vec3  uLightDir;     // normalized, pointing FROM fragment TO light
uniform vec3  uLightColor;
uniform vec3  uViewPos;
uniform vec3  uObjectColor;

// Shadow mode: 0 = hard, 1 = PCF, 2 = PCSS
uniform int   uShadowMode;

// PCSS light size in light-space texels
uniform float uLightSize;

// ──────────────────────────────────────────────────────────────────────────────
// Poisson disk samples (16 points)
const vec2 POISSON16[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

// Random rotation angle based on fragment position (breaks Poisson regularity)
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// ──────────────────────────────────────────────────────────────────────────────
// Hard shadow
float shadow_hard(vec4 fragPosLS, float bias) {
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    return (projCoords.z - bias) > closestDepth ? 1.0 : 0.0;
}

// ──────────────────────────────────────────────────────────────────────────────
// PCF with rotated Poisson disk
float shadow_pcf(vec4 fragPosLS, float bias, int kernelSize) {
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float angle    = rand(projCoords.xy) * 6.28318;
    float cosA     = cos(angle);
    float sinA     = sin(angle);

    float shadow = 0.0;
    int   count  = 0;
    for (int i = 0; i < kernelSize && i < 16; ++i) {
        vec2 offset = vec2(
            cosA * POISSON16[i].x - sinA * POISSON16[i].y,
            sinA * POISSON16[i].x + cosA * POISSON16[i].y
        ) * texelSize * 2.0;
        float sampleDepth = texture(uShadowMap, projCoords.xy + offset).r;
        shadow += (projCoords.z - bias) > sampleDepth ? 1.0 : 0.0;
        ++count;
    }
    return shadow / float(count);
}

// ──────────────────────────────────────────────────────────────────────────────
// PCSS: Blocker search → penumbra size → variable-kernel PCF
float shadow_pcss(vec4 fragPosLS, float bias) {
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float receiverDepth = projCoords.z;
    vec2  texelSize     = 1.0 / textureSize(uShadowMap, 0);

    // ── Step 1: Blocker search (average depth of occluders within search radius) ──
    float searchRadius = uLightSize * (receiverDepth - 0.1) / receiverDepth;
    float blockerSum   = 0.0;
    int   blockerCount = 0;
    float angle = rand(projCoords.xy) * 6.28318;
    float cosA  = cos(angle), sinA = sin(angle);

    for (int i = 0; i < 16; ++i) {
        vec2 offset = vec2(
            cosA * POISSON16[i].x - sinA * POISSON16[i].y,
            sinA * POISSON16[i].x + cosA * POISSON16[i].y
        ) * texelSize * searchRadius * 20.0;

        float sDepth = texture(uShadowMap, projCoords.xy + offset).r;
        if ((receiverDepth - bias) > sDepth) {
            blockerSum += sDepth;
            ++blockerCount;
        }
    }

    if (blockerCount == 0) return 0.0; // fully lit
    float avgBlocker = blockerSum / float(blockerCount);

    // ── Step 2: Penumbra width ──────────────────────────────────────────────
    // w_penumbra = (d_receiver - d_blocker) / d_blocker * w_light
    float penumbraWidth = (receiverDepth - avgBlocker) / avgBlocker * uLightSize;

    // ── Step 3: Variable-kernel PCF ─────────────────────────────────────────
    float shadow = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = vec2(
            cosA * POISSON16[i].x - sinA * POISSON16[i].y,
            sinA * POISSON16[i].x + cosA * POISSON16[i].y
        ) * texelSize * penumbraWidth * 30.0;

        float sDepth = texture(uShadowMap, projCoords.xy + offset).r;
        shadow += (receiverDepth - bias) > sDepth ? 1.0 : 0.0;
    }
    return shadow / 16.0;
}

// ──────────────────────────────────────────────────────────────────────────────
void main() {
    vec3 N    = normalize(fs_in.Normal);
    vec3 L    = normalize(uLightDir);
    vec3 V    = normalize(uViewPos - fs_in.FragPos);
    vec3 H    = normalize(L + V);

    // Optimal bias: avoids shadow acne while minimising Peter Panning
    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);

    float shadow = 0.0;
    if      (uShadowMode == 0) shadow = shadow_hard(fs_in.FragPosLightSpace, bias);
    else if (uShadowMode == 1) shadow = shadow_pcf (fs_in.FragPosLightSpace, bias, 16);
    else                        shadow = shadow_pcss(fs_in.FragPosLightSpace, bias);

    float ambient  = 0.15;
    float diffuse  = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 64.0) * 0.5;

    vec3 color = uObjectColor * (ambient + (1.0 - shadow) * (diffuse + specular)) * uLightColor;
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
