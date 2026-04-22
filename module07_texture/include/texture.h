#pragma once
#include <glad/glad.h>
#include <string>

// 2D 纹理封装
class Texture2D {
public:
    GLuint id{0};
    int width{0}, height{0}, channels{0};

    // 从文件加载（stb_image），flip_y=true 翻转（OpenGL 纹理原点在左下）
    static Texture2D from_file(const char* path, bool flip_y = true, bool srgb = false);

    // 创建空纹理（用于 FBO 附件，后续模块用）
    static Texture2D create_empty(int w, int h, GLenum internal_fmt, GLenum fmt, GLenum type);

    // 绑定到纹理单元 slot（0-15）
    void bind(int slot = 0) const;

    void destroy();
};
