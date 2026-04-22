#version 460 core
// Deferred lighting pass: iterate all point lights using G-Buffer data
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uGPosition;
uniform sampler2D uGNormal;
uniform sampler2D uGAlbedoSpec;
uniform sampler2D uSSAO;         // blurred occlusion factor

struct PointLight {
    vec3  position;
    vec3  color;
    float linear;
    float quadratic;
};
uniform PointLight uLights[32];
uniform int        uNumLights;
uniform vec3       uViewPos;
uniform int        uVisMode;     // 0=shading, 1=position, 2=normal, 3=albedo, 4=ssao

void main() {
    vec3  fragPos    = texture(uGPosition,    vTexCoord).rgb;
    vec3  normal     = normalize(texture(uGNormal, vTexCoord).rgb);
    vec4  albedoSpec = texture(uGAlbedoSpec,  vTexCoord);
    vec3  albedo     = albedoSpec.rgb;
    float specPower  = albedoSpec.a;
    float ao         = texture(uSSAO, vTexCoord).r;

    // ── G-Buffer visualization modes ─────────────────────────────────────────
    if      (uVisMode == 1) { FragColor = vec4(fragPos * 0.1 + 0.5, 1.0); return; }
    else if (uVisMode == 2) { FragColor = vec4(normal * 0.5 + 0.5, 1.0); return; }
    else if (uVisMode == 3) { FragColor = vec4(albedo, 1.0); return; }
    else if (uVisMode == 4) { FragColor = vec4(vec3(ao), 1.0); return; }

    // ── Blinn-Phong accumulation over all point lights ────────────────────────
    vec3 V       = normalize(uViewPos - fragPos);
    vec3 ambient = 0.3 * albedo * ao;  // AO modulates ambient
    vec3 result  = ambient;

    for (int i = 0; i < uNumLights; ++i) {
        vec3  L    = normalize(uLights[i].position - fragPos);
        vec3  H    = normalize(L + V);
        float dist = length(uLights[i].position - fragPos);
        float att  = 1.0 / (1.0 + uLights[i].linear * dist + uLights[i].quadratic * dist * dist);

        float diff  = max(dot(normal, L), 0.0);
        float spec  = pow(max(dot(normal, H), 0.0), 16.0) * specPower;

        result += (diff * albedo + spec) * uLights[i].color * att;
    }

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));
    FragColor = vec4(result, 1.0);
}
