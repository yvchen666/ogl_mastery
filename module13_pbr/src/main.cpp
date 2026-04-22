// module13_pbr/src/main.cpp
// PBR 材质球演示：5×5 矩阵，IBL 天空盒，4 个点光源，ImGui 参数调节
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "cubemap.h"
#include "brdf_lut.h"
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

// ──────────────────────────────────────────────────────────────────────────────
// Sphere mesh
// ──────────────────────────────────────────────────────────────────────────────
struct SphereMesh {
    GLuint vao{0}, vbo{0}, ebo{0};
    GLsizei index_count{0};
};

SphereMesh make_sphere(int x_segs = 64, int y_segs = 64) {
    SphereMesh m;
    std::vector<float>    verts;
    std::vector<unsigned> idx;

    for (int y = 0; y <= y_segs; ++y) {
        for (int x = 0; x <= x_segs; ++x) {
            float xSeg = (float)x / x_segs;
            float ySeg = (float)y / y_segs;
            float xPos = std::cos(xSeg * 2.0f * glm::pi<float>()) * std::sin(ySeg * glm::pi<float>());
            float yPos = std::cos(ySeg * glm::pi<float>());
            float zPos = std::sin(xSeg * 2.0f * glm::pi<float>()) * std::sin(ySeg * glm::pi<float>());
            // pos, normal, uv
            verts.insert(verts.end(), {xPos, yPos, zPos, xPos, yPos, zPos, xSeg, ySeg});
        }
    }
    bool odd = false;
    for (int y = 0; y < y_segs; ++y) {
        if (!odd) {
            for (int x = 0; x <= x_segs; ++x) {
                idx.push_back(y       * (x_segs + 1) + x);
                idx.push_back((y + 1) * (x_segs + 1) + x);
            }
        } else {
            for (int x = x_segs; x >= 0; --x) {
                idx.push_back((y + 1) * (x_segs + 1) + x);
                idx.push_back(y       * (x_segs + 1) + x);
            }
        }
        odd = !odd;
    }
    m.index_count = (GLsizei)idx.size();

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    return m;
}

// ──────────────────────────────────────────────────────────────────────────────
// Skybox cube VAO
// ──────────────────────────────────────────────────────────────────────────────
static GLuint g_sky_vao = 0, g_sky_vbo = 0;
static void init_skybox_vao() {
    float v[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };
    glGenVertexArrays(1, &g_sky_vao); glGenBuffers(1, &g_sky_vbo);
    glBindVertexArray(g_sky_vao); glBindBuffer(GL_ARRAY_BUFFER, g_sky_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

// ──────────────────────────────────────────────────────────────────────────────
// Camera
// ──────────────────────────────────────────────────────────────────────────────
struct Camera {
    glm::vec3 pos{0, 0, 12};
    float yaw{-90.0f}, pitch{0.0f};
    float fov{45.0f};

    glm::vec3 front() const {
        return glm::normalize(glm::vec3(
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))));
    }
    glm::mat4 view() const { return glm::lookAt(pos, pos + front(), glm::vec3(0,1,0)); }
    glm::mat4 proj(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
    }
};

static Camera  g_cam;
static bool    g_mouse_captured = false;
static double  g_last_mx = 0, g_last_my = 0;
static bool    g_first_mouse = true;

static void mouse_callback(GLFWwindow* w, double xpos, double ypos) {
    (void)w;
    if (!g_mouse_captured) return;
    if (g_first_mouse) { g_last_mx = xpos; g_last_my = ypos; g_first_mouse = false; }
    float dx = (float)(xpos - g_last_mx) * 0.1f;
    float dy = (float)(g_last_my - ypos) * 0.1f;
    g_last_mx = xpos; g_last_my = ypos;
    g_cam.yaw   += dx;
    g_cam.pitch += dy;
    g_cam.pitch  = glm::clamp(g_cam.pitch, -89.0f, 89.0f);
}

// ──────────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────────
int main() {
    // GLFW init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Module13 - PBR", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSetCursorPosCallback(win, mouse_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL); // for skybox

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    ImGui::StyleColorsDark();

    // ── Shaders ──────────────────────────────────────────────────────────────
    GLuint pbr_prog = make_program("shaders/pbr.vert", "shaders/pbr.frag");
    GLuint sky_prog = make_program("shaders/skybox.vert", "shaders/skybox.frag");

    // ── Load HDR environment ─────────────────────────────────────────────────
    stbi_set_flip_vertically_on_load(true);
    int hdr_w, hdr_h, hdr_nc;
    // Fallback: generate a simple gradient if no HDR file exists
    float* hdr_data = stbi_loadf("assets/hdr/newport_loft.hdr", &hdr_w, &hdr_h, &hdr_nc, 0);
    GLuint equirect_tex;
    glGenTextures(1, &equirect_tex);
    glBindTexture(GL_TEXTURE_2D, equirect_tex);
    if (hdr_data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, hdr_w, hdr_h, 0, GL_RGB, GL_FLOAT, hdr_data);
        stbi_image_free(hdr_data);
    } else {
        // Simple procedural 2×2 HDR fallback
        std::cerr << "[main] HDR not found, using fallback env\n";
        float fallback[4 * 3] = {
            2.0f, 2.0f, 2.5f,   1.5f, 1.5f, 2.0f,
            1.8f, 1.6f, 1.5f,   2.2f, 2.0f, 1.8f
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 2, 2, 0, GL_RGB, GL_FLOAT, fallback);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ── IBL precompute ────────────────────────────────────────────────────────
    std::cout << "Converting equirect to cubemap...\n";
    Cubemap env_cubemap   = Cubemap::from_equirect(equirect_tex, 512);
    std::cout << "Convolving irradiance...\n";
    Cubemap irradiance    = Cubemap::convolve_irradiance(env_cubemap.id, 32);
    std::cout << "Prefiltering env...\n";
    Cubemap prefilter     = Cubemap::prefilter_env(env_cubemap.id, 128);
    std::cout << "Generating BRDF LUT...\n";
    GLuint  brdf_lut      = generate_brdf_lut(512);
    std::cout << "IBL precompute done.\n";

    // ── Sphere mesh ───────────────────────────────────────────────────────────
    SphereMesh sphere = make_sphere(64, 64);
    init_skybox_vao();

    // ── Lights ────────────────────────────────────────────────────────────────
    glm::vec3 light_positions[] = {
        {-10.0f,  10.0f, 10.0f},
        { 10.0f,  10.0f, 10.0f},
        {-10.0f, -10.0f, 10.0f},
        { 10.0f, -10.0f, 10.0f},
    };
    glm::vec3 light_colors[] = {
        {300.0f, 300.0f, 300.0f},
        {300.0f, 300.0f, 300.0f},
        {300.0f, 300.0f, 300.0f},
        {300.0f, 300.0f, 300.0f},
    };

    // ── Material params ───────────────────────────────────────────────────────
    float albedo[3] = {1.0f, 0.5f, 0.0f};
    float ao = 1.0f;

    const int NR = 5; // rows/cols

    // ── Main loop ─────────────────────────────────────────────────────────────
    float last_time = 0.0f;
    while (!glfwWindowShouldClose(win)) {
        float now   = (float)glfwGetTime();
        float delta = now - last_time;
        last_time   = now;

        glfwPollEvents();

        // Camera movement
        float speed = 5.0f * delta;
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) g_cam.pos += speed * g_cam.front();
            if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) g_cam.pos -= speed * g_cam.front();
            glm::vec3 right = glm::normalize(glm::cross(g_cam.front(), glm::vec3(0,1,0)));
            if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) g_cam.pos -= speed * right;
            if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) g_cam.pos += speed * right;
            if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
        }

        // Toggle mouse capture with right click
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (!g_mouse_captured) {
                g_mouse_captured = true; g_first_mouse = true;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        } else if (g_mouse_captured) {
            g_mouse_captured = false;
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        int fb_w, fb_h;
        glfwGetFramebufferSize(win, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (float)fb_w / (float)fb_h;
        glm::mat4 view = g_cam.view();
        glm::mat4 proj = g_cam.proj(aspect);

        // ── PBR pass ──────────────────────────────────────────────────────────
        glUseProgram(pbr_prog);
        glUniformMatrix4fv(glGetUniformLocation(pbr_prog, "uView"),       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(pbr_prog, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(pbr_prog, "uCamPos"), 1, glm::value_ptr(g_cam.pos));
        // lights
        for (int i = 0; i < 4; ++i) {
            std::string lp = "uLightPositions[" + std::to_string(i) + "]";
            std::string lc = "uLightColors["    + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(pbr_prog, lp.c_str()), 1, glm::value_ptr(light_positions[i]));
            glUniform3fv(glGetUniformLocation(pbr_prog, lc.c_str()), 1, glm::value_ptr(light_colors[i]));
        }
        // IBL textures
        irradiance.bind(0);  glUniform1i(glGetUniformLocation(pbr_prog, "uIrradianceMap"), 0);
        prefilter.bind(1);   glUniform1i(glGetUniformLocation(pbr_prog, "uPrefilterMap"),  1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, brdf_lut);
        glUniform1i(glGetUniformLocation(pbr_prog, "uBrdfLut"), 2);
        glUniform3fv(glGetUniformLocation(pbr_prog, "uAlbedo"), 1, albedo);
        glUniform1f(glGetUniformLocation(pbr_prog, "uAo"), ao);

        // 5×5 sphere matrix
        glBindVertexArray(sphere.vao);
        float spacing = 2.5f;
        float offset  = (NR - 1) * spacing * 0.5f;
        for (int row = 0; row < NR; ++row) {
            float metallic = (float)row / (float)(NR - 1);
            glUniform1f(glGetUniformLocation(pbr_prog, "uMetallic"), metallic);
            for (int col = 0; col < NR; ++col) {
                float roughness = glm::clamp((float)col / (float)(NR - 1), 0.05f, 1.0f);
                glUniform1f(glGetUniformLocation(pbr_prog, "uRoughness"), roughness);
                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                    glm::vec3(col * spacing - offset, row * spacing - offset, 0.0f));
                glUniformMatrix4fv(glGetUniformLocation(pbr_prog, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
                glDrawElements(GL_TRIANGLE_STRIP, sphere.index_count, GL_UNSIGNED_INT, nullptr);
            }
        }
        glBindVertexArray(0);

        // ── Skybox ────────────────────────────────────────────────────────────
        glUseProgram(sky_prog);
        glm::mat4 sky_view = glm::mat4(glm::mat3(view)); // strip translation
        glUniformMatrix4fv(glGetUniformLocation(sky_prog, "uView"),       1, GL_FALSE, glm::value_ptr(sky_view));
        glUniformMatrix4fv(glGetUniformLocation(sky_prog, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
        env_cubemap.bind(0);
        glUniform1i(glGetUniformLocation(sky_prog, "uEnvMap"), 0);
        glBindVertexArray(g_sky_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        // ── ImGui ─────────────────────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("PBR Controls");
        ImGui::Text("5x5 sphere matrix: col=roughness, row=metallic");
        ImGui::Separator();
        ImGui::ColorEdit3("Albedo", albedo);
        ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Camera: (%.1f, %.1f, %.1f)", g_cam.pos.x, g_cam.pos.y, g_cam.pos.z);
        ImGui::Text("Right-click + drag to look, WASD to move");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }

    // Cleanup
    sphere.~SphereMesh();
    irradiance.destroy(); prefilter.destroy(); env_cubemap.destroy();
    glDeleteTextures(1, &brdf_lut);
    glDeleteTextures(1, &equirect_tex);
    glDeleteVertexArrays(1, &g_sky_vao); glDeleteBuffers(1, &g_sky_vbo);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
