// module10_shadow/src/main.cpp
// Shadow mapping demo: Hard Shadow / PCF / PCSS switchable via keys 1/2/3
// Light position can be adjusted with WASD+QE keys.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shadow_map.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ─── Window ──────────────────────────────────────────────────────────────────
static int  SCR_W = 1280, SCR_H = 720;
static bool g_wireframe = false;
static int  g_shadow_mode = 1;   // 0=hard 1=PCF 2=PCSS
static float g_light_angle = 0.0f;
static bool  g_auto_rotate = true;

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    SCR_W = w; SCR_H = h;
    glViewport(0, 0, w, h);
}
static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, true);
        if (key == GLFW_KEY_1) { g_shadow_mode = 0; printf("Hard shadow\n"); }
        if (key == GLFW_KEY_2) { g_shadow_mode = 1; printf("PCF shadow\n"); }
        if (key == GLFW_KEY_3) { g_shadow_mode = 2; printf("PCSS shadow\n"); }
        if (key == GLFW_KEY_R) g_auto_rotate = !g_auto_rotate;
        if (key == GLFW_KEY_W) g_wireframe = !g_wireframe;
    }
}

// ─── Shader utils ────────────────────────────────────────────────────────────
static std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error(std::string("Cannot open: ") + path);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}
static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        throw std::runtime_error(std::string("Shader compile error:\n") + log);
    }
    return s;
}
static GLuint make_program(const char* vert_path, const char* frag_path) {
    auto vs_src = read_file(vert_path);
    auto fs_src = read_file(frag_path);
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   vs_src.c_str());
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src.c_str());
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(p, 512, nullptr, log);
        throw std::runtime_error(std::string("Program link error:\n") + log);
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// ─── Geometry helpers ────────────────────────────────────────────────────────
struct Mesh {
    GLuint vao{0}, vbo{0}, ebo{0};
    GLsizei index_count{0};
    void draw() const { glBindVertexArray(vao); glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr); }
};

// Interleaved: pos(3) + normal(3)
static Mesh build_cube() {
    // clang-format off
    float v[] = {
        // positions          // normals
        // Back face
        -0.5f,-0.5f,-0.5f,  0, 0,-1,
         0.5f,-0.5f,-0.5f,  0, 0,-1,
         0.5f, 0.5f,-0.5f,  0, 0,-1,
        -0.5f, 0.5f,-0.5f,  0, 0,-1,
        // Front face
        -0.5f,-0.5f, 0.5f,  0, 0, 1,
         0.5f,-0.5f, 0.5f,  0, 0, 1,
         0.5f, 0.5f, 0.5f,  0, 0, 1,
        -0.5f, 0.5f, 0.5f,  0, 0, 1,
        // Left face
        -0.5f,-0.5f,-0.5f, -1, 0, 0,
        -0.5f, 0.5f,-0.5f, -1, 0, 0,
        -0.5f, 0.5f, 0.5f, -1, 0, 0,
        -0.5f,-0.5f, 0.5f, -1, 0, 0,
        // Right face
         0.5f,-0.5f,-0.5f,  1, 0, 0,
         0.5f, 0.5f,-0.5f,  1, 0, 0,
         0.5f, 0.5f, 0.5f,  1, 0, 0,
         0.5f,-0.5f, 0.5f,  1, 0, 0,
        // Bottom face
        -0.5f,-0.5f,-0.5f,  0,-1, 0,
         0.5f,-0.5f,-0.5f,  0,-1, 0,
         0.5f,-0.5f, 0.5f,  0,-1, 0,
        -0.5f,-0.5f, 0.5f,  0,-1, 0,
        // Top face
        -0.5f, 0.5f,-0.5f,  0, 1, 0,
         0.5f, 0.5f,-0.5f,  0, 1, 0,
         0.5f, 0.5f, 0.5f,  0, 1, 0,
        -0.5f, 0.5f, 0.5f,  0, 1, 0,
    };
    unsigned idx[] = {
        0,1,2, 2,3,0,   4,5,6, 6,7,4,
        8,9,10,10,11,8, 12,13,14,14,15,12,
        16,17,18,18,19,16, 20,21,22,22,23,20
    };
    // clang-format on
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    m.index_count = 36;
    glBindVertexArray(0);
    return m;
}

static Mesh build_plane() {
    float v[] = {
        -10,-0.5f,-10,  0,1,0,
         10,-0.5f,-10,  0,1,0,
         10,-0.5f, 10,  0,1,0,
        -10,-0.5f, 10,  0,1,0,
    };
    unsigned idx[] = {0,1,2, 2,3,0};
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    m.index_count = 6;
    glBindVertexArray(0);
    return m;
}

// ─── main ────────────────────────────────────────────────────────────────────
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(SCR_W, SCR_H, "module10 Shadow Map / PCF / PCSS", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_cb);
    glfwSetKeyCallback(win, key_cb);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    // ── Shaders ──────────────────────────────────────────────────────────────
    GLuint prog_depth    = make_program("shaders/shadow_depth.vert",    "shaders/shadow_depth.frag");
    GLuint prog_lighting = make_program("shaders/shadow_lighting.vert", "shaders/shadow_lighting.frag");

    // ── Shadow map ───────────────────────────────────────────────────────────
    ShadowMap shadow_map;
    shadow_map.create(2048, 2048);

    // ── Geometry ─────────────────────────────────────────────────────────────
    Mesh cube  = build_cube();
    Mesh plane = build_plane();

    // Cube world transforms
    struct CubeInst { glm::vec3 pos; float scale; glm::vec3 color; };
    CubeInst cubes[] = {
        {{ 0.0f, 0.0f,  0.0f}, 1.0f, {0.8f, 0.4f, 0.2f}},
        {{ 2.5f, 0.5f, -1.0f}, 1.2f, {0.2f, 0.6f, 0.8f}},
        {{-2.0f, 1.0f,  2.0f}, 1.5f, {0.5f, 0.8f, 0.3f}},
        {{ 1.0f, 0.3f,  3.0f}, 0.8f, {0.9f, 0.7f, 0.1f}},
        {{-3.5f, 0.5f, -2.0f}, 1.0f, {0.6f, 0.3f, 0.9f}},
    };

    // Scene bounding sphere
    glm::vec3 scene_center{0.0f, 1.0f, 0.0f};
    float     scene_radius = 8.0f;

    printf("Controls: 1=Hard 2=PCF 3=PCSS  R=toggle auto-rotate  W=wireframe\n");

    float last_time = 0.0f;
    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float dt  = now - last_time;
        last_time = now;

        if (g_auto_rotate)
            g_light_angle += dt * 0.5f;

        // Manual light control
        if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) g_light_angle -= dt * 1.0f;
        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) g_light_angle += dt * 1.0f;

        glm::vec3 light_dir = glm::vec3(
            sinf(g_light_angle) * 0.7f,
            -1.0f,
            cosf(g_light_angle) * 0.7f
        );

        // ── Pass 1: depth map ─────────────────────────────────────────────
        // Render front faces only avoids self-shadowing (Peter Panning tradeoff)
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        glm::mat4 lsm = shadow_map.light_space_matrix(light_dir, scene_center, scene_radius);

        shadow_map.bind_for_write();
        glUseProgram(prog_depth);
        glUniformMatrix4fv(glGetUniformLocation(prog_depth, "uLightSpaceMVP"), 1, GL_FALSE, glm::value_ptr(lsm));

        auto draw_scene_depth = [&]() {
            // Plane
            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(prog_depth, "uLightSpaceMVP"), 1, GL_FALSE,
                               glm::value_ptr(lsm * model));
            plane.draw();
            // Cubes
            for (auto& ci : cubes) {
                model = glm::translate(glm::mat4(1.0f), ci.pos);
                model = glm::scale(model, glm::vec3(ci.scale));
                glUniformMatrix4fv(glGetUniformLocation(prog_depth, "uLightSpaceMVP"), 1, GL_FALSE,
                                   glm::value_ptr(lsm * model));
                cube.draw();
            }
        };
        draw_scene_depth();

        glCullFace(GL_BACK);

        // ── Pass 2: lighting ──────────────────────────────────────────────
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_W, SCR_H);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, g_wireframe ? GL_LINE : GL_FILL);

        glm::mat4 view = glm::lookAt(
            glm::vec3(5.0f, 7.0f, 10.0f),
            scene_center,
            glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                          (float)SCR_W / (float)SCR_H,
                                          0.1f, 100.0f);

        glUseProgram(prog_lighting);
        glUniformMatrix4fv(glGetUniformLocation(prog_lighting, "uView"),            1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog_lighting, "uProjection"),      1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(prog_lighting, "uLightSpaceMatrix"),1, GL_FALSE, glm::value_ptr(lsm));
        glUniform3fv(glGetUniformLocation(prog_lighting, "uLightDir"),    1, glm::value_ptr(-glm::normalize(light_dir)));
        glUniform3f (glGetUniformLocation(prog_lighting, "uLightColor"),  1.0f, 1.0f, 0.95f);
        glUniform3f (glGetUniformLocation(prog_lighting, "uViewPos"),     5.0f, 7.0f, 10.0f);
        glUniform1i (glGetUniformLocation(prog_lighting, "uShadowMode"),  g_shadow_mode);
        glUniform1f (glGetUniformLocation(prog_lighting, "uLightSize"),   3.0f);
        glUniform1i (glGetUniformLocation(prog_lighting, "uShadowMap"),   0);
        shadow_map.bind_depth_tex(0);

        // Draw plane
        {
            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(prog_lighting, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(glGetUniformLocation(prog_lighting, "uObjectColor"), 0.7f, 0.7f, 0.7f);
            plane.draw();
        }
        // Draw cubes
        for (auto& ci : cubes) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), ci.pos);
            model = glm::scale(model, glm::vec3(ci.scale));
            glUniformMatrix4fv(glGetUniformLocation(prog_lighting, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(glGetUniformLocation(prog_lighting, "uObjectColor"), 1, glm::value_ptr(ci.color));
            cube.draw();
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    shadow_map.destroy();
    glfwTerminate();
    return 0;
}
