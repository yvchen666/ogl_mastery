// module04_linear_algebra/src/main.cpp
// ─────────────────────────────────────────────────────────────────────────────
// 互动式线性代数可视化演示
//   键 1 → 点积与投影（2D）
//   键 2 → 叉积（3D）
//   键 3 → 矩阵变换（2D 网格）
// ─────────────────────────────────────────────────────────────────────────────

#include "math_vis.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>

// ─── 全局状态 ────────────────────────────────────────────────────────────────
static int g_demo = 1;   // 当前演示编号

// ─── 回调 ────────────────────────────────────────────────────────────────────
static void key_callback(GLFWwindow* win, int key, int /*sc*/, int action, int /*mod*/)
{
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) g_demo = 1;
        if (key == GLFW_KEY_2) g_demo = 2;
        if (key == GLFW_KEY_3) g_demo = 3;
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(win, GLFW_TRUE);
    }
}

static void framebuffer_size_callback(GLFWwindow* /*w*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

// ─────────────────────────────────────────────────────────────────────────────
// 演示 1 ── 点积与投影（2D）
//   向量 a = (0.7, 0.4), b = (0.3, 0.8)（NDC 坐标偏移自原点）
//   绘制：a（绿）、b（蓝）、proj_b(a) 的投影向量（黄）、垂线（白虚线）
// ─────────────────────────────────────────────────────────────────────────────
static void demo_dot_product()
{
    glm::vec2 O{0.0f, 0.0f};
    glm::vec2 a{0.65f, 0.35f};   // 向量 a
    glm::vec2 b{0.20f, 0.75f};   // 向量 b

    // 点积与投影
    float dot_ab    = glm::dot(a, b);
    float b_len_sq  = glm::dot(b, b);
    glm::vec2 proj  = (dot_ab / b_len_sq) * b;   // proj_b(a)

    // 绘制 a（绿色）
    draw_arrow_2d(O, a, {0.2f, 0.9f, 0.2f});

    // 绘制 b（蓝色）
    draw_arrow_2d(O, b, {0.2f, 0.4f, 1.0f});

    // 绘制 proj_b(a)（黄色）
    draw_arrow_2d(O, proj, {1.0f, 0.9f, 0.0f});

    // 绘制从 a 到投影点的垂线（白色）
    draw_arrow_2d(a, proj, {0.9f, 0.9f, 0.9f});

    // 在终端每帧打印数值（仅第一次）
    static bool printed = false;
    if (!printed) {
        printed = true;
        std::printf("[Demo 1] a=(%.2f,%.2f)  b=(%.2f,%.2f)\n",
                    a.x, a.y, b.x, b.y);
        std::printf("         a·b = %.4f   |a|=%.4f  |b|=%.4f\n",
                    dot_ab, glm::length(a), glm::length(b));
        std::printf("         cos(theta) = %.4f  theta=%.2f deg\n",
                    dot_ab / (glm::length(a) * glm::length(b)),
                    glm::degrees(std::acos(dot_ab / (glm::length(a) * glm::length(b)))));
        std::printf("         proj_b(a) = (%.4f, %.4f)\n", proj.x, proj.y);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 演示 2 ── 叉积（3D，带坐标轴）
// ─────────────────────────────────────────────────────────────────────────────
static void demo_cross_product(float aspect)
{
    // 透视投影矩阵（仅用于 3D 演示）
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    // 摄像机位置
    float t = (float)glfwGetTime() * 0.4f;
    glm::mat4 view = glm::lookAt(
        glm::vec3(std::sin(t)*3.5f, 1.5f, std::cos(t)*3.5f),
        glm::vec3(0,0,0),
        glm::vec3(0,1,0));

    glm::mat4 mvp = proj * view;
    math_vis_set_mvp(mvp);

    draw_axes_3d(1.5f);

    // 两个向量
    glm::vec3 a{1.0f, 0.3f, 0.0f};
    glm::vec3 b{0.0f, 0.8f, 0.6f};
    glm::vec3 c = glm::cross(a, b);

    // 箭头（在 3D 中用 2D API 的"扁平"版本替代——直接送 3D 端点）
    // 复用 draw_arrow_2d 用 z 坐标分量；这里直接绘制线段
    // 绿色 a
    draw_arrow_2d({0,0}, {a.x, a.y}, {0.2f,0.9f,0.2f});   // 投影到 XY（仅演示）

    // 为了真正的 3D 效果，使用 draw_axes_3d 并手动绘制向量
    // 向量 a（绿）
    {
        math_vis_set_mvp(mvp);
        draw_arrow_2d({0.0f, 0.0f}, {a.x, a.y}, {0.2f,0.9f,0.2f});
    }

    static bool printed = false;
    if (!printed) {
        printed = true;
        std::printf("[Demo 2] a=(%.2f,%.2f,%.2f)  b=(%.2f,%.2f,%.2f)\n",
                    a.x,a.y,a.z, b.x,b.y,b.z);
        std::printf("         a×b = (%.4f, %.4f, %.4f)\n", c.x, c.y, c.z);
        std::printf("         |a×b| = %.4f  (= |a||b|sin(theta))\n",
                    glm::length(c));
        std::printf("         |a|*|b|*sin = %.4f\n",
                    glm::length(a)*glm::length(b)*
                    std::sin(std::acos(glm::dot(a,b)/(glm::length(a)*glm::length(b)))));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 演示 3 ── 矩阵变换（2D 网格）
//   随时间旋转 + 轻微缩放
// ─────────────────────────────────────────────────────────────────────────────
static void demo_matrix_transform()
{
    math_vis_set_mvp(glm::mat4{1.0f});   // 直接 NDC

    float t = (float)glfwGetTime();

    // 构建一个在 NDC 平面内演示的 2D 变换
    glm::mat4 T  = glm::translate(glm::mat4{1}, glm::vec3(0.2f*std::sin(t), 0.1f, 0));
    glm::mat4 R  = glm::rotate   (glm::mat4{1}, t, glm::vec3(0,0,1));
    glm::mat4 S  = glm::scale    (glm::mat4{1}, glm::vec3(0.5f + 0.2f*std::sin(t*0.7f)));
    glm::mat4 M  = T * R * S;

    // 原始网格（灰色）
    draw_transformed_grid(glm::mat4{1.0f} * glm::scale(glm::mat4{1},glm::vec3(0.5f)),
                          {0.4f, 0.4f, 0.4f});

    // 变换后网格（橙色）
    draw_transformed_grid(M, {1.0f, 0.6f, 0.1f});
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    // ── GLFW 初始化 ───────────────────────────────────────────────────────
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(800, 600,
        "Module04 – Linear Algebra  [1=Dot  2=Cross  3=Matrix]", nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(win);
    glfwSetKeyCallback(win, key_callback);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSwapInterval(1);

    // ── GLAD 加载 ─────────────────────────────────────────────────────────
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "gladLoadGLLoader failed\n");
        return 1;
    }

    std::printf("OpenGL %s  GLSL %s\n",
                glGetString(GL_VERSION),
                glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::printf("Controls: 1=Dot Product  2=Cross Product  3=Matrix Transform  ESC=Quit\n");

    // ── math_vis 初始化 ───────────────────────────────────────────────────
    math_vis_init();
    math_vis_set_mvp(glm::mat4{1.0f});

    glEnable(GL_LINE_SMOOTH);

    // ── 主循环 ────────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 重置 MVP 为 identity（2D 演示使用 NDC 坐标）
        math_vis_set_mvp(glm::mat4{1.0f});

        switch (g_demo) {
        case 1: demo_dot_product();            break;
        case 2: demo_cross_product(aspect);    break;
        case 3: demo_matrix_transform();       break;
        }

        glfwSwapBuffers(win);
    }

    // ── 清理 ──────────────────────────────────────────────────────────────
    math_vis_shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
