#include "math_vis.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// 内部状态
// ─────────────────────────────────────────────────────────────────────────────

static GLuint s_prog  = 0;
static GLuint s_vao   = 0;
static GLuint s_vbo   = 0;
static glm::mat4 s_mvp{1.0f};

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数
// ─────────────────────────────────────────────────────────────────────────────

static GLuint compile_shader(GLenum type, const char* path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error(std::string("Cannot open shader: ") + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();
    const char* c   = src.c_str();

    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("Shader compile error (") + path + "): " + log);
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// 公共 API
// ─────────────────────────────────────────────────────────────────────────────

void math_vis_init()
{
    // ── 着色器程序 ────────────────────────────────────────────────────────
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   "shaders/simple.vert");
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.frag");

    s_prog = glCreateProgram();
    glAttachShader(s_prog, vs);
    glAttachShader(s_prog, fs);
    glLinkProgram(s_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(s_prog, 512, nullptr, log);
        throw std::runtime_error(std::string("Program link error: ") + log);
    }

    // ── 动态 VAO/VBO（每帧上传顶点）──────────────────────────────────────
    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    // 预分配 1 MB
    glBufferData(GL_ARRAY_BUFFER, 1024 * 1024, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void math_vis_shutdown()
{
    glDeleteBuffers(1, &s_vbo);
    glDeleteVertexArrays(1, &s_vao);
    glDeleteProgram(s_prog);
    s_vbo = s_vao = s_prog = 0;
}

void math_vis_set_mvp(const glm::mat4& mvp)
{
    s_mvp = mvp;
}

// ── 内部：上传顶点并绘制基元 ────────────────────────────────────────────────
static void draw_lines(const std::vector<glm::vec3>& pts, glm::vec3 color, GLenum mode)
{
    if (pts.empty()) return;

    glUseProgram(s_prog);
    glUniformMatrix4fv(glGetUniformLocation(s_prog, "uMVP"),   1, GL_FALSE, glm::value_ptr(s_mvp));
    glUniform3fv      (glGetUniformLocation(s_prog, "uColor"), 1, glm::value_ptr(color));

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(pts.size() * sizeof(glm::vec3)),
                    pts.data());

    glLineWidth(2.0f);
    glDrawArrays(mode, 0, static_cast<GLsizei>(pts.size()));
    glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_arrow_2d
//   在 2D（z=0）平面绘制带箭头的向量线段
//   start/end 为 NDC 坐标 [-1,1]^2
// ─────────────────────────────────────────────────────────────────────────────
void draw_arrow_2d(glm::vec2 start, glm::vec2 end, glm::vec3 color)
{
    std::vector<glm::vec3> pts;

    // 主干
    pts.push_back({start.x, start.y, 0.f});
    pts.push_back({end.x,   end.y,   0.f});

    // 箭头两翼（在方向上反转 0.08 长度，偏移 ±25°）
    glm::vec2 dir = glm::normalize(end - start);
    float     hl  = 0.08f;
    float     ang = glm::radians(25.0f);

    auto rotate2 = [](glm::vec2 v, float a) -> glm::vec2 {
        return { v.x * std::cos(a) - v.y * std::sin(a),
                 v.x * std::sin(a) + v.y * std::cos(a) };
    };

    glm::vec2 w1 = end + hl * rotate2(-dir,  ang);
    glm::vec2 w2 = end + hl * rotate2(-dir, -ang);

    pts.push_back({end.x, end.y, 0.f});
    pts.push_back({w1.x,  w1.y,  0.f});
    pts.push_back({end.x, end.y, 0.f});
    pts.push_back({w2.x,  w2.y,  0.f});

    draw_lines(pts, color, GL_LINES);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_axes_3d
//   绘制 3D 坐标轴；颜色由 RGB 分别对应 XYZ
// ─────────────────────────────────────────────────────────────────────────────
void draw_axes_3d(float length)
{
    // X 轴 — 红
    draw_lines({{ 0,0,0 }, { length,0,0 }}, {1,0,0}, GL_LINES);
    // Y 轴 — 绿
    draw_lines({{ 0,0,0 }, { 0,length,0 }}, {0,1,0}, GL_LINES);
    // Z 轴 — 蓝
    draw_lines({{ 0,0,0 }, { 0,0,length }}, {0,0,1}, GL_LINES);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_transformed_grid
//   在 [-1,1]^2 范围内生成网格，经过 M 变换后绘制
// ─────────────────────────────────────────────────────────────────────────────
void draw_transformed_grid(const glm::mat4& M, glm::vec3 color)
{
    const int   N    = 5;      // 每侧格数
    const float step = 2.0f / N;

    std::vector<glm::vec3> pts;

    // 水平线
    for (int i = 0; i <= N; ++i) {
        float y = -1.0f + i * step;
        for (int j = 0; j < N; ++j) {
            float x0 = -1.0f + j * step;
            float x1 = x0 + step;

            glm::vec4 p0 = M * glm::vec4(x0, y, 0, 1);
            glm::vec4 p1 = M * glm::vec4(x1, y, 0, 1);
            pts.push_back({p0.x/p0.w, p0.y/p0.w, p0.z/p0.w});
            pts.push_back({p1.x/p1.w, p1.y/p1.w, p1.z/p1.w});
        }
    }
    // 垂直线
    for (int j = 0; j <= N; ++j) {
        float x = -1.0f + j * step;
        for (int i = 0; i < N; ++i) {
            float y0 = -1.0f + i * step;
            float y1 = y0 + step;

            glm::vec4 p0 = M * glm::vec4(x, y0, 0, 1);
            glm::vec4 p1 = M * glm::vec4(x, y1, 0, 1);
            pts.push_back({p0.x/p0.w, p0.y/p0.w, p0.z/p0.w});
            pts.push_back({p1.x/p1.w, p1.y/p1.w, p1.z/p1.w});
        }
    }

    draw_lines(pts, color, GL_LINES);
}
