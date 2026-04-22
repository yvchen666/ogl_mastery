#version 460 core
// SSAO computation pass
in vec2 vTexCoord;
out float FragOcclusion;

uniform sampler2D uGPosition;  // world-space position
uniform sampler2D uGNormal;    // world-space normal
uniform sampler2D uNoiseTex;   // 4x4 random rotation vectors

uniform vec3  uSamples[64];    // hemisphere kernel
uniform mat4  uProjection;
uniform mat4  uView;
uniform vec2  uScreenSize;

const float RADIUS  = 0.5;
const float BIAS    = 0.025;

void main() {
    vec2 noiseScale = uScreenSize / 4.0; // tile the 4x4 noise

    vec3 fragPos = texture(uGPosition, vTexCoord).xyz;
    vec3 normal  = normalize(texture(uGNormal, vTexCoord).xyz);
    vec3 randVec = normalize(texture(uNoiseTex, vTexCoord * noiseScale).xyz);

    // Build TBN matrix to transform samples from tangent space → world space
    vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 64; ++i) {
        // Sample position in world space
        vec3 samplePos = fragPos + TBN * uSamples[i] * RADIUS;

        // Project sample to screen space to read G-Buffer depth
        vec4 offset = uProjection * uView * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        // Get the depth of the scene geometry at the projected coordinate
        float sampleDepth = texture(uGPosition, offset.xy).z;

        // Range check: only occlude if the geometry is close enough (avoid leaking)
        float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(fragPos.z - sampleDepth));

        // Occlusion: sample is behind geometry? (with bias to avoid self-occlusion)
        occlusion += (sampleDepth >= samplePos.z + BIAS ? 1.0 : 0.0) * rangeCheck;
    }

    FragOcclusion = 1.0 - (occlusion / 64.0);
}
