#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// SSAO data: kernel, noise texture, and FBO
class SSAO {
public:
    GLuint fbo{0},      tex{0};       // SSAO output (single-channel)
    GLuint blur_fbo{0}, blur_tex{0};  // blurred SSAO
    GLuint noise_tex{0};              // 4x4 random rotation vectors

    std::vector<glm::vec3> kernel;    // hemisphere samples (64)

    void init(int w, int h);
    void generate_kernel(int n = 64);
    void generate_noise_tex();
    void destroy();
};
