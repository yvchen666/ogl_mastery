// STB_IMAGE_IMPLEMENTATION must be defined in exactly one translation unit
// The CMakeLists.txt adds -DSTB_IMAGE_IMPLEMENTATION for the target
#include <stb_image.h>
#include "texture.h"
#include <iostream>

Texture2D Texture2D::from_file(const char* path, bool flip_y, bool srgb) {
    Texture2D tex;
    stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

    unsigned char* data = stbi_load(path, &tex.width, &tex.height, &tex.channels, 0);
    if (!data) {
        std::cerr << "[Texture2D] Failed to load: " << path
                  << "  reason: " << stbi_failure_reason() << "\n";
        return tex;
    }

    GLenum internal_fmt = GL_RGB8;
    GLenum fmt          = GL_RGB;

    if (tex.channels == 1) {
        internal_fmt = GL_R8;
        fmt          = GL_RED;
    } else if (tex.channels == 2) {
        internal_fmt = GL_RG8;
        fmt          = GL_RG;
    } else if (tex.channels == 3) {
        internal_fmt = srgb ? GL_SRGB8         : GL_RGB8;
        fmt          = GL_RGB;
    } else if (tex.channels == 4) {
        internal_fmt = srgb ? GL_SRGB8_ALPHA8  : GL_RGBA8;
        fmt          = GL_RGBA;
    }

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);

    glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt,
                 tex.width, tex.height, 0,
                 fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Default wrap / filter
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Anisotropic filtering (OpenGL 4.6 core, or ARB_texture_filter_anisotropic)
    GLfloat max_aniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return tex;
}

Texture2D Texture2D::create_empty(int w, int h,
                                   GLenum internal_fmt,
                                   GLenum fmt,
                                   GLenum type) {
    Texture2D tex;
    tex.width    = w;
    tex.height   = h;
    tex.channels = 0;

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt, w, h, 0, fmt, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void Texture2D::bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture2D::destroy() {
    if (id) { glDeleteTextures(1, &id); id = 0; }
}
