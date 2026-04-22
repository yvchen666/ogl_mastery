#include "yuv_renderer.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

// ─── Shader helpers ──────────────────────────────────────────────────────────
static std::string yuv_read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open shader: " << path << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static GLuint yuv_compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(s, 1024, nullptr, buf);
        std::cerr << "YUV shader error:\n" << buf << "\n";
    }
    return s;
}

// ─── 全屏四边形顶点数据（NDC，UV 翻转以匹配图像坐标）──────────────────────
static const float QUAD_VERTS[] = {
    // x      y     u     v
    -1.0f,  1.0f, 0.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
};

// ─────────────────────────────────────────────────────────────────────────────
// YuvRenderer::init
// ─────────────────────────────────────────────────────────────────────────────
bool YuvRenderer::init(int width, int height) {
    width_  = width;
    height_ = height;

    const int uv_w = width  / 2;
    const int uv_h = height / 2;

    // ── 三张 GL_R8 纹理（单通道，8 位无符号）──────────────────────────
    auto make_tex = [](int w, int h) -> GLuint {
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0,
                     GL_RED, GL_UNSIGNED_BYTE, nullptr);
        return t;
    };
    tex_y_ = make_tex(width,  height);
    tex_u_ = make_tex(uv_w,   uv_h);
    tex_v_ = make_tex(uv_w,   uv_h);

    // ── 双 PBO（用于 Y 平面；UV 类似）────────────────────────────────
    // 简化：每个 PBO 存 Y 数据（最大平面），UV 共用两个 PBO 中的剩余空间
    // 此处每 PBO 分配 Y + U + V 总大小
    size_t y_sz  = (size_t)width  * height;
    size_t uv_sz = (size_t)uv_w   * uv_h;
    size_t total = y_sz + uv_sz * 2;

    glGenBuffers(2, pbos_);
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)total,
                     nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // ── 全屏四边形 VAO/VBO ───────────────────────────────────────────
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    return build_shader();
}

// ─────────────────────────────────────────────────────────────────────────────
// build_shader
// ─────────────────────────────────────────────────────────────────────────────
bool YuvRenderer::build_shader() {
    auto vsrc = yuv_read_file("shaders/yuv.vert");
    auto fsrc = yuv_read_file("shaders/yuv.frag");
    GLuint vs = yuv_compile(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = yuv_compile(GL_FRAGMENT_SHADER, fsrc);
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);
    glLinkProgram(prog_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok; glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(prog_, 1024, nullptr, buf);
        std::cerr << "YUV program link error:\n" << buf << "\n";
        return false;
    }
    // 绑定纹理单元
    glUseProgram(prog_);
    glUniform1i(glGetUniformLocation(prog_, "uTexY"), 0);
    glUniform1i(glGetUniformLocation(prog_, "uTexU"), 1);
    glUniform1i(glGetUniformLocation(prog_, "uTexV"), 2);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// upload_frame — 同步上传
// ─────────────────────────────────────────────────────────────────────────────
void YuvRenderer::upload_frame(const uint8_t* y,
                                const uint8_t* u,
                                const uint8_t* v) {
    const int uv_w = width_  / 2;
    const int uv_h = height_ / 2;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                    GL_RED, GL_UNSIGNED_BYTE, y);

    glBindTexture(GL_TEXTURE_2D, tex_u_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_w, uv_h,
                    GL_RED, GL_UNSIGNED_BYTE, u);

    glBindTexture(GL_TEXTURE_2D, tex_v_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_w, uv_h,
                    GL_RED, GL_UNSIGNED_BYTE, v);
}

// ─────────────────────────────────────────────────────────────────────────────
// upload_frame_pbo — 双 PBO 异步上传
//
// 流程（乒乓）：
//   帧 N:   把第 N 帧数据 memcpy 到 pbos_[pbo_idx_]（CPU 写 PBO）
//           然后用 pbos_[1-pbo_idx_]（已在 GPU 的上一帧数据）触发 DMA 传输
//   帧 N+1: 反转 pbo_idx_，重复
//
// 效果：CPU memcpy 与 GPU DMA 重叠，不阻塞渲染管线
// ─────────────────────────────────────────────────────────────────────────────
void YuvRenderer::upload_frame_pbo(const uint8_t* y,
                                    const uint8_t* u,
                                    const uint8_t* v) {
    const int uv_w   = width_  / 2;
    const int uv_h   = height_ / 2;
    size_t    y_sz   = (size_t)width_  * height_;
    size_t    uv_sz  = (size_t)uv_w    * uv_h;

    int cur  = pbo_idx_;
    int next = 1 - pbo_idx_;

    // ── Step 1：将当前帧数据 memcpy 到 PBO[cur]（CPU 写，DMA 之后处理）──
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[cur]);
    void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                  (GLsizeiptr)(y_sz + uv_sz * 2),
                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (ptr) {
        uint8_t* dst = (uint8_t*)ptr;
        memcpy(dst,              y, y_sz);
        memcpy(dst + y_sz,       u, uv_sz);
        memcpy(dst + y_sz + uv_sz, v, uv_sz);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    // ── Step 2：用 PBO[next]（上一帧已 DMA 完成）触发纹理更新 ──────────
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[next]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                    GL_RED, GL_UNSIGNED_BYTE, (void*)0);  // offset=0 in PBO

    glBindTexture(GL_TEXTURE_2D, tex_u_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_w, uv_h,
                    GL_RED, GL_UNSIGNED_BYTE, (void*)y_sz);

    glBindTexture(GL_TEXTURE_2D, tex_v_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_w, uv_h,
                    GL_RED, GL_UNSIGNED_BYTE, (void*)(y_sz + uv_sz));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    pbo_idx_ = next;  // 交换
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────
void YuvRenderer::render(YuvColorSpace cs, YuvRange range) {
    glUseProgram(prog_);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex_y_);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, tex_u_);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, tex_v_);

    glUniform1i(glGetUniformLocation(prog_, "uColorSpace"),
                cs == YuvColorSpace::BT601 ? 0 : 1);
    glUniform1i(glGetUniformLocation(prog_, "uRange"),
                range == YuvRange::Studio ? 0 : 1);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// destroy
// ─────────────────────────────────────────────────────────────────────────────
void YuvRenderer::destroy() {
    glDeleteTextures(1, &tex_y_);
    glDeleteTextures(1, &tex_u_);
    glDeleteTextures(1, &tex_v_);
    glDeleteBuffers(2, pbos_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(prog_);
    tex_y_ = tex_u_ = tex_v_ = 0;
    pbos_[0] = pbos_[1] = vbo_ = vao_ = prog_ = 0;
}
