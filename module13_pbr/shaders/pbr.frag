#version 460 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

out vec4 FragColor;

// Material parameters
uniform vec3  uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAo;

// Lights
uniform vec3 uLightPositions[4];
uniform vec3 uLightColors[4];

// Camera
uniform vec3 uCamPos;

// IBL
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D   uBrdfLut;

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// ── GGX / Trowbridge-Reitz NDF ────────────────────────────────────────────────
// D(h) = α² / (π · ((n·h)²·(α²-1) + 1)²)
// α = roughness²  (perceptually linear)
float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a     = roughness * roughness;
    float a2    = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// ── Smith geometry (Schlick-GGX) ──────────────────────────────────────────────
// G_sub(n,v,k) = (n·v) / ((n·v)·(1-k) + k)
// k_direct = (roughness+1)² / 8
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(NdotV, roughness) *
           geometry_schlick_ggx(NdotL, roughness);
}

// ── Fresnel (Schlick) ─────────────────────────────────────────────────────────
// F(v,h) = F0 + (1 - F0) · (1 - v·h)^5
vec3 fresnel_schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel with roughness (for IBL ambient)
vec3 fresnel_schlick_roughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 R = reflect(-V, N);

    // Metallic workflow: F0 = 0.04 for dielectrics, albedo for metals
    vec3 F0 = mix(vec3(0.04), uAlbedo, uMetallic);

    // ── Direct lighting ───────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        vec3  L           = normalize(uLightPositions[i] - vWorldPos);
        vec3  H           = normalize(V + L);
        float dist        = length(uLightPositions[i] - vWorldPos);
        float attenuation = 1.0 / (dist * dist);
        vec3  radiance    = uLightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float D = distribution_ggx(N, H, uRoughness);
        float G = geometry_smith(N, V, L, uRoughness);
        vec3  F = fresnel_schlick(max(dot(H, V), 0.0), F0);

        // kS = F, kD = 1 - kS, metal has no diffuse
        vec3  kD       = (1.0 - F) * (1.0 - uMetallic);
        float NdotL    = max(dot(N, L), 0.0);
        float NdotV    = max(dot(N, V), 0.0);
        float denom    = 4.0 * NdotV * NdotL + 0.0001;
        vec3  specular = (D * G * F) / denom;

        Lo += (kD * uAlbedo / PI + specular) * radiance * NdotL;
    }

    // ── IBL ambient ───────────────────────────────────────────────────────────
    float NdotV = max(dot(N, V), 0.0);
    vec3  F_amb = fresnel_schlick_roughness(NdotV, F0, uRoughness);
    vec3  kD_amb = (1.0 - F_amb) * (1.0 - uMetallic);

    // Diffuse IBL: irradiance map
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 diffuse_ibl = irradiance * uAlbedo * kD_amb;

    // Specular IBL: split-sum approximation
    // Part 1: pre-filtered env map (roughness -> mip level)
    vec3 prefiltered = textureLod(uPrefilterMap, R, uRoughness * MAX_REFLECTION_LOD).rgb;
    // Part 2: BRDF integration LUT
    vec2 brdf = texture(uBrdfLut, vec2(NdotV, uRoughness)).rg;
    vec3 specular_ibl = prefiltered * (F_amb * brdf.r + brdf.g);

    vec3 ambient = (diffuse_ibl + specular_ibl) * uAo;
    vec3 color   = ambient + Lo;

    // HDR tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
