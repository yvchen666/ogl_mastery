// module03_glsl_deep — GLSL 特性演示程序
//
// 演示内容：
//   - UBO（Uniform Buffer Object）以 std140 布局传递帧数据
//   - gl_FragCoord 实现棋盘格效果
//   - 基于时间（uTime）的颜色动画（sin/cos）
//   - smoothstep 实现边缘渐变
//
// 依赖：ogl_common（glad, glfw, glm）+ ubo_helper

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>

#include "ubo_helper.h"

// ── 窗口尺寸 ─────────────────────────────────────────────────────────────────
static constexpr int WIDTH  = 800;
static constexpr int HEIGHT = 600;

// ── UBO 数据结构（std140 布局）───────────────────────────────────────────────
// 必须与着色器中的 layout(std140, binding=0) uniform FrameData 完全对应
//
// std140 对齐规则（详见 README 第5节）：
//   float  → 4 字节，对齐到 4 字节
//   float  → 4 字节，对齐到 4 字节（紧接上面的 float）
//   vec2   → 8 字节，对齐到 8 字节
// 总大小：4 + 4 + 8 = 16 字节（满足 16 字节末尾对齐）
//
// 注意：如果换成 vec3，对齐到 16 字节，总大小会变成 32 字节！（见 README 坑1）
struct FrameData {
    float uTime;      // 偏移 0，大小 4
    float uAspect;    // 偏移 4，大小 4
    float uResX;      // 偏移 8，大小 4（vec2 拆开写避免 vec3 对齐陷阱）
    float uResY;      // 偏移 12，大小 4
};  // 总大小 16 字节

// ── 调试回调（同 module02）──────────────────────────────────────────────────
void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity,
    GLsizei, const GLchar* msg, const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::cerr << "[GL] " << msg << "\n";
}

// ── 着色器工具函数 ────────────────────────────────────────────────────────────
static std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Cannot open: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_shader(GLenum type, const char* path) {
    std::string src = read_file(path);
    const char* c = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "Shader compile error (" << path << "):\n" << log << "\n";
    }
    return s;
}

static GLuint link_program(GLuint vert, GLuint frag) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << "Program link error:\n" << log << "\n";
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return p;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main() {
    // ── GLFW 初始化 ──
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "GLSL Deep", nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // ── GLAD 加载 ──
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n";
        return -1;
    }

    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "Version:  " << glGetString(GL_VERSION)  << "\n";

    // ── 调试回调 ──
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_callback, nullptr);

    // ────────────────────────────────────────────────────────────────────────
    // 矩形顶点数据：两个三角形组成一个全屏（NDC）矩形
    // 每个顶点：位置(xy) + UV 坐标
    //
    // NDC 矩形覆盖 [-1,1] x [-1,1]，UV 为 [0,1] x [0,1]
    // 使用 EBO（索引缓冲）避免重复顶点
    // ────────────────────────────────────────────────────────────────────────

    // 矩形四个顶点：左下、右下、右上、左上
    //   位置 x    y      UV u    v
    float vertices[] = {
        -1.0f, -1.0f,    0.0f, 0.0f,   // 0: 左下
         1.0f, -1.0f,    1.0f, 0.0f,   // 1: 右下
         1.0f,  1.0f,    1.0f, 1.0f,   // 2: 右上
        -1.0f,  1.0f,    0.0f, 1.0f,   // 3: 左上
    };

    // 两个三角形（逆时针方向，OpenGL 默认正面）
    unsigned int indices[] = {
        0, 1, 2,   // 三角形 1：左下→右下→右上
        0, 2, 3,   // 三角形 2：左下→右上→左上
    };

    // ── 创建 VAO / VBO / EBO ──
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO 绑定到 VAO 绑定期间会被 VAO 记录（不同于 VBO！）
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 顶点属性：每个顶点 4 个 float（位置xy + UVxy），stride = 16 字节
    // location=0: 位置（2个float，偏移0）
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // location=1: UV（2个float，偏移8字节）
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);  // 解绑 VAO

    // ── 编译着色器 ──
    GLuint program = link_program(
        compile_shader(GL_VERTEX_SHADER,   "shaders/demo.vert"),
        compile_shader(GL_FRAGMENT_SHADER, "shaders/demo.frag")
    );

    // ── 创建并初始化 UBO ──
    UboHelper ubo;
    ubo.create(sizeof(FrameData));
    // 绑定到 binding point 0（对应着色器中 layout(std140, binding=0)）
    ubo.bind_to_point(0);

    // ── 渲染循环 ──
    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // 更新 UBO 数据（每帧）
        // glfwGetTime() 返回程序启动以来的秒数（double）
        FrameData frame_data;
        frame_data.uTime   = static_cast<float>(glfwGetTime());
        frame_data.uAspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        frame_data.uResX   = static_cast<float>(WIDTH);
        frame_data.uResY   = static_cast<float>(HEIGHT);
        ubo.update(&frame_data, 0, sizeof(FrameData));

        // 清屏
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 绘制矩形（6 个索引 = 2 个三角形）
        glUseProgram(program);
        glBindVertexArray(vao);
        // glDrawElements 使用 EBO 中的索引
        // 参数：图元类型, 索引数量, 索引类型, 索引数组偏移（EBO 已绑定在 VAO 中，nullptr 表示从头开始）
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ── 清理 ──
    ubo.destroy();
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
