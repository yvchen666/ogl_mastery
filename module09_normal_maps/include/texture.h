#pragma once
#include <glad/glad.h>
#include <string>

// 2D 纹理封装（module09 自带，与 module07 相同接口）
class Texture2D {
public:
    GLuint id{0};
    int width{0}, height{0}, channels{0};

    static Texture2D from_file(const char* path, bool flip_y = true, bool srgb = false);
    static Texture2D create_empty(int w, int h, GLenum internal_fmt, GLenum fmt, GLenum type);
    void bind(int slot = 0) const;
    void destroy();
};
