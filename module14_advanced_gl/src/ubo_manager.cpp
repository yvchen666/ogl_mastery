#include "ubo_manager.h"
#include <cstring>

void UboManager::init() {
    // Camera UBO (binding = 0)
    glGenBuffers(1, &camera_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, camera_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, camera_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Lights UBO (binding = 1)
    glGenBuffers(1, &lights_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, lights_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, lights_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UboManager::update_camera(const glm::mat4& view, const glm::mat4& proj, glm::vec3 cam_pos) {
    CameraBlock block;
    block.view       = view;
    block.projection = proj;
    block.cam_pos    = cam_pos;
    block._pad       = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, camera_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraBlock), &block);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UboManager::update_lights(const glm::vec4* positions, const glm::vec4* colors, int count) {
    LightBlock block;
    block.count = count;
    for (int i = 0; i < count && i < 4; ++i) {
        block.positions[i] = positions[i];
        block.colors[i]    = colors[i];
    }

    glBindBuffer(GL_UNIFORM_BUFFER, lights_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightBlock), &block);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UboManager::destroy() {
    if (camera_ubo) { glDeleteBuffers(1, &camera_ubo); camera_ubo = 0; }
    if (lights_ubo) { glDeleteBuffers(1, &lights_ubo); lights_ubo = 0; }
}
