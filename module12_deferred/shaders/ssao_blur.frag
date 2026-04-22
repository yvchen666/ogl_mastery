#version 460 core
// SSAO blur: simple 4x4 box filter to reduce noise
in  vec2  vTexCoord;
out float FragOcclusion;

uniform sampler2D uSSAOInput;

void main() {
    vec2 texelSize = 1.0 / textureSize(uSSAOInput, 0);
    float result = 0.0;
    for (int x = -2; x <= 1; ++x)
        for (int y = -2; y <= 1; ++y)
            result += texture(uSSAOInput, vTexCoord + vec2(x, y) * texelSize).r;
    FragOcclusion = result / 16.0;
}
