#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

// Shadow Map encapsulation (depth FBO)
class ShadowMap {
public:
    GLuint fbo{0};
    GLuint depth_tex{0};
    int width{1024}, height{1024};

    void create(int w = 1024, int h = 1024);
    void bind_for_write();
    void bind_depth_tex(int slot);
    glm::mat4 light_space_matrix(
        glm::vec3 light_dir,
        glm::vec3 scene_center,
        float scene_radius) const;
    void destroy();
};
