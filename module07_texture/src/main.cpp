// module07_texture/src/main.cpp
// 演示：纹理贴图、多纹理混合、不同过滤/环绕模式对比、mipmap

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdint>

#include "shader.h"
#include "mesh.h"
#include "texture.h"

// ── Window ────────────────────────────────────────────────────────────────────
static int SCR_W = 1280, SCR_H = 720;

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    SCR_W = w; SCR_H = h;
    glViewport(0, 0, w, h);
}

// ── Keyboard state ────────────────────────────────────────────────────────────
static float g_mix_ratio = 0.5f;  // mix between two textures
// mode: 0=single, 1=mix, 2=filter_compare, 3=mipmap_demo
static int g_mode = 0;

static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, true);
        if (key == GLFW_KEY_1) g_mode = 0;
        if (key == GLFW_KEY_2) g_mode = 1;
        if (key == GLFW_KEY_3) g_mode = 2;
        if (key == GLFW_KEY_4) g_mode = 3;
        if (key == GLFW_KEY_UP)   g_mix_ratio = std::min(1.0f, g_mix_ratio + 0.05f);
        if (key == GLFW_KEY_DOWN) g_mix_ratio = std::max(0.0f, g_mix_ratio - 0.05f);
    }
}

// ── Procedural texture (fallback if PNG not found) ─────────────────────────
// Generate checkerboard in GPU memory directly
static GLuint make_checkerboard_tex(int size = 256, int cell = 16) {
    std::vector<uint8_t> pixels(size * size * 3);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            bool w = ((x/cell)+(y/cell)) % 2 == 0;
            int i = (y*size+x)*3;
            pixels[i+0] = pixels[i+1] = pixels[i+2] = w ? 220 : 40;
        }
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,size,size,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    GLfloat max_aniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY,&max_aniso);
    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY,max_aniso);
    glBindTexture(GL_TEXTURE_2D,0);
    return id;
}

static GLuint make_gradient_tex(int size = 256) {
    std::vector<uint8_t> pixels(size * size * 3);
    const float pi = 3.14159f;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = (float)x/(size-1);
            float v = (float)y/(size-1);
            float r = 0.5f+0.5f*std::cos(2.f*pi*(u+0.f/3.f));
            float g = 0.5f+0.5f*std::cos(2.f*pi*(u+1.f/3.f));
            float b = 0.5f+0.5f*std::cos(2.f*pi*(u+2.f/3.f));
            float br = 0.4f+0.6f*v;
            int i = (y*size+x)*3;
            pixels[i+0]=(uint8_t)(r*br*255);
            pixels[i+1]=(uint8_t)(g*br*255);
            pixels[i+2]=(uint8_t)(b*br*255);
        }
    GLuint id;
    glGenTextures(1,&id);
    glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,size,size,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,0);
    return id;
}

// ── Filter demo: create textures with specific filter settings ─────────────
struct FilterTex {
    GLuint id;
    GLint  min_filter, mag_filter;
    GLint  wrap_s,    wrap_t;
};

static GLuint clone_with_params(GLuint src_id, int w, int h,
                                 GLint min_f, GLint mag_f,
                                 GLint wrap_s, GLint wrap_t)
{
    // Read pixels from src
    std::vector<uint8_t> pixels(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, src_id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint id;
    glGenTextures(1,&id);
    glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap_s);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap_t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,min_f);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,mag_f);
    glBindTexture(GL_TEXTURE_2D,0);
    return id;
}

// ── Quad with custom UV (for filter/mipmap demos) ─────────────────────────
struct QuadInfo { GLuint vao, vbo, ebo; };

static QuadInfo make_uv_quad(float x0, float y0, float x1, float y1,
                              float u0, float v0, float u1, float v1) {
    float verts[] = {
        x0,y0,0, 0,0,1, u0,v0,
        x1,y0,0, 0,0,1, u1,v0,
        x1,y1,0, 0,0,1, u1,v1,
        x0,y1,0, 0,0,1, u0,v1,
    };
    uint32_t idx[] = {0,1,2, 2,3,0};
    QuadInfo q;
    glGenVertexArrays(1,&q.vao);
    glGenBuffers(1,&q.vbo);
    glGenBuffers(1,&q.ebo);
    glBindVertexArray(q.vao);
    glBindBuffer(GL_ARRAY_BUFFER,q.vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,q.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glBindVertexArray(0);
    return q;
}

static void draw_quad_info(const QuadInfo& q) {
    glBindVertexArray(q.vao);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
    glBindVertexArray(0);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(SCR_W, SCR_H,
        "Module 07 - Textures | 1=Single 2=Mix 3=Filter 4=Mipmap | UP/DOWN=mix ratio",
        nullptr, nullptr);
    if (!win) { std::cerr << "glfwCreateWindow failed\n"; return 1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_cb);
    glfwSetKeyCallback(win, key_cb);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n"; return 1;
    }
    glEnable(GL_DEPTH_TEST);

    // ── Shaders ──
    Shader shader("shaders/texture.vert", "shaders/texture.frag");

    // ── Textures ──
    // Try to load PNG from assets/, fall back to procedural
    GLuint tex_check = 0, tex_grad = 0;
    {
        Texture2D t = Texture2D::from_file("assets/checkerboard.png");
        if (t.id) tex_check = t.id;
    }
    if (!tex_check) tex_check = make_checkerboard_tex();
    {
        Texture2D t = Texture2D::from_file("assets/gradient.png");
        if (t.id) tex_grad = t.id;
    }
    if (!tex_grad) tex_grad = make_gradient_tex();

    // Filter demo textures (4 variants from checkerboard)
    GLuint t_nn = clone_with_params(tex_check,256,256,
        GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    GLuint t_ll = clone_with_params(tex_check,256,256,
        GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    GLuint t_clamp = clone_with_params(tex_check,256,256,
        GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    GLuint t_repeat = clone_with_params(tex_check,256,256,
        GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);

    // ── Meshes ──
    Mesh quad = Mesh::make_quad(2.0f);

    // 4 sub-quads for filter demo (NDC-ish layout)
    QuadInfo q_tl = make_uv_quad(-1.0f, 0.0f, 0.0f, 1.0f,  0,0,1,1);
    QuadInfo q_tr = make_uv_quad( 0.0f, 0.0f, 1.0f, 1.0f,  0,0,3,3); // repeat UV
    QuadInfo q_bl = make_uv_quad(-1.0f,-1.0f, 0.0f, 0.0f,  0,0,1,1);
    QuadInfo q_br = make_uv_quad( 0.0f,-1.0f, 1.0f, 0.0f,  0,0,1,1);

    // Identity matrices for 2D
    glm::mat4 ident(1.0f);

    shader.use();
    shader.set_int("uTex0", 0);
    shader.set_int("uTex1", 1);

    while (!glfwWindowShouldClose(win)) {
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.set_mat4("uModel",      ident);
        shader.set_mat4("uView",       ident);
        shader.set_mat4("uProjection", ident);
        shader.set_float("uMixRatio",  g_mix_ratio);

        if (g_mode == 0) {
            // Single texture on full quad
            shader.set_int("uMode", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_check);
            quad.draw();

        } else if (g_mode == 1) {
            // Multi-texture mix
            shader.set_int("uMode", 1);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex_check);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, tex_grad);
            quad.draw();

        } else if (g_mode == 2) {
            // Filter comparison: 4 sub-quads
            // TL = Nearest/Nearest+Clamp
            shader.set_int("uMode", 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t_nn);
            draw_quad_info(q_tl);
            // TR = Linear/Linear+Repeat (larger UV → repeat)
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t_repeat);
            draw_quad_info(q_tr);
            // BL = Linear+Clamp
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t_clamp);
            draw_quad_info(q_bl);
            // BR = Linear+Mipmap trilinear
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t_ll);
            draw_quad_info(q_br);

        } else if (g_mode == 3) {
            // Mipmap demo: draw same texture at 4 different sizes
            // We simulate "distance" by scaling down the quad
            float time = (float)glfwGetTime();
            shader.set_int("uMode", 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex_check);
            for (int i = 0; i < 4; ++i) {
                float s = 1.0f / (1 << i);       // 1, 0.5, 0.25, 0.125
                float ox = (i - 1.5f) * 0.45f;
                glm::mat4 model = glm::translate(ident, glm::vec3(ox, 0.0f, 0.0f));
                model = glm::scale(model, glm::vec3(s, s, 1.0f));
                shader.set_mat4("uModel", model);
                quad.draw();
            }
            shader.set_mat4("uModel", ident); // reset
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    // cleanup
    glDeleteTextures(1, &tex_check);
    glDeleteTextures(1, &tex_grad);
    glDeleteTextures(1, &t_nn);
    glDeleteTextures(1, &t_ll);
    glDeleteTextures(1, &t_clamp);
    glDeleteTextures(1, &t_repeat);
    quad.destroy();
    shader.destroy();
    glfwTerminate();
    return 0;
}
