// module06_camera/src/main.cpp
// ─────────────────────────────────────────────────────────────────────────────
// 摄像机演示
//   FPS 摄像机：WASD 移动，鼠标旋转，滚轮调 FOV
//   轨道摄像机：鼠标右键拖拽旋转，滚轮缩放
//   Tab 键切换两种摄像机
//   场景中有多个球体/立方体，进行视锥体剔除演示（终端打印剔除数量）
// ─────────────────────────────────────────────────────────────────────────────

#include "camera.h"
#include "frustum.h"

// 复用 module05 的 Shader / Mesh（直接内联实现）
// ── 由于 module06 不依赖 module05 的构建，这里直接 copy 最小实现 ──────────────

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <array>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// 内嵌 Shader（避免跨模块依赖）
// ─────────────────────────────────────────────────────────────────────────────
static GLuint compile_shader_file(GLenum type, const char* path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error(std::string("Cannot open: ") + path);
    std::ostringstream ss; ss << f.rdbuf();
    std::string src = ss.str();
    const char* c   = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("Shader: ") + path + " → " + log);
    }
    return s;
}

static GLuint make_program(const char* vp, const char* fp)
{
    GLuint vs = compile_shader_file(GL_VERTEX_SHADER, vp);
    GLuint fs = compile_shader_file(GL_FRAGMENT_SHADER, fp);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(p,1024,nullptr,log);
               glDeleteProgram(p); throw std::runtime_error(log); }
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// 内嵌 Mesh（仅立方体和球体）
// ─────────────────────────────────────────────────────────────────────────────
struct Vertex { glm::vec3 pos, nrm; glm::vec2 uv; };

struct Mesh {
    GLuint vao{0}, vbo{0}, ebo{0};
    GLsizei idx_cnt{0};

    Mesh(const std::vector<Vertex>& V, const std::vector<uint32_t>& I)
    {
        idx_cnt = static_cast<GLsizei>(I.size());
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, V.size()*sizeof(Vertex), V.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, I.size()*sizeof(uint32_t), I.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,pos));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,nrm));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }
    ~Mesh() {
        if(ebo) glDeleteBuffers(1,&ebo);
        if(vbo) glDeleteBuffers(1,&vbo);
        if(vao) glDeleteVertexArrays(1,&vao);
    }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, idx_cnt, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};

static Mesh make_cube()
{
    auto face = [](std::vector<Vertex>& V, std::vector<uint32_t>& I,
                   glm::vec3 p0,glm::vec3 p1,glm::vec3 p2,glm::vec3 p3,glm::vec3 n){
        uint32_t b = V.size();
        V.push_back({p0,n,{0,0}}); V.push_back({p1,n,{1,0}});
        V.push_back({p2,n,{1,1}}); V.push_back({p3,n,{0,1}});
        I.insert(I.end(),{b,b+1,b+2, b,b+2,b+3});
    };
    std::vector<Vertex> V; std::vector<uint32_t> I;
    face(V,I,{-0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{0,0,1});
    face(V,I,{ 0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{0,0,-1});
    face(V,I,{ 0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f, 0.5f},{1,0,0});
    face(V,I,{-0.5f,-0.5f,-0.5f},{-0.5f,-0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f, 0.5f,-0.5f},{-1,0,0});
    face(V,I,{-0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},{0,1,0});
    face(V,I,{-0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f, 0.5f},{-0.5f,-0.5f, 0.5f},{0,-1,0});
    return Mesh(V,I);
}

static Mesh make_sphere(int stacks=16, int slices=32)
{
    const float PI = 3.14159265358979f;
    std::vector<Vertex> V; std::vector<uint32_t> I;
    for(int i=0;i<=stacks;++i){
        float phi=PI*i/stacks, sp=std::sin(phi), cp=std::cos(phi);
        for(int j=0;j<=slices;++j){
            float theta=2*PI*j/slices, st=std::sin(theta), ct=std::cos(theta);
            glm::vec3 p={sp*ct,cp,sp*st};
            V.push_back({p,p,{(float)j/slices,(float)i/stacks}});
        }
    }
    for(int i=0;i<stacks;++i)
        for(int j=0;j<slices;++j){
            uint32_t a=i*(slices+1)+j,b=a+1,c=a+(slices+1),d=c+1;
            I.insert(I.end(),{a,c,b, b,c,d});
        }
    return Mesh(V,I);
}

// ─────────────────────────────────────────────────────────────────────────────
// 全局状态
// ─────────────────────────────────────────────────────────────────────────────
static FpsCamera   g_fps;
static OrbitCamera g_orbit;
static bool        g_use_fps     = true;   // true=FPS, false=Orbit
static bool        g_first_mouse = true;
static double      g_last_mx     = 0, g_last_my = 0;
static bool        g_right_btn   = false;  // 轨道摄像机：右键拖拽
static float       g_last_time   = 0;

// ─── 回调 ────────────────────────────────────────────────────────────────────
static void key_cb(GLFWwindow* w, int key, int, int action, int)
{
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
        if (key == GLFW_KEY_TAB) {
            g_use_fps = !g_use_fps;
            std::printf("Camera: %s\n", g_use_fps ? "FPS" : "Orbit");
            // 切换时释放鼠标（FPS模式锁定）
            if (g_use_fps)
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            else
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            g_first_mouse = true;
        }
    }
}

static void mouse_button_cb(GLFWwindow*, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        g_right_btn = (action == GLFW_PRESS);
}

static void cursor_pos_cb(GLFWwindow*, double xpos, double ypos)
{
    if (g_first_mouse) {
        // 第一帧跳跃防护：直接记录位置，不计算位移
        g_last_mx = xpos;
        g_last_my = ypos;
        g_first_mouse = false;
        return;
    }

    float dx = (float)(xpos - g_last_mx);
    float dy = (float)(ypos - g_last_my);
    g_last_mx = xpos;
    g_last_my = ypos;

    if (g_use_fps) {
        g_fps.process_mouse(dx, dy);
    } else {
        if (g_right_btn)
            g_orbit.process_mouse_drag(dx, dy);
    }
}

static void scroll_cb(GLFWwindow*, double, double yoff)
{
    if (g_use_fps)
        g_fps.process_scroll((float)yoff);
    else
        g_orbit.process_scroll((float)yoff);
}

static void framebuffer_cb(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    if (!glfwInit()) { std::fprintf(stderr,"glfwInit\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1024, 768,
        "Module06 – Camera  [TAB=Switch  RMB=Orbit Drag  Scroll=Zoom  ESC=Quit]",
        nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(win);
    glfwSetKeyCallback          (win, key_cb);
    glfwSetMouseButtonCallback  (win, mouse_button_cb);
    glfwSetCursorPosCallback    (win, cursor_pos_cb);
    glfwSetScrollCallback       (win, scroll_cb);
    glfwSetFramebufferSizeCallback(win, framebuffer_cb);
    glfwSwapInterval(1);

    // FPS 模式默认锁定鼠标
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr,"gladLoadGLLoader\n"); return 1;
    }
    glEnable(GL_DEPTH_TEST);

    std::printf("OpenGL %s\n", glGetString(GL_VERSION));
    std::printf("FPS Camera: WASD=Move  Mouse=Rotate  Scroll=FOV\n");
    std::printf("Tab=Switch camera  (Orbit: RMB drag, Scroll=zoom)\n");
    std::printf("Frustum cull stats printed every 2 seconds.\n\n");

    // ── 着色器 ────────────────────────────────────────────────────────────
    GLuint prog = make_program("shaders/scene.vert", "shaders/scene.frag");

    // ── 几何体 ────────────────────────────────────────────────────────────
    Mesh cube   = make_cube();
    Mesh sphere = make_sphere();

    // ── 场景：20 个物体（球 + 立方体）随机分布在 ±15 范围 ────────────────
    struct SceneObj {
        glm::vec3 pos;
        float     scale;
        glm::vec3 color;
        bool      is_sphere;   // 用于视锥剔除半径计算
    };

    std::vector<SceneObj> scene;
    // 用固定种子生成（避免 <random>）
    auto rng = [](unsigned& s) -> float {
        s = s * 1664525u + 1013904223u;
        return (float)(s & 0xFFFF) / 65535.0f;
    };
    unsigned seed = 42;
    for (int i = 0; i < 20; ++i) {
        float x = (rng(seed)*2-1)*14.0f;
        float y = (rng(seed)*2-1)*5.0f;
        float z = (rng(seed)*2-1)*14.0f;
        float s = 0.5f + rng(seed) * 1.5f;
        glm::vec3 col = {0.3f+rng(seed)*0.7f,
                         0.3f+rng(seed)*0.7f,
                         0.3f+rng(seed)*0.7f};
        bool sph = (i % 3 == 0);
        scene.push_back({{x,y,z}, s, col, sph});
    }

    glm::vec3 light_pos  {5.0f, 8.0f, 5.0f};
    glm::vec3 light_color{1.0f, 1.0f, 1.0f};

    double last_print = -999.0;

    // ── 主循环 ────────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float dt  = now - g_last_time;
        g_last_time = now;

        glfwPollEvents();

        // FPS 键盘处理（持续按键）
        if (g_use_fps) {
            for (int k : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
                          GLFW_KEY_SPACE, GLFW_KEY_LEFT_CONTROL}) {
                if (glfwGetKey(win, k) == GLFW_PRESS)
                    g_fps.process_keyboard(k, dt);
            }
        }

        int fb_w, fb_h;
        glfwGetFramebufferSize(win, &fb_w, &fb_h);
        float aspect = (fb_h > 0) ? (float)fb_w / fb_h : 1.0f;

        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view, proj;
        glm::vec3 cam_pos;

        if (g_use_fps) {
            view    = g_fps.view_matrix();
            proj    = g_fps.proj_matrix(aspect);
            cam_pos = g_fps.position;
        } else {
            view    = g_orbit.view_matrix();
            proj    = g_orbit.proj_matrix(aspect);
            cam_pos = g_orbit.position();
        }

        // 提取视锥体平面
        Frustum frustum = Frustum::from_vp(proj * view);

        glUseProgram(prog);
        glUniform3fv(glGetUniformLocation(prog,"uLightPos"),   1, glm::value_ptr(light_pos));
        glUniform3fv(glGetUniformLocation(prog,"uLightColor"), 1, glm::value_ptr(light_color));
        glUniform3fv(glGetUniformLocation(prog,"uViewPos"),    1, glm::value_ptr(cam_pos));
        glUniformMatrix4fv(glGetUniformLocation(prog,"uView"),       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog,"uProjection"), 1, GL_FALSE, glm::value_ptr(proj));

        int drawn = 0, culled = 0;
        for (const auto& obj : scene) {
            // ── 视锥剔除（AABB 测试）─────────────────────────────────────
            glm::vec3 half{obj.scale * 0.5f};
            if (!frustum.intersects_aabb(obj.pos - half, obj.pos + half)) {
                ++culled;
                continue;
            }

            glm::mat4 model{1.0f};
            model = glm::translate(model, obj.pos);
            model = glm::scale    (model, glm::vec3{obj.scale});
            glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

            glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),        1,GL_FALSE,glm::value_ptr(model));
            glUniformMatrix3fv(glGetUniformLocation(prog,"uNormalMatrix"),  1,GL_FALSE,glm::value_ptr(normalMat));
            glUniform3fv      (glGetUniformLocation(prog,"uObjectColor"),   1,glm::value_ptr(obj.color));

            if (obj.is_sphere) sphere.draw();
            else               cube.draw();
            ++drawn;
        }

        if (now - last_print >= 2.0) {
            last_print = now;
            std::printf("[t=%.1fs] cam=(%.2f,%.2f,%.2f)  drawn=%d  culled=%d\n",
                        now, cam_pos.x, cam_pos.y, cam_pos.z, drawn, culled);
        }

        glfwSwapBuffers(win);
    }

    glDeleteProgram(prog);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
