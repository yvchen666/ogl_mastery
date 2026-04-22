// module05_transforms/src/main.cpp
// ─────────────────────────────────────────────────────────────────────────────
// MVP 变换演示：多个立方体，每个拥有独立 Model 矩阵
// WASD 移动相机（固定朝向 -Z），ESC 退出
// 终端打印每帧 MVP 矩阵
// ─────────────────────────────────────────────────────────────────────────────

#include "shader.h"
#include "mesh.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <array>
#include <cmath>

// ─── 相机状态 ────────────────────────────────────────────────────────────────
static glm::vec3 g_cam_pos   {0.0f, 1.5f, 6.0f};
static glm::vec3 g_cam_front {0.0f, 0.0f,-1.0f};
static glm::vec3 g_cam_up    {0.0f, 1.0f, 0.0f};
static float     g_last_time = 0.0f;

// ─── 回调 ────────────────────────────────────────────────────────────────────
static void framebuffer_cb(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

// ─── 按键处理（在主循环中调用，delta_time 解耦帧率）─────────────────────────
static void process_input(GLFWwindow* win, float dt)
{
    const float speed = 4.0f * dt;
    glm::vec3 right = glm::normalize(glm::cross(g_cam_front, g_cam_up));

    if (glfwGetKey(win, GLFW_KEY_W)      == GLFW_PRESS) g_cam_pos += speed * g_cam_front;
    if (glfwGetKey(win, GLFW_KEY_S)      == GLFW_PRESS) g_cam_pos -= speed * g_cam_front;
    if (glfwGetKey(win, GLFW_KEY_A)      == GLFW_PRESS) g_cam_pos -= speed * right;
    if (glfwGetKey(win, GLFW_KEY_D)      == GLFW_PRESS) g_cam_pos += speed * right;
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// ─── 打印 4×4 矩阵到终端 ─────────────────────────────────────────────────────
static void print_mat4(const char* label, const glm::mat4& m)
{
    std::printf("%s:\n", label);
    for (int r = 0; r < 4; ++r)
        std::printf("  [%8.4f  %8.4f  %8.4f  %8.4f]\n",
                    m[0][r], m[1][r], m[2][r], m[3][r]);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    // ── GLFW ─────────────────────────────────────────────────────────────
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(900, 600,
        "Module05 – MVP Transforms  [WASD=Move  ESC=Quit]", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_cb);
    glfwSwapInterval(1);

    // ── GLAD ─────────────────────────────────────────────────────────────
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "gladLoadGLLoader failed\n");
        return 1;
    }

    glEnable(GL_DEPTH_TEST);

    std::printf("OpenGL %s\n", glGetString(GL_VERSION));
    std::printf("Controls: WASD=Move camera  ESC=Quit\n");
    std::printf("MVP matrices printed every 2 seconds.\n\n");

    // ── 资源 ─────────────────────────────────────────────────────────────
    Shader shader("shaders/mvp.vert", "shaders/mvp.frag");
    Mesh   cube  = Mesh::create_cube();

    // ── 场景：5 个立方体，各自有不同 TRS ─────────────────────────────────
    struct CubeInstance {
        glm::vec3 pos;
        glm::vec3 scale;
        glm::vec3 rot_axis;
        float     rot_speed;   // rad/s
        glm::vec3 color;
    };

    std::array<CubeInstance, 5> cubes = {{
        // 中央大立方体，慢速绕 Y 轴旋转
        {{ 0.0f,  0.0f,  0.0f}, {1.2f,1.2f,1.2f}, {0,1,0},  0.4f, {0.8f,0.4f,0.2f}},
        // 右侧小立方体，绕 (1,1,0) 轴旋转
        {{ 3.0f,  0.0f,  0.0f}, {0.7f,0.7f,0.7f}, {1,1,0},  0.8f, {0.2f,0.7f,0.9f}},
        // 左侧压扁（非均匀缩放，法向量变换可见效果）
        {{-3.0f,  0.0f,  0.0f}, {1.0f,0.3f,1.0f}, {0,0,1},  0.5f, {0.3f,0.9f,0.4f}},
        // 上方立方体
        {{ 0.0f,  2.5f,  0.0f}, {0.5f,0.5f,0.5f}, {1,0,1},  1.2f, {0.9f,0.9f,0.2f}},
        // 后方远处
        {{ 0.0f,  0.0f, -4.0f}, {1.5f,1.5f,1.5f}, {0,1,0}, -0.3f, {0.7f,0.3f,0.8f}},
    }};

    glm::vec3 light_pos  {3.0f, 4.0f, 2.0f};
    glm::vec3 light_color{1.0f, 1.0f, 1.0f};

    double last_print = -999.0;

    // ── 主循环 ────────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float dt  = now - g_last_time;
        g_last_time = now;

        glfwPollEvents();
        process_input(win, dt);

        int fb_w, fb_h;
        glfwGetFramebufferSize(win, &fb_w, &fb_h);
        float aspect = (fb_h > 0) ? (float)fb_w / fb_h : 1.0f;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ── View / Projection ─────────────────────────────────────────────
        glm::mat4 view = glm::lookAt(g_cam_pos,
                                     g_cam_pos + g_cam_front,
                                     g_cam_up);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        shader.use();
        shader.set_vec3("uLightPos",   light_pos);
        shader.set_vec3("uLightColor", light_color);
        shader.set_mat4("uView",       view);
        shader.set_mat4("uProjection", proj);

        bool do_print = (now - last_print) >= 2.0;
        if (do_print) {
            last_print = now;
            std::printf("=== t=%.1fs  cam_pos=(%.2f,%.2f,%.2f) ===\n",
                        now, g_cam_pos.x, g_cam_pos.y, g_cam_pos.z);
            print_mat4("View",       view);
            print_mat4("Projection", proj);
        }

        // ── 绘制每个立方体 ────────────────────────────────────────────────
        for (size_t i = 0; i < cubes.size(); ++i) {
            const auto& ci = cubes[i];

            // TRS 顺序：先缩放 → 再旋转 → 再平移（矩阵从右到左相乘）
            glm::mat4 model{1.0f};
            model = glm::translate(model, ci.pos);
            model = glm::rotate(model, now * ci.rot_speed, ci.rot_axis);
            model = glm::scale (model, ci.scale);

            // 法向量矩阵：(M⁻¹)ᵀ 取 3×3
            glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

            shader.set_mat4("uModel",        model);
            shader.set_mat3("uNormalMatrix", normalMat);
            shader.set_vec3("uObjectColor",  ci.color);

            if (do_print && i == 0) {
                print_mat4("Model[0]", model);
                glm::mat4 mvp = proj * view * model;
                print_mat4("MVP[0]",   mvp);
                std::printf("\n");
            }

            cube.draw();
        }

        glfwSwapBuffers(win);
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
