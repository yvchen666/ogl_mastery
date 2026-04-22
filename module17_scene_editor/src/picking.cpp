#include "picking.h"
#include "components.h"
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// ─── Shader helpers ──────────────────────────────────────────────────────────
static std::string pick_read_file(const std::string& p) {
    std::ifstream f(p);
    if (!f) { std::cerr << "Cannot open: " << p << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
static GLuint pick_compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetShaderInfoLog(s,1024,nullptr,buf);
               std::cerr << "Pick shader:\n" << buf << "\n"; }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Picking::init
// ─────────────────────────────────────────────────────────────────────────────
void Picking::init(int w, int h) {
    width_ = w; height_ = h;

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // R32UI 颜色缓冲：每个像素存一个 uint32（EntityId）
    glGenTextures(1, &color_tex_);
    glBindTexture(GL_TEXTURE_2D, color_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, w, h, 0,
                 GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, color_tex_, 0);

    // 深度 Renderbuffer
    glGenRenderbuffers(1, &depth_rb_);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_rb_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Picking FBO incomplete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    build_shader();
}

void Picking::resize(int w, int h) {
    destroy();
    init(w, h);
}

bool Picking::build_shader() {
    auto vsrc = pick_read_file("shaders/picking.vert");
    auto fsrc = pick_read_file("shaders/picking.frag");
    GLuint vs = pick_compile(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = pick_compile(GL_FRAGMENT_SHADER, fsrc);
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs); glAttachShader(prog_, fs);
    glLinkProgram(prog_);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok; glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetProgramInfoLog(prog_,1024,nullptr,buf);
               std::cerr << "Pick prog:\n" << buf << "\n"; return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// render_ids：将每个物体的 EntityId 渲染到 FBO 颜色缓冲
// ─────────────────────────────────────────────────────────────────────────────
void Picking::render_ids(World& world, const glm::mat4& vp) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);

    // 清除颜色为 0（INVALID_ENTITY）
    GLuint clear_val = 0;
    glClearBufferuiv(GL_COLOR, 0, &clear_val);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog_);
    GLint loc_mvp = glGetUniformLocation(prog_, "uMVP");
    GLint loc_id  = glGetUniformLocation(prog_, "uEntityId");

    world.each<MeshComponent>([&](EntityId eid, MeshComponent& mc) {
        if (mc.vao == 0 || mc.index_count == 0) return;
        auto* tc = world.get_component<TransformComponent>(eid);
        glm::mat4 model = tc ? tc->matrix() : glm::mat4(1.0f);
        glm::mat4 mvp   = vp * model;
        glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1ui(loc_id, (GLuint)eid);

        glBindVertexArray(mc.vao);
        glDrawElements(GL_TRIANGLES, mc.index_count, GL_UNSIGNED_INT, nullptr);
    });

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// read_entity：读取像素坐标处的 EntityId
// ─────────────────────────────────────────────────────────────────────────────
EntityId Picking::read_entity(int x, int y) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // OpenGL 坐标系 Y 轴翻转（窗口左下为原点）
    int flipped_y = height_ - 1 - y;

    GLuint id = 0;
    glReadPixels(x, flipped_y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return (EntityId)id;
}

void Picking::destroy() {
    glDeleteTextures(1, &color_tex_);
    glDeleteRenderbuffers(1, &depth_rb_);
    glDeleteFramebuffers(1, &fbo_);
    glDeleteProgram(prog_);
    fbo_ = color_tex_ = depth_rb_ = prog_ = 0;
}
