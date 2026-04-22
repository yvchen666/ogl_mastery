#pragma once
#include <glad/glad.h>

// G-Buffer: position / normal / albedo+specular (MRT)
class GBuffer {
public:
    GLuint fbo{0};
    GLuint pos_tex{0};         // GL_RGB16F : world-space position
    GLuint normal_tex{0};      // GL_RGB16F : world-space normal
    GLuint albedo_spec_tex{0}; // GL_RGBA8  : RGB=diffuse, A=specular intensity
    GLuint depth_rbo{0};

    bool create(int w, int h);
    void bind_geometry_pass();
    void bind_textures();   // binds to texture units 0/1/2
    void destroy();
};
