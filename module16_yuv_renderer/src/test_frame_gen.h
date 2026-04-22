#pragma once
#include <cstdint>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// 生成 YUV420P 测试帧（彩色条纹 + 动态亮度波动）
// 参数：
//   y, u, v    — 输出缓冲区（调用方分配：y=w*h, u=w/2*h/2, v=w/2*h/2）
//   width, height — 帧分辨率
//   frame_idx  — 帧序号（用于动态效果）
// ─────────────────────────────────────────────────────────────────────────────
inline void gen_yuv_frame(uint8_t* y_plane,
                          uint8_t* u_plane,
                          uint8_t* v_plane,
                          int width, int height, int frame_idx)
{
    const int uv_w = width  / 2;
    const int uv_h = height / 2;

    // 8 色竖条纹（BT.601 Studio Swing YUV 值）
    struct StripColor { uint8_t y, u, v; };
    static const StripColor STRIPS[8] = {
        {235, 128, 128},  // 白
        {210, 16,  146},  // 黄
        {169, 166, 16 },  // 青
        {145, 54,  34 },  // 绿
        {106, 202, 222},  // 紫
        { 81,  90, 240},  // 红
        { 41, 240, 110},  // 蓝
        { 16, 128, 128},  // 黑
    };

    int stripe_w = width / 8;
    float wave = sinf((float)frame_idx * 0.05f) * 20.0f;  // 动态亮度

    // ── Y 平面 ──
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int  s = col / stripe_w;
            if (s >= 8) s = 7;
            int  luma = STRIPS[s].y + (int)wave;
            if (luma < 16)  luma = 16;
            if (luma > 235) luma = 235;
            y_plane[row * width + col] = (uint8_t)luma;
        }
    }

    // ── U、V 平面（色度二次采样，每 2×2 像素共享一个 UV）──
    for (int row = 0; row < uv_h; ++row) {
        for (int col = 0; col < uv_w; ++col) {
            int s = (col * 2) / stripe_w;
            if (s >= 8) s = 7;
            u_plane[row * uv_w + col] = STRIPS[s].u;
            v_plane[row * uv_w + col] = STRIPS[s].v;
        }
    }
}
