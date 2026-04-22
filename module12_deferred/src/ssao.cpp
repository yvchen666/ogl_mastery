#include "ssao.h"
#include <random>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

void SSAO::init(int w, int h) {
    generate_kernel(64);
    generate_noise_tex();

    // ── SSAO output FBO (single-channel R16F) ─────────────────────────────────
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ── Blur output FBO ───────────────────────────────────────────────────────
    glGenFramebuffers(1, &blur_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo);
    glGenTextures(1, &blur_tex);
    glBindTexture(GL_TEXTURE_2D, blur_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::generate_kernel(int n) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_signed(-1.0f, 1.0f);

    kernel.resize(n);
    for (int i = 0; i < n; ++i) {
        glm::vec3 sample(
            dist_signed(rng),
            dist_signed(rng),
            dist(rng)  // positive Z (hemisphere)
        );
        sample = glm::normalize(sample);
        sample *= dist(rng);

        // Accelerating interpolation — concentrate samples near origin
        float scale = (float)i / (float)n;
        scale = 0.1f + 0.9f * scale * scale; // lerp(0.1, 1.0, i/n ^2)
        kernel[i] = sample * scale;
    }
}

void SSAO::generate_noise_tex() {
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // 16 random rotation vectors (z=0: rotate in XY plane)
    glm::vec3 noise[16];
    for (int i = 0; i < 16; ++i) {
        noise[i] = glm::vec3(dist(rng), dist(rng), 0.0f);
    }

    glGenTextures(1, &noise_tex);
    glBindTexture(GL_TEXTURE_2D, noise_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, glm::value_ptr(noise[0]));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Tile the 4x4 noise across the screen
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SSAO::destroy() {
    if (tex)       { glDeleteTextures(1, &tex);      tex = 0; }
    if (blur_tex)  { glDeleteTextures(1, &blur_tex); blur_tex = 0; }
    if (noise_tex) { glDeleteTextures(1, &noise_tex);noise_tex = 0; }
    if (fbo)       { glDeleteFramebuffers(1, &fbo);  fbo = 0; }
    if (blur_fbo)  { glDeleteFramebuffers(1, &blur_fbo); blur_fbo = 0; }
    kernel.clear();
}
