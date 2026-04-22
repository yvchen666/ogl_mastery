// module14_advanced_gl/src/main.cpp
// 演示：实例化渲染、SSBO、间接绘制、UBO、Compute Shader
// ImGui 在 4 个演示之间切换

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "instance_renderer.h"
#include "indirect_draw.h"
#include "ubo_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

// ──────────────────────────────────────────────────────────────────────────────
// Utility
// ──────────────────────────────────────────────────────────────────────────────
static std::string read_file(const std::string& p) {
    std::ifstream f(p);
    if (!f.is_open()) { std::cerr << "Cannot open: " << p << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static GLuint compile_shader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char b[2048]; glGetShaderInfoLog(s, sizeof(b), nullptr, b);
               std::cerr << "Shader error:\n" << b << "\n"; }
    return s;
}

static GLuint make_program(const std::string& vs_path, const std::string& fs_path) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   read_file(vs_path));
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, read_file(fs_path));
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char b[2048]; glGetProgramInfoLog(p, sizeof(b), nullptr, b);
               std::cerr << "Link error:\n" << b << "\n"; }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

static GLuint make_compute_program(const std::string& cs_path) {
    GLuint cs = compile_shader(GL_COMPUTE_SHADER, read_file(cs_path));
    GLuint p  = glCreateProgram();
    glAttachShader(p, cs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char b[2048]; glGetProgramInfoLog(p, sizeof(b), nullptr, b);
               std::cerr << "Compute link error:\n" << b << "\n"; }
    glDeleteShader(cs);
    return p;
}

// ──────────────────────────────────────────────────────────────────────────────
// Simple cube mesh (pos + normal)
// ──────────────────────────────────────────────────────────────────────────────
Mesh make_cube() {
    float verts[] = {
        // pos            normal
        -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
         0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
        -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
         0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f,-0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f,-0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, 0.5f,-0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,-0.5f,-0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,-0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,
         0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,
         0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,
    };
    unsigned idx[] = {
         0, 1, 2,  2, 3, 0,   4, 5, 6,  6, 7, 4,
         8, 9,10, 10,11, 8,  12,13,14, 14,15,12,
        16,17,18, 18,19,16,  20,21,22, 22,23,20,
    };
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    m.index_count = 36;
    return m;
}

// ──────────────────────────────────────────────────────────────────────────────
// Camera
// ──────────────────────────────────────────────────────────────────────────────
struct Camera {
    glm::vec3 pos{0, 5, 30};
    float yaw{-90.0f}, pitch{-10.0f}, fov{45.0f};

    glm::vec3 front() const {
        return glm::normalize(glm::vec3(
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))));
    }
    glm::mat4 view()          const { return glm::lookAt(pos, pos + front(), {0,1,0}); }
    glm::mat4 proj(float asp) const { return glm::perspective(glm::radians(fov), asp, 0.1f, 500.f); }
};

static Camera g_cam;
static bool   g_mouse_captured = false;
static double g_last_mx = 0, g_last_my = 0;
static bool   g_first_mouse = true;

static void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (!g_mouse_captured) return;
    if (g_first_mouse) { g_last_mx = xpos; g_last_my = ypos; g_first_mouse = false; }
    g_cam.yaw   += (float)(xpos - g_last_mx) * 0.1f;
    g_cam.pitch += (float)(g_last_my - ypos) * 0.1f;
    g_cam.pitch  = glm::clamp(g_cam.pitch, -89.f, 89.f);
    g_last_mx = xpos; g_last_my = ypos;
}

// ──────────────────────────────────────────────────────────────────────────────
// Generate asteroid transforms (CPU)
// ──────────────────────────────────────────────────────────────────────────────
static std::vector<glm::mat4> gen_asteroid_transforms(int count, float ring_r = 15.0f) {
    std::vector<glm::mat4> out;
    out.reserve(count);
    srand(42);
    for (int i = 0; i < count; ++i) {
        float angle  = (float)i / count * 2.0f * glm::pi<float>();
        float spread = (float)(rand() % 100) / 100.0f * 4.0f - 2.0f;
        float height = (float)(rand() % 100) / 100.0f * 1.0f - 0.5f;
        float scale  = 0.05f + (float)(rand() % 100) / 100.0f * 0.1f;
        glm::mat4 m  = glm::translate(glm::mat4(1.f),
            glm::vec3((ring_r + spread) * std::cos(angle),
                       height,
                      (ring_r + spread) * std::sin(angle)));
        m = glm::rotate(m, angle * 3.0f, glm::vec3(0.4f, 0.6f, 0.8f));
        m = glm::scale(m, glm::vec3(scale));
        out.push_back(m);
    }
    return out;
}

// ──────────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────────
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Module14 - Advanced GL", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSetCursorPosCallback(win, mouse_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    ImGui::StyleColorsDark();

    // ── Shaders ───────────────────────────────────────────────────────────────
    GLuint instanced_prog  = make_program("shaders/instanced.vert",  "shaders/instanced.frag");
    GLuint ubo_prog        = make_program("shaders/ubo_demo.vert",   "shaders/ubo_demo.frag");
    GLuint compute_prog    = make_compute_program("shaders/orbit_update.comp");

    // Bind UBO blocks in programs
    auto bind_ubo = [](GLuint prog, const char* name, GLuint binding) {
        GLuint idx = glGetUniformBlockIndex(prog, name);
        if (idx != GL_INVALID_INDEX) glUniformBlockBinding(prog, idx, binding);
    };
    bind_ubo(instanced_prog, "CameraBlock", 0);
    bind_ubo(ubo_prog,       "CameraBlock", 0);
    bind_ubo(ubo_prog,       "LightBlock",  1);

    // ── Mesh ──────────────────────────────────────────────────────────────────
    Mesh cube = make_cube();

    // ── UBO Manager ───────────────────────────────────────────────────────────
    UboManager ubo_mgr;
    ubo_mgr.init();

    // ── Instance Renderer ─────────────────────────────────────────────────────
    const int  ASTEROID_COUNT = 10000;
    InstanceRenderer inst_renderer;
    inst_renderer.init(cube, ASTEROID_COUNT);

    auto asteroid_transforms = gen_asteroid_transforms(ASTEROID_COUNT);
    inst_renderer.upload_transforms(asteroid_transforms);

    // ── SSBO for Compute Shader (orbit positions) ─────────────────────────────
    // Format: vec4 per instance: xyz = position, w = orbit_speed
    struct OrbitParticle { glm::vec4 pos_speed; };
    std::vector<OrbitParticle> particles(ASTEROID_COUNT);
    {
        srand(42);
        for (int i = 0; i < ASTEROID_COUNT; ++i) {
            float angle = (float)i / ASTEROID_COUNT * 2.0f * glm::pi<float>();
            float r     = 15.0f + (rand() % 100) / 100.0f * 4.0f - 2.0f;
            particles[i].pos_speed = glm::vec4(r * std::cos(angle), 0.0f,
                                               r * std::sin(angle), r);
        }
    }
    GLuint orbit_ssbo;
    glGenBuffers(1, &orbit_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, orbit_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 ASTEROID_COUNT * sizeof(OrbitParticle),
                 particles.data(), GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // ── Indirect Draw ─────────────────────────────────────────────────────────
    IndirectDrawBuffer indirect_buf;
    indirect_buf.create(4);
    // 4 separate draw calls via multi-draw indirect
    std::vector<DrawElementsIndirectCommand> cmds = {
        {36, 2500,     0, 0,    0},  // group 0: instances 0–2499
        {36, 2500,     0, 0, 2500},  // group 1: instances 2500–4999
        {36, 2500,     0, 0, 5000},  // group 2: instances 5000–7499
        {36, 2500,     0, 0, 7500},  // group 3: instances 7500–9999
    };
    indirect_buf.upload(cmds);

    // ── Demo selection ────────────────────────────────────────────────────────
    int  demo        = 0; // 0=instanced, 1=ssbo_anim, 2=indirect, 3=ubo
    bool use_compute = false;
    float time_acc   = 0.0f;

    glm::vec4 lpos[4] = {{10,10,10,1},{-10,10,-10,1},{0,20,0,1},{5,-5,15,1}};
    glm::vec4 lcol[4] = {{1,1,1,1},{0.8f,0.8f,1,1},{1,0.9f,0.7f,1},{0.5f,1,0.5f,1}};

    float last_t = 0.0f;
    while (!glfwWindowShouldClose(win)) {
        float now   = (float)glfwGetTime();
        float delta = now - last_t; last_t = now;
        time_acc   += delta;

        glfwPollEvents();

        // Camera
        float speed = 20.0f * delta;
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) g_cam.pos += speed * g_cam.front();
            if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) g_cam.pos -= speed * g_cam.front();
            glm::vec3 right = glm::normalize(glm::cross(g_cam.front(), {0,1,0}));
            if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) g_cam.pos -= speed * right;
            if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) g_cam.pos += speed * right;
            if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win,1);
        }
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (!g_mouse_captured) { g_mouse_captured = true; g_first_mouse = true;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }
        } else if (g_mouse_captured) {
            g_mouse_captured = false;
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        int fb_w, fb_h;
        glfwGetFramebufferSize(win, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float asp = (float)fb_w / (float)fb_h;
        glm::mat4 view = g_cam.view();
        glm::mat4 proj = g_cam.proj(asp);

        // Update UBO
        ubo_mgr.update_camera(view, proj, g_cam.pos);
        ubo_mgr.update_lights(lpos, lcol, 4);

        // ── Compute Shader: GPU-side orbit update ─────────────────────────────
        if (use_compute || demo == 1) {
            glUseProgram(compute_prog);
            glUniform1f(glGetUniformLocation(compute_prog, "uDeltaTime"), delta);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, orbit_ssbo);
            // Dispatch: ceil(ASTEROID_COUNT / 256)
            int groups = (ASTEROID_COUNT + 255) / 256;
            glDispatchCompute(groups, 1, 1);
            // Memory barrier: SSBO writes visible to subsequent reads
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // Update instance transforms from orbit SSBO (read back for demo)
            // In a fully GPU-driven pipeline, we'd keep this on GPU;
            // for demo clarity we read back and re-upload.
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, orbit_ssbo);
            OrbitParticle* ptr = (OrbitParticle*)glMapBuffer(
                GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
            if (ptr) {
                for (int i = 0; i < ASTEROID_COUNT; ++i) {
                    glm::vec3 p   = glm::vec3(ptr[i].pos_speed);
                    float     scl = 0.05f + (i % 10) * 0.01f;
                    glm::mat4 m   = glm::translate(glm::mat4(1.f), p);
                    m = glm::rotate(m, time_acc * (0.5f + i * 0.0001f), glm::vec3(1,0.5f,0.3f));
                    m = glm::scale(m, glm::vec3(scl));
                    asteroid_transforms[i] = m;
                }
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            inst_renderer.upload_transforms(asteroid_transforms);
        }

        // ── Render ────────────────────────────────────────────────────────────
        if (demo == 0 || demo == 1) {
            // Instanced rendering
            glUseProgram(instanced_prog);
            inst_renderer.draw(ASTEROID_COUNT);

        } else if (demo == 2) {
            // Indirect draw
            glUseProgram(instanced_prog);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inst_renderer.ssbo());
            glBindVertexArray(cube.vao);
            indirect_buf.bind();
            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                        nullptr, 4, 0);
            glBindVertexArray(0);

        } else if (demo == 3) {
            // UBO demo: single large cube, lit via UBO
            glUseProgram(ubo_prog);
            glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(3.f));
            glUniformMatrix4fv(glGetUniformLocation(ubo_prog, "uModel"), 1, GL_FALSE,
                               glm::value_ptr(model));
            glBindVertexArray(cube.vao);
            glDrawElements(GL_TRIANGLES, cube.index_count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

        // ── ImGui ─────────────────────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Advanced GL");
        ImGui::Text("Select Demo:");
        ImGui::RadioButton("0: Instanced (10k asteroids)", &demo, 0);
        ImGui::RadioButton("1: GPU Orbit (Compute Shader)", &demo, 1);
        ImGui::RadioButton("2: Indirect Draw (MultiDraw)", &demo, 2);
        ImGui::RadioButton("3: UBO Demo (lit cube)",        &demo, 3);
        ImGui::Separator();
        ImGui::Checkbox("Compute orbit update", &use_compute);
        ImGui::Separator();
        ImGui::Text("Camera: (%.1f,%.1f,%.1f)", g_cam.pos.x,g_cam.pos.y,g_cam.pos.z);
        ImGui::Text("Right-click + drag to look, WASD to move");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }

    inst_renderer.destroy();
    indirect_buf.destroy();
    ubo_mgr.destroy();
    glDeleteBuffers(1, &orbit_ssbo);
    glDeleteVertexArrays(1, &cube.vao);
    glDeleteBuffers(1, &cube.vbo);
    glDeleteBuffers(1, &cube.ebo);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
