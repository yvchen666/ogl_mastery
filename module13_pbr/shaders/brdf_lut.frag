#version 460 core

in vec2 vUV;
out vec4 FragColor;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;

// ── Van der Corput + Hammersley ───────────────────────────────────────────────
float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radical_inverse_vdc(i));
}

// ── Importance-sample GGX ─────────────────────────────────────────────────────
vec3 importance_sample_ggx(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi       = 2.0 * PI * Xi.x;
    float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    vec3 H = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// ── Geometry function (IBL variant: k = α²/2) ─────────────────────────────────
// For IBL we use k_IBL = α²/2 (different from direct k_direct = (α+1)²/8)
// Reason: no hotspot correction needed for environment light
float geometry_schlick_ggx_ibl(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith_ibl(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx_ibl(NdotV, roughness) *
           geometry_schlick_ggx_ibl(NdotL, roughness);
}

// ── Integrate BRDF ────────────────────────────────────────────────────────────
// Input:  vUV.x = NdotV,  vUV.y = roughness
// Output: rg = (scale, bias) such that ∫ fr·cosθ dω ≈ F0·scale + bias
vec2 integrate_brdf(float NdotV, float roughness) {
    // Reconstruct V from NdotV (assume N = (0,0,1))
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importance_sample_ggx(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G     = geometry_smith_ibl(N, V, L, roughness);
            // G_vis removes the denominator factor and pdf
            float G_vis = (G * VdotH) / (NdotH * NdotV);
            // Fc = (1 - VdotH)^5  (Schlick Fresnel term without F0)
            float Fc    = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_vis;
            B += Fc          * G_vis;
        }
    }
    return vec2(A, B) / float(SAMPLE_COUNT);
}

void main() {
    vec2 brdf  = integrate_brdf(vUV.x, vUV.y);
    FragColor  = vec4(brdf, 0.0, 1.0);
}
