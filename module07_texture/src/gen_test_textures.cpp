// gen_test_textures.cpp
// 生成两张测试纹理并保存为 PPM（或写入原始 RGBA 供 stb_image_write 使用）
// 用法：编译后直接运行，输出 assets/checkerboard.png 和 assets/gradient.png

// 使用 stb_image_write 写 PNG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <iostream>

static void gen_checkerboard(const char* path,
                              int width, int height,
                              int cell_size = 16) {
    std::vector<uint8_t> pixels(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int cx = x / cell_size;
            int cy = y / cell_size;
            bool white = (cx + cy) % 2 == 0;
            int idx = (y * width + x) * 3;
            pixels[idx+0] = white ? 255 : 30;
            pixels[idx+1] = white ? 255 : 30;
            pixels[idx+2] = white ? 255 : 30;
        }
    }
    if (stbi_write_png(path, width, height, 3, pixels.data(), width * 3))
        std::cout << "Written: " << path << "\n";
    else
        std::cerr << "Failed to write: " << path << "\n";
}

static void gen_gradient(const char* path, int width, int height) {
    std::vector<uint8_t> pixels(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = (float)x / (width  - 1);
            float v = (float)y / (height - 1);
            // hue rotation trick: red→green→blue
            float r = 0.5f + 0.5f * std::cos(2.0f * 3.14159f * (u + 0.0f/3.0f));
            float g = 0.5f + 0.5f * std::cos(2.0f * 3.14159f * (u + 1.0f/3.0f));
            float b = 0.5f + 0.5f * std::cos(2.0f * 3.14159f * (u + 2.0f/3.0f));
            float bright = 0.4f + 0.6f * v;
            int idx = (y * width + x) * 3;
            pixels[idx+0] = (uint8_t)(r * bright * 255);
            pixels[idx+1] = (uint8_t)(g * bright * 255);
            pixels[idx+2] = (uint8_t)(b * bright * 255);
        }
    }
    if (stbi_write_png(path, width, height, 3, pixels.data(), width * 3))
        std::cout << "Written: " << path << "\n";
    else
        std::cerr << "Failed to write: " << path << "\n";
}

int main() {
    gen_checkerboard("assets/checkerboard.png", 256, 256, 16);
    gen_gradient    ("assets/gradient.png",     256, 256);
    return 0;
}
