#include "particle_system.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

// ─────────────────────────────────────────────────────────
// Shader helpers
// ─────────────────────────────────────────────────────────
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static GLuint compile_shader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(s, 1024, nullptr, buf);
        std::cerr << "Shader compile error:\n" << buf << "\n";
    }
    return s;
}

static GLuint link_program(std::initializer_list<GLuint> shaders) {
    GLuint p = glCreateProgram();
    for (auto s : shaders) glAttachShader(p, s);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(p, 1024, nullptr, buf);
        std::cerr << "Program link error:\n" << buf << "\n";
    }
    for (auto s : shaders) glDeleteShader(s);
    return p;
}

// ─────────────────────────────────────────────────────────
// ParticleSystem::init
// ─────────────────────────────────────────────────────────
void ParticleSystem::init(int n) {
    max_particles = n;

    // ── SSBO ──
    std::vector<Particle> init_data(n);
    for (int i = 0; i < n; ++i) {
        float age  = static_cast<float>(rand()) / RAND_MAX * 8.0f;  // 随机初始年龄
        float life = 3.0f + static_cast<float>(rand()) / RAND_MAX * 5.0f;
        float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
        float speed = 2.0f + static_cast<float>(rand()) / RAND_MAX * 3.0f;
        init_data[i].position = {0.0f, 0.0f, 0.0f, age};
        init_data[i].velocity = {cosf(angle)*speed,
                                  5.0f + static_cast<float>(rand())/RAND_MAX*5.0f,
                                  sinf(angle)*speed,
                                  life};
        init_data[i].color = {static_cast<float>(rand())/RAND_MAX,
                               static_cast<float>(rand())/RAND_MAX,
                               static_cast<float>(rand())/RAND_MAX, 1.0f};
    }
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 n * sizeof(Particle),
                 init_data.data(),
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    // ── 空 VAO（billboard 由 vs 算出）──
    glGenVertexArrays(1, &vao);

    // ── Compute Shader ──
    {
        auto src = read_file("shaders/particle_update.comp");
        auto cs  = compile_shader(GL_COMPUTE_SHADER, src);
        update_prog_ = link_program({cs});
    }

    // ── 渲染程序 ──
    {
        auto vsrc = read_file("shaders/particle.vert");
        auto fsrc = read_file("shaders/particle.frag");
        auto vs = compile_shader(GL_VERTEX_SHADER,   vsrc);
        auto fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
        render_prog_ = link_program({vs, fs});
    }
}

// ─────────────────────────────────────────────────────────
// ParticleSystem::update
// ─────────────────────────────────────────────────────────
void ParticleSystem::update(float dt) {
    time_ += dt;
    glUseProgram(update_prog_);
    glUniform1f(glGetUniformLocation(update_prog_, "uDeltaTime"), dt);
    glUniform1f(glGetUniformLocation(update_prog_, "uTime"), time_);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    // local_size_x=256
    int groups = (max_particles + 255) / 256;
    glDispatchCompute(groups, 1, 1);

    // 确保 SSBO 写完后再渲染读取
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// ─────────────────────────────────────────────────────────
// ParticleSystem::render
// ─────────────────────────────────────────────────────────
void ParticleSystem::render(const glm::mat4& view, const glm::mat4& proj,
                             glm::vec3 camera_pos) {
    glUseProgram(render_prog_);

    // 传矩阵
    glUniformMatrix4fv(glGetUniformLocation(render_prog_, "uView"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(render_prog_, "uProj"), 1, GL_FALSE,
                       glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(render_prog_, "uCameraPos"), 1,
                 glm::value_ptr(camera_pos));
    glUniform1i(glGetUniformLocation(render_prog_, "uParticleCount"), max_particles);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    // 加法混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);  // 不写深度（粒子互相不遮挡）

    glBindVertexArray(vao);
    // 每粒子 4 个顶点（billboard 四边形），instanced → 用 gl_VertexID
    glDrawArrays(GL_TRIANGLE_STRIP, 0, max_particles * 4);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// ─────────────────────────────────────────────────────────
// ParticleSystem::destroy
// ─────────────────────────────────────────────────────────
void ParticleSystem::destroy() {
    glDeleteBuffers(1, &ssbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(update_prog_);
    glDeleteProgram(render_prog_);
    ssbo = vao = update_prog_ = render_prog_ = 0;
}
