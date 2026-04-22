#version 460 core

in vec3 vLocalPos;
out vec4 FragColor;

uniform samplerCube uEnvMap;
uniform float       uRoughness;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;

// ── Van der Corput sequence (bit reversal) ────────────────────────────────────
float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// ── Hammersley 2D low-discrepancy sequence ────────────────────────────────────
vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radical_inverse_vdc(i));
}

// ── Importance-sample GGX distribution ───────────────────────────────────────
// Given uniform sample (Xi), return a half-vector H weighted by GGX NDF.
// From: θ = atan(α · sqrt(ξ₁) / sqrt(1 − ξ₁))
// This is the inverse CDF of the GGX NDF marginal distribution over θ.
vec3 importance_sample_ggx(vec2 Xi, vec3 N, float roughness) {
    float a  = roughness * roughness;

    float phi      = 2.0 * PI * Xi.x;
    float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    // Half-vector in tangent space
    vec3 H = vec3(cos(phi) * sin_theta,
                  sin(phi) * sin_theta,
                  cos_theta);

    // Tangent space to world
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent  = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// ── GGX NDF ───────────────────────────────────────────────────────────────────
float distribution_ggx(float NdotH, float roughness) {
    float a2    = roughness * roughness * roughness * roughness;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

void main() {
    vec3 N = normalize(vLocalPos);
    // Assume V = R = N (isotropic assumption for pre-filtering)
    vec3 R = N;
    vec3 V = R;

    float total_weight   = 0.0;
    vec3  prefiltered_color = vec3(0.0);

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importance_sample_ggx(Xi, N, uRoughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            // Use mip-biased sample to avoid bright spot artifacts
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float D     = distribution_ggx(NdotH, uRoughness);
            float pdf   = (D * NdotH / (4.0 * HdotV)) + 0.0001;

            // Resolution of source cubemap (approximate)
            float resolution = 512.0;
            float sa_texel   = 4.0 * PI / (6.0 * resolution * resolution);
            float sa_sample  = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float mip_level  = uRoughness == 0.0 ? 0.0
                               : 0.5 * log2(sa_sample / sa_texel);

            prefiltered_color += textureLod(uEnvMap, L, mip_level).rgb * NdotL;
            total_weight      += NdotL;
        }
    }

    prefiltered_color /= total_weight;
    FragColor = vec4(prefiltered_color, 1.0);
}
