#version 460 core

uniform sampler2D uTexY;
uniform sampler2D uTexU;
uniform sampler2D uTexV;
uniform int uColorSpace;  // 0=BT.601, 1=BT.709
uniform int uRange;       // 0=Studio Swing (16-235), 1=Full Range (0-255)

in  vec2 vTexCoords;
out vec4 FragColor;

void main() {
    // ── 采样三平面 ──────────────────────────────────────────────────────
    float y = texture(uTexY, vTexCoords).r;
    float u = texture(uTexU, vTexCoords).r;
    float v = texture(uTexV, vTexCoords).r;

    // ── 范围归一化 ──────────────────────────────────────────────────────
    if (uRange == 0) {
        // Studio Swing：Y ∈ [16/255, 235/255]，Cb/Cr ∈ [16/255, 240/255]，中心 128/255
        y = (y - 16.0/255.0) / (219.0/255.0);
        u = (u - 128.0/255.0) / (112.0/255.0);
        v = (v - 128.0/255.0) / (112.0/255.0);
    } else {
        // Full Range：Y ∈ [0,1]，Cb/Cr ∈ [0,1]，中心 0.5
        u -= 0.5;
        v -= 0.5;
    }

    // ── YCbCr → RGB 转换矩阵 ────────────────────────────────────────────
    vec3 rgb;
    if (uColorSpace == 0) {
        // BT.601（SD 标准，如 DVD、标准广播）
        // R = Y              + 1.402   * Cr
        // G = Y - 0.344136 * Cb - 0.714136 * Cr
        // B = Y + 1.772   * Cb
        rgb.r = y                      + 1.402    * v;
        rgb.g = y - 0.344136 * u - 0.714136 * v;
        rgb.b = y + 1.772    * u;
    } else {
        // BT.709（HD 标准，如 1080p 视频）
        // R = Y              + 1.5748  * Cr
        // G = Y - 0.1873   * Cb - 0.4681  * Cr
        // B = Y + 1.8556  * Cb
        rgb.r = y                     + 1.5748  * v;
        rgb.g = y - 0.1873  * u - 0.4681  * v;
        rgb.b = y + 1.8556  * u;
    }

    FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
