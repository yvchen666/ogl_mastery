#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <chrono>
#include "yuv_renderer.h"
#include "test_frame_gen.h"

static int   g_width  = 1280;
static int   g_height = 720;
static bool  g_use_pbo = true;
static YuvColorSpace g_cs    = YuvColorSpace::BT601;
static YuvRange      g_range = YuvRange::Studio;

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}

static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_P) {
        g_use_pbo = !g_use_pbo;
        std::cout << "[YUV] PBO upload: " << (g_use_pbo ? "ON" : "OFF") << "\n";
    }
    if (key == GLFW_KEY_C) {
        g_cs = (g_cs == YuvColorSpace::BT601) ? YuvColorSpace::BT709
                                               : YuvColorSpace::BT601;
        std::cout << "[YUV] ColorSpace: "
                  << (g_cs == YuvColorSpace::BT601 ? "BT.601" : "BT.709") << "\n";
    }
    if (key == GLFW_KEY_R) {
        g_range = (g_range == YuvRange::Studio) ? YuvRange::Full
                                                 : YuvRange::Studio;
        std::cout << "[YUV] Range: "
                  << (g_range == YuvRange::Studio ? "Studio (16-235)" : "Full (0-255)") << "\n";
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(g_width, g_height,
        "Module 16 – YUV Renderer | P=PBO toggle | C=colorspace | R=range",
        nullptr, nullptr);
    if (!win) { std::cerr << "GLFW window failed\n"; return -1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_cb);
    glfwSetKeyCallback(win, key_cb);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD failed\n"; return -1;
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
    std::cout << "Keys: P=toggle PBO, C=toggle colorspace, R=toggle range\n";

    // ── 视频帧尺寸（测试帧）─────────────────────────────────────────
    const int VW = 1280, VH = 720;
    const int UV_W = VW / 2, UV_H = VH / 2;
    std::vector<uint8_t> y_buf(VW * VH);
    std::vector<uint8_t> u_buf(UV_W * UV_H);
    std::vector<uint8_t> v_buf(UV_W * UV_H);

    YuvRenderer renderer;
    if (!renderer.init(VW, VH)) return -1;

    double prev_time  = glfwGetTime();
    double fps_acc    = 0.0;
    int    fps_count  = 0;
    int    frame_idx  = 0;

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        double dt  = now - prev_time;
        prev_time  = now;
        fps_acc   += dt;
        fps_count++;

        glfwPollEvents();

        // ── 生成测试帧 ────────────────────────────────────────────────
        gen_yuv_frame(y_buf.data(), u_buf.data(), v_buf.data(),
                      VW, VH, frame_idx++);

        // ── 计时上传 ──────────────────────────────────────────────────
        auto t0 = std::chrono::high_resolution_clock::now();
        if (g_use_pbo)
            renderer.upload_frame_pbo(y_buf.data(), u_buf.data(), v_buf.data());
        else
            renderer.upload_frame(y_buf.data(), u_buf.data(), v_buf.data());
        auto t1 = std::chrono::high_resolution_clock::now();
        double upload_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        // ── 渲染 ──────────────────────────────────────────────────────
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.render(g_cs, g_range);

        // ── FPS 和上传延迟显示 ────────────────────────────────────────
        if (fps_acc >= 1.0) {
            char title[128];
            snprintf(title, sizeof(title),
                     "Module 16 – YUV | FPS: %d | Upload: %.1f us | PBO:%s | %s | %s",
                     fps_count,
                     upload_us,
                     g_use_pbo ? "ON" : "OFF",
                     g_cs == YuvColorSpace::BT601 ? "BT601" : "BT709",
                     g_range == YuvRange::Studio ? "Studio" : "Full");
            glfwSetWindowTitle(win, title);
            fps_count = 0; fps_acc = 0.0;
        }

        glfwSwapBuffers(win);
    }

    renderer.destroy();
    glfwTerminate();
    return 0;
}
