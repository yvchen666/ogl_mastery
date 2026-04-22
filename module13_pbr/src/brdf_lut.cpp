#include "brdf_lut.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

static std::string brdf_read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "[brdf_lut] cannot open: " << path << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static GLuint brdf_compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
               std::cerr << "[brdf_lut] " << buf << "\n"; }
    return s;
}

GLuint generate_brdf_lut(int size) {
    // Create LUT texture
    GLuint lut;
    glGenTextures(1, &lut);
    glBindTexture(GL_TEXTURE_2D, lut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, size, size, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Full-screen quad shader
    const char* vert_src = R"(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() { vUV = aUV; gl_Position = vec4(aPos, 1.0); }
)";
    std::string frag_src = brdf_read_file("shaders/brdf_lut.frag");
    GLuint vs = brdf_compile(GL_VERTEX_SHADER, vert_src);
    GLuint fs = brdf_compile(GL_FRAGMENT_SHADER, frag_src);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    // FBO
    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lut, 0);

    // Draw full-screen quad
    float verts[] = {
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    glViewport(0, 0, size, size);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);

    return lut;
}
