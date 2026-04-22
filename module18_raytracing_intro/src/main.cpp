#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// 场景球体描述（与 raytracer.comp 中的 Sphere struct 完全对应）
// ─────────────────────────────────────────────────────────────────────────────
struct GpuSphere {
    glm::vec4 center_radius;    // xyz=center, w=radius
    glm::vec4 albedo;           // rgb=color,  a=roughness
    glm::vec4 material_params;  // x=mat_type(0=diffuse,1=metal,2=glass), y=ior
};

// ─── 窗口状态 ─────────────────────────────────────────────────────────────────
static int   g_width  = 1280;
static int   g_height = 720;
static int   g_frame_count = 0;

// ─── 摄像机 ───────────────────────────────────────────────────────────────────
static glm::vec3 g_cam_pos(0.0f, 2.0f, 8.0f);
static glm::vec3 g_cam_front(0.0f, -0.2f, -1.0f);
static float g_cam_yaw   = -90.0f;
static float g_cam_pitch = -10.0f;
static double g_last_x   = 640.0, g_last_y = 360.0;
static bool   g_first_mouse = true;
static bool   g_mouse_captured = false;

static void recalc_camera() {
    glm::vec3 f;
    f.x = cosf(glm::radians(g_cam_yaw)) * cosf(glm::radians(g_cam_pitch));
    f.y = sinf(glm::radians(g_cam_pitch));
    f.z = sinf(glm::radians(g_cam_yaw)) * cosf(glm::radians(g_cam_pitch));
    g_cam_front = glm::normalize(f);
}

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
    g_frame_count = 0;  // 分辨率改变 → 重置累积
}

static void mouse_cb(GLFWwindow*, double xpos, double ypos) {
    if (!g_mouse_captured) { g_first_mouse = true; return; }
    if (g_first_mouse) { g_last_x = xpos; g_last_y = ypos; g_first_mouse = false; }
    float dx = (float)(xpos - g_last_x) * 0.1f;
    float dy = (float)(g_last_y - ypos) * 0.1f;
    g_last_x = xpos; g_last_y = ypos;
    g_cam_yaw   += dx;
    g_cam_pitch  = glm::clamp(g_cam_pitch + dy, -89.0f, 89.0f);
    recalc_camera();
    g_frame_count = 0;
}

static void mouse_button_cb(GLFWwindow* win, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_RIGHT) {
        g_mouse_captured = (action == GLFW_PRESS);
        glfwSetInputMode(win, GLFW_CURSOR,
                         g_mouse_captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (g_mouse_captured) g_frame_count = 0;
    }
}

static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        g_frame_count = 0;
        std::cout << "[RT] Reset accumulation\n";
    }
}

static void process_input(GLFWwindow* win, float dt) {
    if (!g_mouse_captured) return;
    float speed = 3.0f * dt;
    bool moved = false;
    glm::vec3 right = glm::normalize(glm::cross(g_cam_front, {0,1,0}));
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) { g_cam_pos += g_cam_front * speed; moved=true; }
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) { g_cam_pos -= g_cam_front * speed; moved=true; }
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) { g_cam_pos -= right * speed; moved=true; }
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) { g_cam_pos += right * speed; moved=true; }
    if (moved) g_frame_count = 0;
}

// ─── Shader 工具 ──────────────────────────────────────────────────────────────
static std::string read_file(const std::string& p) {
    std::ifstream f(p); if (!f) { std::cerr << "Cannot open: " << p << "\n"; return ""; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
static GLuint compile(GLenum t, const std::string& s) {
    GLuint sh = glCreateShader(t);
    const char* c = s.c_str();
    glShaderSource(sh, 1, &c, nullptr); glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[2048]; glGetShaderInfoLog(sh,2048,nullptr,buf);
               std::cerr << "Shader compile error:\n" << buf << "\n"; }
    return sh;
}

static GLuint make_compute_prog(const std::string& path) {
    auto src = read_file(path);
    GLuint cs = compile(GL_COMPUTE_SHADER, src);
    GLuint p  = glCreateProgram();
    glAttachShader(p, cs); glLinkProgram(p); glDeleteShader(cs);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetProgramInfoLog(p,1024,nullptr,buf);
               std::cerr << "Compute link:\n" << buf << "\n"; }
    return p;
}

static GLuint make_render_prog(const std::string& vert, const std::string& frag) {
    GLuint vs = compile(GL_VERTEX_SHADER,   read_file(vert));
    GLuint fs = compile(GL_FRAGMENT_SHADER, read_file(frag));
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// 场景定义：地面 + 5 个球体（漫反射 / 金属 / 玻璃）
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<GpuSphere> build_scene() {
    std::vector<GpuSphere> spheres;

    auto add = [&](glm::vec3 center, float r,
                   glm::vec3 albedo, float roughness,
                   int mat_type, float ior = 1.5f) {
        GpuSphere s;
        s.center_radius  = glm::vec4(center, r);
        s.albedo         = glm::vec4(albedo, roughness);
        s.material_params = glm::vec4((float)mat_type, ior, 0, 0);
        spheres.push_back(s);
    };

    // 地面（大球，半径 100）
    add({0, -100.5f, -1}, 100.0f, {0.5f, 0.8f, 0.3f}, 0.0f, 0 /*diffuse*/);

    // 中心：漫反射蓝色
    add({ 0.0f, 0.0f, -2.0f}, 0.5f, {0.1f, 0.2f, 0.9f}, 0.0f, 0);

    // 左侧：玻璃球（ior=1.5）
    add({-1.2f, 0.0f, -2.0f}, 0.5f, {1.0f, 1.0f, 1.0f}, 0.0f, 2 /*glass*/, 1.5f);

    // 右侧：金属（低粗糙度）
    add({ 1.2f, 0.0f, -2.0f}, 0.5f, {0.8f, 0.6f, 0.2f}, 0.05f, 1 /*metal*/);

    // 小球：漫反射红色
    add({-0.4f, -0.2f, -1.2f}, 0.3f, {0.9f, 0.2f, 0.2f}, 0.0f, 0);

    // 小球：磨砂金属
    add({ 0.5f, -0.25f, -1.0f}, 0.25f, {0.7f, 0.7f, 0.9f}, 0.5f, 1);

    return spheres;
}

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(g_width, g_height,
        "Module 18 – Path Tracer | R=reset | RMB=camera", nullptr, nullptr);
    if (!win) return -1;
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_cb);
    glfwSetCursorPosCallback(win, mouse_cb);
    glfwSetMouseButtonCallback(win, mouse_button_cb);
    glfwSetKeyCallback(win, key_cb);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
    std::cout << "Controls: RMB+drag=camera, WASD=move, R=reset, ESC=quit\n";

    // ── 着色器程序 ────────────────────────────────────────────────────────
    GLuint rt_prog  = make_compute_prog("shaders/raytracer.comp");
    GLuint blit_prog = make_render_prog("shaders/fullscreen.vert",
                                        "shaders/fullscreen_tonemap.frag");

    // ── 累积纹理（rgba32f）────────────────────────────────────────────────
    GLuint accum_tex;
    glGenTextures(1, &accum_tex);
    glBindTexture(GL_TEXTURE_2D, accum_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, g_width, g_height, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // ── 场景 SSBO ─────────────────────────────────────────────────────────
    auto scene = build_scene();
    GLuint scene_ssbo;
    glGenBuffers(1, &scene_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 scene.size() * sizeof(GpuSphere),
                 scene.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, scene_ssbo);

    // ── 全屏空 VAO（用于 blit pass）──────────────────────────────────────
    GLuint empty_vao;
    glGenVertexArrays(1, &empty_vao);

    recalc_camera();

    double prev = glfwGetTime();
    double fps_acc = 0.0; int fps_count = 0;

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float  dt  = (float)(now - prev); prev = now;
        fps_acc += dt; fps_count++;

        glfwPollEvents();
        process_input(win, dt);

        // ── 视图/投影矩阵 ─────────────────────────────────────────────────
        glm::mat4 view = glm::lookAt(g_cam_pos, g_cam_pos + g_cam_front, {0,1,0});
        glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                           (float)g_width / (float)g_height,
                                           0.1f, 100.0f);
        glm::mat4 inv_view = glm::inverse(view);
        glm::mat4 inv_proj = glm::inverse(proj);

        // ── 如果累积纹理尺寸与窗口不符，重建 ────────────────────────────
        {
            int tw, th;
            glBindTexture(GL_TEXTURE_2D, accum_tex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &tw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
            if (tw != g_width || th != g_height) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, g_width, g_height,
                             0, GL_RGBA, GL_FLOAT, nullptr);
                g_frame_count = 0;
            }
        }

        // ── Compute Pass：路径追踪，1 spp，累积 ──────────────────────────
        glUseProgram(rt_prog);
        glBindImageTexture(0, accum_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, scene_ssbo);
        glUniform1i(glGetUniformLocation(rt_prog, "uFrameCount"), g_frame_count);
        glUniformMatrix4fv(glGetUniformLocation(rt_prog, "uInvView"), 1, GL_FALSE,
                           glm::value_ptr(inv_view));
        glUniformMatrix4fv(glGetUniformLocation(rt_prog, "uInvProj"), 1, GL_FALSE,
                           glm::value_ptr(inv_proj));

        int gx = (g_width  + 15) / 16;
        int gy = (g_height + 15) / 16;
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        g_frame_count++;

        // ── Blit Pass：tone map + gamma，显示到屏幕 ────────────────────
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(blit_prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, accum_tex);
        glUniform1i(glGetUniformLocation(blit_prog, "uAccumTex"), 0);
        glBindVertexArray(empty_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);  // 全屏大三角形

        // ── FPS 和帧数显示 ────────────────────────────────────────────────
        if (fps_acc >= 1.0) {
            char title[128];
            snprintf(title, sizeof(title),
                     "Module 18 – Path Tracer | FPS: %d | Samples: %d",
                     fps_count, g_frame_count);
            glfwSetWindowTitle(win, title);
            fps_count = 0; fps_acc = 0.0;
        }

        glfwSwapBuffers(win);
    }

    glDeleteTextures(1, &accum_tex);
    glDeleteBuffers(1, &scene_ssbo);
    glDeleteVertexArrays(1, &empty_vao);
    glDeleteProgram(rt_prog);
    glDeleteProgram(blit_prog);
    glfwTerminate();
    return 0;
}
