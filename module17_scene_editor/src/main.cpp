#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ecs.h"
#include "components.h"
#include "gltf_loader.h"
#include "picking.h"

// ─── 窗口状态 ─────────────────────────────────────────────────────────────────
static int   g_width  = 1280;
static int   g_height = 720;
static float g_cam_yaw  = -90.0f;
static float g_cam_pitch = 0.0f;
static glm::vec3 g_cam_pos(0.0f, 2.0f, 8.0f);
static glm::vec3 g_cam_front(0.0f, 0.0f, -1.0f);
static double    g_last_x = 640.0, g_last_y = 360.0;
static bool      g_first_mouse = true;
static bool      g_mouse_captured = false;

// ─── 场景状态 ─────────────────────────────────────────────────────────────────
static World     g_world;
static EntityId  g_selected = INVALID_ENTITY;
static EntityId  g_light_entity = INVALID_ENTITY;

// ─── GPU Query（耗时）────────────────────────────────────────────────────────
static GLuint g_query[2] = {0, 0};  // 0=scene pass, 1=picking pass
static double g_scene_ms = 0.0, g_pick_ms = 0.0;

// ─────────────────────────────────────────────────────────────────────────────
static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}

static void mouse_cb(GLFWwindow* win, double xpos, double ypos) {
    if (!g_mouse_captured) { g_first_mouse = true; return; }
    if (g_first_mouse) {
        g_last_x = xpos; g_last_y = ypos; g_first_mouse = false;
    }
    float dx = (float)(xpos - g_last_x) * 0.1f;
    float dy = (float)(g_last_y - ypos) * 0.1f;
    g_last_x = xpos; g_last_y = ypos;
    g_cam_yaw   += dx;
    g_cam_pitch  = glm::clamp(g_cam_pitch + dy, -89.0f, 89.0f);
    glm::vec3 f;
    f.x = cosf(glm::radians(g_cam_yaw)) * cosf(glm::radians(g_cam_pitch));
    f.y = sinf(glm::radians(g_cam_pitch));
    f.z = sinf(glm::radians(g_cam_yaw)) * cosf(glm::radians(g_cam_pitch));
    g_cam_front = glm::normalize(f);
}

static void mouse_button_cb(GLFWwindow* win, int btn, int action, int) {
    // 右键：控制相机
    if (btn == GLFW_MOUSE_BUTTON_RIGHT) {
        g_mouse_captured = (action == GLFW_PRESS);
        glfwSetInputMode(win, GLFW_CURSOR,
                         g_mouse_captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

static void process_input(GLFWwindow* win, float dt) {
    if (!g_mouse_captured) return;
    float speed = 5.0f * dt;
    glm::vec3 right = glm::normalize(glm::cross(g_cam_front, {0,1,0}));
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) g_cam_pos += g_cam_front * speed;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) g_cam_pos -= g_cam_front * speed;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) g_cam_pos -= right * speed;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) g_cam_pos += right * speed;
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// ─── 着色器工具 ───────────────────────────────────────────────────────────────
static std::string read_file(const std::string& p) {
    std::ifstream f(p); if (!f) return "";
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
static GLuint compile(GLenum t, const std::string& s) {
    GLuint sh = glCreateShader(t);
    const char* c = s.c_str();
    glShaderSource(sh, 1, &c, nullptr); glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetShaderInfoLog(sh,1024,nullptr,buf);
               std::cerr << buf << "\n"; }
    return sh;
}
static GLuint link_prog(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// 创建内置立方体（无 glTF 文件时的默认场景）
// ─────────────────────────────────────────────────────────────────────────────
static void create_default_scene() {
    // 简单立方体顶点（位置 + 法线 + uv）
    static const float verts[] = {
        // pos              normal           uv
        -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.0f,0.0f,
         0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  1.0f,0.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  1.0f,1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.0f,1.0f,
        -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.0f,0.0f,
         0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  1.0f,0.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  1.0f,1.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.0f,1.0f,
    };
    static const uint32_t idx[] = {0,1,2,2,3,0, 4,5,6,6,7,4,
                                    0,4,7,7,3,0, 1,5,6,6,2,1,
                                    3,2,6,6,7,3, 0,1,5,5,4,0};
    GLuint vbo, ebo, vao;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // 创建三个立方体实体
    for (int i = 0; i < 3; ++i) {
        EntityId eid = g_world.create_entity();
        NameComponent nc; nc.name = "Cube_" + std::to_string(i);
        g_world.add_component(eid, nc);
        TransformComponent tc;
        tc.pos = glm::vec3((float)(i-1) * 2.5f, 0.0f, 0.0f);
        g_world.add_component(eid, tc);
        MeshComponent mc; mc.vao = vao; mc.index_count = 36;
        g_world.add_component(eid, mc);
    }

    // 光源实体
    g_light_entity = g_world.create_entity();
    NameComponent lnc; lnc.name = "PointLight";
    g_world.add_component(g_light_entity, lnc);
    TransformComponent ltc; ltc.pos = glm::vec3(3.0f, 5.0f, 3.0f);
    g_world.add_component(g_light_entity, ltc);
    LightComponent lc; lc.color = glm::vec3(1.0f); lc.intensity = 1.0f;
    g_world.add_component(g_light_entity, lc);
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(g_width, g_height,
                                        "Module 17 – Scene Editor", nullptr, nullptr);
    if (!win) return -1;
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_cb);
    glfwSetCursorPosCallback(win, mouse_cb);
    glfwSetMouseButtonCallback(win, mouse_button_cb);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    glEnable(GL_DEPTH_TEST);

    // ── ImGui 初始化 ─────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // ── 着色器 ───────────────────────────────────────────────────────────
    GLuint scene_prog = link_prog(
        compile(GL_VERTEX_SHADER,   read_file("shaders/scene.vert")),
        compile(GL_FRAGMENT_SHADER, read_file("shaders/scene.frag")));

    // ── 场景 ─────────────────────────────────────────────────────────────
    if (argc >= 2) {
        load_gltf(argv[1], g_world);
        // 找到第一个灯光
        g_world.each<LightComponent>([&](EntityId eid, LightComponent&) {
            if (g_light_entity == INVALID_ENTITY) g_light_entity = eid;
        });
        if (g_light_entity == INVALID_ENTITY) {
            // 若 glTF 无灯光，手动加一个
            g_light_entity = g_world.create_entity();
            NameComponent nc; nc.name = "DefaultLight";
            g_world.add_component(g_light_entity, nc);
            TransformComponent tc; tc.pos = glm::vec3(3,5,3);
            g_world.add_component(g_light_entity, tc);
            LightComponent lc;
            g_world.add_component(g_light_entity, lc);
        }
    } else {
        create_default_scene();
    }

    // ── Picking ──────────────────────────────────────────────────────────
    Picking picking;
    picking.init(g_width, g_height);

    // ── GPU Query ────────────────────────────────────────────────────────
    glGenQueries(2, g_query);

    double prev = glfwGetTime();

    // ─── 主循环 ──────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float  dt  = (float)(now - prev); prev = now;

        glfwPollEvents();
        process_input(win, dt);

        // ── ImGui 鼠标拾取（左键单击，非 ImGui 捕获时）────────────────
        if (!ImGui::GetIO().WantCaptureMouse &&
            glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(win, &mx, &my);

            glm::mat4 view = glm::lookAt(g_cam_pos, g_cam_pos+g_cam_front, {0,1,0});
            glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                               (float)g_width/(float)g_height,
                                               0.1f, 100.0f);

            // 渲染 ID pass
            glBeginQuery(GL_TIME_ELAPSED, g_query[1]);
            picking.render_ids(g_world, proj * view);
            glEndQuery(GL_TIME_ELAPSED);

            g_selected = picking.read_entity((int)mx, (int)my);
        }

        // ── 读取 GPU Query 结果（上一帧）────────────────────────────────
        {
            GLuint64 t;
            glGetQueryObjectui64v(g_query[0], GL_QUERY_RESULT_NO_WAIT, &t);
            g_scene_ms = (double)t * 1e-6;
            glGetQueryObjectui64v(g_query[1], GL_QUERY_RESULT_NO_WAIT, &t);
            g_pick_ms  = (double)t * 1e-6;
        }

        // ── 场景渲染 ─────────────────────────────────────────────────────
        glViewport(0, 0, g_width, g_height);
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(g_cam_pos, g_cam_pos+g_cam_front, {0,1,0});
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                           (float)g_width/(float)g_height,
                                           0.1f, 100.0f);

        // 获取灯光信息
        glm::vec3 light_pos(3,5,3), light_color(1);
        float light_int = 1.0f;
        if (g_light_entity != INVALID_ENTITY) {
            auto* ltc = g_world.get_component<TransformComponent>(g_light_entity);
            auto* lc  = g_world.get_component<LightComponent>(g_light_entity);
            if (ltc) light_pos   = ltc->pos;
            if (lc)  { light_color = lc->color; light_int = lc->intensity; }
        }

        glBeginQuery(GL_TIME_ELAPSED, g_query[0]);
        glUseProgram(scene_prog);
        glUniformMatrix4fv(glGetUniformLocation(scene_prog,"uView"), 1,GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(scene_prog,"uProj"), 1,GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(scene_prog,"uLightPos"),   1, glm::value_ptr(light_pos));
        glUniform3fv(glGetUniformLocation(scene_prog,"uLightColor"), 1, glm::value_ptr(light_color));
        glUniform1f(glGetUniformLocation(scene_prog,"uLightIntensity"), light_int);
        glUniform3fv(glGetUniformLocation(scene_prog,"uCamPos"),     1, glm::value_ptr(g_cam_pos));

        g_world.each<MeshComponent>([&](EntityId eid, MeshComponent& mc) {
            if (mc.vao == 0 || mc.index_count == 0) return;
            auto* tc = g_world.get_component<TransformComponent>(eid);
            glm::mat4 model = tc ? tc->matrix() : glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(scene_prog,"uModel"), 1,GL_FALSE, glm::value_ptr(model));
            bool has_tex = (mc.albedo_tex != 0);
            glUniform1i(glGetUniformLocation(scene_prog,"uHasTexture"), has_tex ? 1 : 0);
            if (has_tex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mc.albedo_tex);
                glUniform1i(glGetUniformLocation(scene_prog,"uAlbedo"), 0);
            }
            glUniform1i(glGetUniformLocation(scene_prog,"uSelected"), eid == g_selected ? 1 : 0);
            glBindVertexArray(mc.vao);
            glDrawElements(GL_TRIANGLES, mc.index_count, GL_UNSIGNED_INT, nullptr);
        });
        glEndQuery(GL_TIME_ELAPSED);

        // ── ImGui UI ─────────────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 场景图面板
        ImGui::Begin("Scene Graph");
        ImGui::Text("GPU Scene: %.2f ms", g_scene_ms);
        ImGui::Text("GPU Pick:  %.2f ms", g_pick_ms);
        ImGui::Separator();
        for (EntityId eid : g_world.all_entities()) {
            auto* nc = g_world.get_component<NameComponent>(eid);
            std::string label = (nc ? nc->name : "Entity") + "##" + std::to_string(eid);
            bool selected = (eid == g_selected);
            if (ImGui::Selectable(label.c_str(), selected))
                g_selected = eid;
        }
        ImGui::End();

        // 属性面板
        if (g_selected != INVALID_ENTITY) {
            ImGui::Begin("Properties");
            auto* nc = g_world.get_component<NameComponent>(g_selected);
            if (nc) ImGui::Text("Name: %s", nc->name.c_str());

            auto* tc = g_world.get_component<TransformComponent>(g_selected);
            if (tc) {
                ImGui::DragFloat3("Position", glm::value_ptr(tc->pos), 0.05f);
                ImGui::DragFloat3("Scale",    glm::value_ptr(tc->scale), 0.01f, 0.01f, 100.0f);
            }
            auto* lc = g_world.get_component<LightComponent>(g_selected);
            if (lc) {
                ImGui::ColorEdit3("Light Color",  glm::value_ptr(lc->color));
                ImGui::SliderFloat("Intensity", &lc->intensity, 0.0f, 10.0f);
            }
            if (ImGui::Button("Delete Entity")) {
                g_world.destroy_entity(g_selected);
                g_selected = INVALID_ENTITY;
            }
            ImGui::End();
        }

        // 光源控制面板
        ImGui::Begin("Light");
        if (g_light_entity != INVALID_ENTITY) {
            auto* ltc = g_world.get_component<TransformComponent>(g_light_entity);
            auto* lc  = g_world.get_component<LightComponent>(g_light_entity);
            if (ltc) ImGui::DragFloat3("Position##L", glm::value_ptr(ltc->pos), 0.1f);
            if (lc) {
                ImGui::ColorEdit3("Color", glm::value_ptr(lc->color));
                ImGui::SliderFloat("Intensity", &lc->intensity, 0.0f, 10.0f);
            }
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }

    picking.destroy();
    glDeleteQueries(2, g_query);
    glDeleteProgram(scene_prog);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
