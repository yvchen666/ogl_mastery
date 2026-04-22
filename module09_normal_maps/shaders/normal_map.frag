#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} fs_in;

out vec4 FragColor;

uniform sampler2D uDiffuseMap;
uniform sampler2D uNormalMap;
uniform sampler2D uDepthMap;   // 用于视差贴图 (height map, 灰度)

uniform bool  uUseNormalMap;
uniform bool  uUseParallax;
uniform float uHeightScale;

// ── 视差遮蔽映射（POM）────────────────────────────────────────────────────────
vec2 parallax_mapping(vec2 texCoords, vec3 viewDir) {
    // Steep Parallax Mapping：分层搜索
    const int   MIN_LAYERS = 8;
    const int   MAX_LAYERS = 32;
    // 更多层次用于掠射角（|viewDir.z| 小时）
    int numLayers = int(mix(float(MAX_LAYERS), float(MIN_LAYERS),
                            abs(viewDir.z)));

    float layerDepth  = 1.0 / float(numLayers);
    float currentLayerDepth = 0.0;

    // 每层移动量（负号因为视线向表面内部）
    vec2 deltaUV = -viewDir.xy * uHeightScale / (viewDir.z * float(numLayers));

    vec2  currentUV    = texCoords;
    float currentDepth = texture(uDepthMap, currentUV).r;

    // 步进直到深度图深度 <= 光线深度
    while (currentLayerDepth < currentDepth) {
        currentUV         += deltaUV;
        currentDepth       = texture(uDepthMap, currentUV).r;
        currentLayerDepth += layerDepth;
    }

    // 视差遮蔽映射（POM）：在前后两个交叉点之间插值
    vec2  prevUV    = currentUV - deltaUV;
    float afterDepth  = currentDepth - currentLayerDepth;
    float beforeDepth = texture(uDepthMap, prevUV).r
                        - (currentLayerDepth - layerDepth);
    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(currentUV, prevUV, weight);
}

void main() {
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec2 uv = fs_in.TexCoords;

    // 视差偏移 UV
    if (uUseParallax) {
        uv = parallax_mapping(uv, viewDir);
        // 丢弃超出 [0,1] 的 UV（边缘伪影）
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            discard;
    }

    // 采样法线贴图
    vec3 normal;
    if (uUseNormalMap) {
        normal = texture(uNormalMap, uv).rgb;
        // [0,1] → [-1,1]（切线空间法线，蓝色通道=Z=朝外）
        normal = normalize(normal * 2.0 - 1.0);
    } else {
        // 无法线贴图：用默认切线空间朝外法线 (0,0,1)
        normal = vec3(0.0, 0.0, 1.0);
    }

    // Blinn-Phong（所有计算已在切线空间）
    vec3 diffColor = texture(uDiffuseMap, uv).rgb;
    vec3 lightDir  = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    vec3 halfDir   = normalize(lightDir + viewDir);

    vec3 ambient  = 0.1 * diffColor;
    float diff    = max(dot(normal, lightDir), 0.0);
    vec3 diffuse  = diff * diffColor;
    float spec    = pow(max(dot(normal, halfDir), 0.0), 64.0);
    vec3 specular = vec3(0.2) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
