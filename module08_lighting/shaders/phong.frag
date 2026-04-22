#version 460 core

in vec3 vFragPos;
in vec3 vNormal;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uViewPos;

// ── Light structures ──────────────────────────────────────────────────────────
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3  position;
    float constant;
    float linear;
    float quadratic;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
};

struct SpotLight {
    vec3  position;
    vec3  direction;
    float cutOff;       // cos of inner cone angle
    float outerCutOff;  // cos of outer cone angle
    float constant;
    float linear;
    float quadratic;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
};

uniform DirLight   uDirLight;
uniform PointLight uPointLights[4];
uniform int        uNumPointLights;
uniform SpotLight  uSpotLight;
uniform bool       uUseSpotLight;

// Material
uniform vec3  uMatAmbient;
uniform vec3  uMatDiffuse;
uniform vec3  uMatSpecular;
uniform float uMatShininess;

// ── Light type selector (0=dir, 1=point, 2=spot) ──────────────────────────
uniform int uLightMode;

// ── Helper functions ──────────────────────────────────────────────────────────
vec3 calc_dir_light(DirLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    // diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    // specular (Blinn-Phong: half-vector)
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), uMatShininess);
    vec3 ambient  = light.ambient  * uMatAmbient;
    vec3 diffuse  = light.diffuse  * diff * uMatDiffuse;
    vec3 specular = light.specular * spec * uMatSpecular;
    return ambient + diffuse + specular;
}

vec3 calc_point_light(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), uMatShininess);

    float d = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * d + light.quadratic * d * d);

    vec3 ambient  = light.ambient  * uMatAmbient  * attenuation;
    vec3 diffuse  = light.diffuse  * diff * uMatDiffuse  * attenuation;
    vec3 specular = light.specular * spec * uMatSpecular * attenuation;
    return ambient + diffuse + specular;
}

vec3 calc_spot_light(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), uMatShininess);

    float d = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * d + light.quadratic * d * d);

    // Soft edge: theta is angle between lightDir and spot direction
    float theta   = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    // smoothstep gives 0 at outerCutOff, 1 at cutOff
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient  = light.ambient  * uMatAmbient;
    vec3 diffuse  = light.diffuse  * diff * uMatDiffuse  * attenuation * intensity;
    vec3 specular = light.specular * spec * uMatSpecular * attenuation * intensity;
    return ambient + diffuse + specular;
}

void main() {
    vec3 norm    = normalize(vNormal);
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 result  = vec3(0.0);

    if (uLightMode == 0) {
        // Directional light only
        result = calc_dir_light(uDirLight, norm, viewDir);
    } else if (uLightMode == 1) {
        // All point lights
        for (int i = 0; i < uNumPointLights; ++i)
            result += calc_point_light(uPointLights[i], norm, vFragPos, viewDir);
    } else if (uLightMode == 2) {
        // Spot light
        result = calc_spot_light(uSpotLight, norm, vFragPos, viewDir);
    } else {
        // All combined
        result += calc_dir_light(uDirLight, norm, viewDir);
        for (int i = 0; i < uNumPointLights; ++i)
            result += calc_point_light(uPointLights[i], norm, vFragPos, viewDir);
        if (uUseSpotLight)
            result += calc_spot_light(uSpotLight, norm, vFragPos, viewDir);
    }

    FragColor = vec4(result, 1.0);
}
