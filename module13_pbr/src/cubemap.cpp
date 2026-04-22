#include "cubemap.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ──────────────────────────────────────────────────────────────────────────────

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[cubemap] cannot open: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_shader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        std::cerr << "[cubemap] shader error:\n" << buf << "\n";
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        std::cerr << "[cubemap] link error:\n" << buf << "\n";
    }
    return p;
}

// Unit cube VAO
static GLuint g_cube_vao = 0;
static GLuint g_cube_vbo = 0;

static void ensure_cube() {
    if (g_cube_vao) return;
    float verts[] = {
        // positions
        -1.0f,  1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,    1.0f,  1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,    1.0f, -1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f,  1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f, -1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f,  1.0f, -1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f,  1.0f
    };
    glGenVertexArrays(1, &g_cube_vao);
    glGenBuffers(1, &g_cube_vbo);
    glBindVertexArray(g_cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

// Full-screen quad VAO
static GLuint g_quad_vao = 0;
static GLuint g_quad_vbo = 0;

static void ensure_quad() {
    if (g_quad_vao) return;
    float verts[] = {
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
    };
    glGenVertexArrays(1, &g_quad_vao);
    glGenBuffers(1, &g_quad_vbo);
    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

static const glm::mat4 k_capture_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
static const glm::mat4 k_capture_views[6] = {
    glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
};

// ──────────────────────────────────────────────────────────────────────────────
// Cubemap::from_faces
// ──────────────────────────────────────────────────────────────────────────────
Cubemap Cubemap::from_faces(const std::vector<std::string>& paths) {
    Cubemap cm;
    glGenTextures(1, &cm.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm.id);

    int w, h, nc;
    for (int i = 0; i < 6; ++i) {
        unsigned char* data = stbi_load(paths[i].c_str(), &w, &h, &nc, 0);
        if (data) {
            GLenum fmt = (nc == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, fmt, w, h, 0,
                         fmt, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "[cubemap] failed to load: " << paths[i] << "\n";
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return cm;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cubemap::from_equirect
// ──────────────────────────────────────────────────────────────────────────────
Cubemap Cubemap::from_equirect(GLuint equirect_tex, int size) {
    ensure_cube();

    // Build cubemap texture
    Cubemap cm;
    glGenTextures(1, &cm.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm.id);
    for (int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Shaders
    std::string vert_src = read_file("shaders/equirect_to_cubemap.vert");
    std::string frag_src = read_file("shaders/equirect_to_cubemap.frag");
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    // FBO
    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uEquirectMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, equirect_tex);

    glViewport(0, 0, size, size);
    for (int i = 0; i < 6; ++i) {
        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE,
                           &k_capture_views[i][0][0]);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProjection"), 1, GL_FALSE,
                           &k_capture_proj[0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm.id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(g_cube_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteProgram(prog);

    // Generate mipmaps for prefilter
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm.id);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return cm;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cubemap::convolve_irradiance
// ──────────────────────────────────────────────────────────────────────────────
Cubemap Cubemap::convolve_irradiance(GLuint env_cubemap, int size) {
    ensure_cube();

    Cubemap cm;
    glGenTextures(1, &cm.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm.id);
    for (int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::string vert_src = read_file("shaders/equirect_to_cubemap.vert");
    std::string frag_src = read_file("shaders/irradiance_conv.frag");
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uEnvMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap);

    glViewport(0, 0, size, size);
    for (int i = 0; i < 6; ++i) {
        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE,
                           &k_capture_views[i][0][0]);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProjection"), 1, GL_FALSE,
                           &k_capture_proj[0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm.id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(g_cube_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteProgram(prog);
    return cm;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cubemap::prefilter_env
// ──────────────────────────────────────────────────────────────────────────────
Cubemap Cubemap::prefilter_env(GLuint env_cubemap, int size) {
    ensure_cube();

    Cubemap cm;
    glGenTextures(1, &cm.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm.id);
    for (int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    std::string vert_src = read_file("shaders/equirect_to_cubemap.vert");
    std::string frag_src = read_file("shaders/prefilter_env.frag");
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uEnvMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env_cubemap);

    const int MAX_MIP = 5;
    for (int mip = 0; mip < MAX_MIP; ++mip) {
        int mip_size = static_cast<int>(size * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_size, mip_size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

        glViewport(0, 0, mip_size, mip_size);
        float roughness = (float)mip / (float)(MAX_MIP - 1);
        glUniform1f(glGetUniformLocation(prog, "uRoughness"), roughness);

        for (int i = 0; i < 6; ++i) {
            glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE,
                               &k_capture_views[i][0][0]);
            glUniformMatrix4fv(glGetUniformLocation(prog, "uProjection"), 1, GL_FALSE,
                               &k_capture_proj[0][0]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm.id, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBindVertexArray(g_cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteProgram(prog);
    return cm;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cubemap::bind / destroy
// ──────────────────────────────────────────────────────────────────────────────
void Cubemap::bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);
}

void Cubemap::destroy() {
    if (id) { glDeleteTextures(1, &id); id = 0; }
}
