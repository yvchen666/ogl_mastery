#include "shadow_map.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

void ShadowMap::create(int w, int h) {
    width  = w;
    height = h;

    glGenFramebuffers(1, &fbo);

    glGenTextures(1, &depth_tex);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 width, height, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Clamp to border so regions outside shadow map are lit (not shadowed)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depth_tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("ShadowMap: FBO incomplete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::bind_for_write() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::bind_depth_tex(int slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
}

glm::mat4 ShadowMap::light_space_matrix(
    glm::vec3 light_dir,
    glm::vec3 scene_center,
    float scene_radius) const
{
    // Directional light: position light far back along its direction
    glm::vec3 light_pos = scene_center - glm::normalize(light_dir) * scene_radius * 2.0f;
    glm::mat4 view = glm::lookAt(light_pos, scene_center, glm::vec3(0.0f, 1.0f, 0.0f));
    // Orthographic projection covering the scene sphere
    float r = scene_radius * 1.2f;
    glm::mat4 proj = glm::ortho(-r, r, -r, r, 0.1f, scene_radius * 6.0f);
    return proj * view;
}

void ShadowMap::destroy() {
    if (depth_tex) { glDeleteTextures(1, &depth_tex); depth_tex = 0; }
    if (fbo)       { glDeleteFramebuffers(1, &fbo);   fbo = 0; }
}
