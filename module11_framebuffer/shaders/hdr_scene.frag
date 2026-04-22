#version 460 core

in  vec3 FragPos;
in  vec3 Normal;
out vec4 FragColor;

// Multiple point lights with HDR intensities
struct Light { vec3 position; vec3 color; };
uniform Light uLights[4];
uniform vec3  uViewPos;
uniform vec3  uObjectColor;

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(uViewPos - FragPos);

    vec3 result = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        vec3 L    = normalize(uLights[i].position - FragPos);
        vec3 H    = normalize(L + V);
        float d   = length(uLights[i].position - FragPos);
        float att = 1.0 / (d * d + 0.01); // HDR: no clamp, distance attenuation

        float diff  = max(dot(N, L), 0.0);
        float spec  = pow(max(dot(N, H), 0.0), 32.0);
        result += (diff * uObjectColor + spec * vec3(1.0)) * uLights[i].color * att;
    }
    result += 0.03 * uObjectColor; // ambient

    // Output is HDR (values can exceed 1.0) — tone mapping done in post-process pass
    FragColor = vec4(result, 1.0);
}
