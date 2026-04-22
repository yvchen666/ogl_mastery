#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "particle_system.h"

// ─── 窗口/摄像机状态 ──────────────────────────────────────
static int  g_width  = 1280;
static int  g_height = 720;

static glm::vec3 g_cam_pos(0.0f, 5.0f, 20.0f);
static glm::vec3 g_cam_front(0.0f, 0.0f, -1.0f);
static bool  g_first_mouse = true;
static float g_last_x = 640.0f, g_last_y = 360.0f;
static float g_yaw = -90.0f, g_pitch = 0.0f;

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}

static void mouse_cb(GLFWwindow* win, double xpos, double ypos) {
    if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) return;
    if (g_first_mouse) { g_last_x = (float)xpos; g_last_y = (float)ypos; g_first_mouse = false; }
    float dx = (float)xpos - g_last_x;
    float dy = g_last_y - (float)ypos;
    g_last_x = (float)xpos; g_last_y = (float)ypos;
    float sens = 0.1f;
    g_yaw   += dx * sens;
    g_pitch += dy * sens;
    g_pitch = glm::clamp(g_pitch, -89.0f, 89.0f);
    glm::vec3 front;
    front.x = cosf(glm::radians(g_yaw)) * cosf(glm::radians(g_pitch));
    front.y = sinf(glm::radians(g_pitch));
    front.z = sinf(glm::radians(g_yaw)) * cosf(glm::radians(g_pitch));
    g_cam_front = glm::normalize(front);
}

static void process_input(GLFWwindow* win, float dt) {
    float speed = 10.0f * dt;
    glm::vec3 right = glm::normalize(glm::cross(g_cam_front, {0,1,0}));
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) g_cam_pos += g_cam_front * speed;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) g_cam_pos -= g_cam_front * speed;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) g_cam_pos -= right * speed;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) g_cam_pos += right * speed;
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}

int main() {
    // ── GLFW 初始化 ──────────────────────────────────────
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(g_width, g_height,
                                        "Module 15 – GPU Particle System", nullptr, nullptr);
    if (!win) { std::cerr << "Failed to create window\n"; return -1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_cb);
    glfwSetCursorPosCallback(win, mouse_cb);

    // ── GLAD 加载 ────────────────────────────────────────
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n"; return -1;
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    glViewport(0, 0, g_width, g_height);
    glEnable(GL_DEPTH_TEST);

    // ── 粒子系统 ─────────────────────────────────────────
    ParticleSystem ps;
    ps.init(100000);

    double prev = glfwGetTime();
    double fps_time = prev;
    int    fps_count = 0;

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float  dt  = (float)(now - prev);
        prev = now;

        // FPS 显示
        fps_count++;
        if (now - fps_time >= 1.0) {
            char title[64];
            snprintf(title, sizeof(title), "Module 15 – GPU Particles | FPS: %d", fps_count);
            glfwSetWindowTitle(win, title);
            fps_count = 0; fps_time = now;
        }

        glfwPollEvents();
        process_input(win, dt);

        // ── 更新粒子（Compute Shader）────────────────────
        ps.update(dt);

        // ── 渲染 ─────────────────────────────────────────
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(g_cam_pos,
                                      g_cam_pos + g_cam_front,
                                      {0.0f, 1.0f, 0.0f});
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                           (float)g_width / (float)g_height,
                                           0.1f, 500.0f);
        ps.render(view, proj, g_cam_pos);

        glfwSwapBuffers(win);
    }

    ps.destroy();
    glfwTerminate();
    return 0;
}
