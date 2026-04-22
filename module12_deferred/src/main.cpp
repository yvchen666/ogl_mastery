// module12_deferred/src/main.cpp
// Deferred rendering pipeline: Geometry Pass → SSAO → Lighting Pass (32 point lights)
// Keys: 1=full shading  2=position  3=normals  4=albedo  5=SSAO

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gbuffer.h"
#include "ssao.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <random>

// ─── State ───────────────────────────────────────────────────────────────────
static int  SCR_W = 1280, SCR_H = 720;
static int  g_vis_mode = 0;   // 0=shading 1-4=G-buffer channels
static bool g_rotate   = true;

static void fb_size_cb(GLFWwindow*, int w, int h) {
    SCR_W = w; SCR_H = h; glViewport(0,0,w,h);
}
static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, true);
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
            g_vis_mode = key - GLFW_KEY_1;
            const char* n[]={"Shading","Position","Normal","Albedo","SSAO"};
            printf("Visualizing: %s\n", n[g_vis_mode]);
        }
        if (key == GLFW_KEY_R) g_rotate = !g_rotate;
    }
}

// ─── Shader utils ────────────────────────────────────────────────────────────
static std::string read_file(const char* p) {
    std::ifstream f(p);
    if (!f) throw std::runtime_error(std::string("open: ")+p);
    std::ostringstream s; s << f.rdbuf(); return s.str();
}
static GLuint compile(GLenum t, const char* src) {
    GLuint s = glCreateShader(t); glShaderSource(s,1,&src,nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char l[1024]; glGetShaderInfoLog(s,1024,nullptr,l); throw std::runtime_error(l); }
    return s;
}
static GLuint make_prog(const char* vp, const char* fp) {
    auto vs = read_file(vp); auto fs = read_file(fp);
    GLuint v = compile(GL_VERTEX_SHADER, vs.c_str());
    GLuint f = compile(GL_FRAGMENT_SHADER, fs.c_str());
    GLuint p = glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char l[1024]; glGetProgramInfoLog(p,1024,nullptr,l); throw std::runtime_error(l); }
    glDeleteShader(v); glDeleteShader(f); return p;
}

// ─── Geometry ────────────────────────────────────────────────────────────────
struct Mesh { GLuint vao{0},vbo{0},ebo{0}; GLsizei nc{0}; void draw() const { glBindVertexArray(vao); glDrawElements(GL_TRIANGLES,nc,GL_UNSIGNED_INT,nullptr); }};

static Mesh build_cube() {
    float v[] = {
        -0.5f,-0.5f,-0.5f,0,0,-1,  0.5f,-0.5f,-0.5f,0,0,-1,  0.5f,0.5f,-0.5f,0,0,-1, -0.5f,0.5f,-0.5f,0,0,-1,
        -0.5f,-0.5f, 0.5f,0,0, 1,  0.5f,-0.5f, 0.5f,0,0, 1,  0.5f,0.5f, 0.5f,0,0, 1, -0.5f,0.5f, 0.5f,0,0, 1,
        -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f,0.5f,-0.5f,-1,0,0, -0.5f,0.5f,0.5f,-1,0,0, -0.5f,-0.5f,0.5f,-1,0,0,
         0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,-0.5f, 1,0,0,  0.5f,0.5f,0.5f, 1,0,0,  0.5f,-0.5f,0.5f, 1,0,0,
        -0.5f,-0.5f,-0.5f,0,-1,0,  0.5f,-0.5f,-0.5f,0,-1,0,  0.5f,-0.5f,0.5f,0,-1,0, -0.5f,-0.5f,0.5f,0,-1,0,
        -0.5f, 0.5f,-0.5f,0, 1,0,  0.5f, 0.5f,-0.5f,0, 1,0,  0.5f, 0.5f,0.5f,0, 1,0, -0.5f, 0.5f,0.5f,0, 1,0,
    };
    unsigned idx[] = {0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20};
    Mesh m; glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo); glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*4,(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*4,(void*)(3*4));
    m.nc = 36; glBindVertexArray(0); return m;
}

static void draw_fullscreen() {
    static GLuint vao = 0;
    if (!vao) glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// ─── main ────────────────────────────────────────────────────────────────────
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(SCR_W, SCR_H, "module12 Deferred + SSAO (32 lights)", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, fb_size_cb);
    glfwSetKeyCallback(win, key_cb);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    // ── Programs ──────────────────────────────────────────────────────────────
    GLuint prog_geo      = make_prog("shaders/gbuffer.vert",     "shaders/gbuffer.frag");
    GLuint prog_ssao     = make_prog("shaders/fullscreen.vert",  "shaders/ssao.frag");
    GLuint prog_ssao_blur= make_prog("shaders/fullscreen.vert",  "shaders/ssao_blur.frag");
    GLuint prog_lighting = make_prog("shaders/fullscreen.vert",  "shaders/deferred_lighting.frag");

    // ── G-Buffer + SSAO ────────────────────────────────────────────────────────
    GBuffer gbuf;
    if (!gbuf.create(SCR_W, SCR_H))
        throw std::runtime_error("GBuffer FBO incomplete");

    SSAO ssao;
    ssao.init(SCR_W, SCR_H);

    // ── Scene objects ──────────────────────────────────────────────────────────
    Mesh cube = build_cube();

    struct ObjInst { glm::vec3 pos; float scale; glm::vec3 albedo; float spec; };
    std::vector<ObjInst> objects = {
        {{ 0.0f, 0.0f,  0.0f}, 1.0f, {0.8f,0.4f,0.2f}, 0.6f},
        {{ 2.5f, 0.0f, -1.0f}, 1.2f, {0.2f,0.6f,0.9f}, 0.8f},
        {{-2.0f, 1.0f,  2.0f}, 1.5f, {0.5f,0.8f,0.3f}, 0.3f},
        {{ 1.0f, 0.3f,  3.0f}, 0.8f, {0.9f,0.7f,0.1f}, 0.5f},
        {{-3.5f, 0.0f, -2.0f}, 1.0f, {0.6f,0.3f,0.9f}, 0.7f},
        {{ 0.0f,-0.5f,  0.0f}, 0.2f, {0.7f,0.7f,0.7f}, 0.1f}, // floor slab (thin)
    };
    // Floor (thin wide cube)
    objects.back().pos   = {0,-0.75f,0};
    objects.back().scale = 0.1f; // height

    // Make a flat floor plane from a scaled cube
    objects.push_back({{0,-0.75f,0}, 1.0f, {0.6f,0.6f,0.6f}, 0.1f});
    {
        auto& fl = objects.back();
        fl.pos   = {0, -1.0f, 0};
        fl.albedo= {0.6f,0.6f,0.6f};
        fl.spec  = 0.1f;
    }

    // ── 32 point lights ────────────────────────────────────────────────────────
    struct PLight { glm::vec3 pos; glm::vec3 col; float lin; float quad; };
    std::mt19937 rng(99);
    std::uniform_real_distribution<float> pdist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> cdist(0.5f, 1.0f);
    std::vector<PLight> lights(32);
    std::vector<float>  light_speeds(32);
    std::uniform_real_distribution<float> spd(0.3f, 1.2f);
    for (auto& l : lights) {
        l.pos  = {pdist(rng), pdist(rng)*0.5f+1.0f, pdist(rng)};
        l.col  = {cdist(rng)*5.0f, cdist(rng)*5.0f, cdist(rng)*5.0f};
        l.lin  = 0.7f;
        l.quad = 1.8f;
    }
    for (auto& s : light_speeds) s = spd(rng);

    glm::vec3 cam_pos{0,5,10};
    glm::mat4 proj = glm::perspective(glm::radians(45.0f),(float)SCR_W/SCR_H,0.1f,100.0f);
    glm::mat4 view = glm::lookAt(cam_pos, glm::vec3(0,0,0), glm::vec3(0,1,0));

    printf("Keys: 1=Shading 2=Position 3=Normal 4=Albedo 5=SSAO  R=toggle light rotation\n");

    float t = 0;
    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float dt  = now - t; t = now;

        if (g_rotate) {
            for (int i = 0; i < 32; ++i) {
                float a = t * light_speeds[i] + i * 0.196f;
                lights[i].pos.x = cosf(a) * 4.5f;
                lights[i].pos.z = sinf(a) * 4.5f;
            }
        }

        // ── Pass 1: Geometry pass ─────────────────────────────────────────────
        gbuf.bind_geometry_pass();
        glViewport(0,0,SCR_W,SCR_H);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glUseProgram(prog_geo);
        glUniformMatrix4fv(glGetUniformLocation(prog_geo,"uView"),       1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog_geo,"uProjection"), 1,GL_FALSE,glm::value_ptr(proj));

        for (auto& o : objects) {
            glm::mat4 model = glm::scale(glm::translate(glm::mat4(1),o.pos),glm::vec3(o.scale));
            glUniformMatrix4fv(glGetUniformLocation(prog_geo,"uModel"), 1,GL_FALSE,glm::value_ptr(model));
            glUniform3fv(glGetUniformLocation(prog_geo,"uAlbedo"),  1, glm::value_ptr(o.albedo));
            glUniform1f (glGetUniformLocation(prog_geo,"uSpecular"),    o.spec);
            cube.draw();
        }

        // ── Pass 2: SSAO ──────────────────────────────────────────────────────
        glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo);
        glViewport(0,0,SCR_W,SCR_H);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(prog_ssao);
        // Bind G-Buffer textures
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuf.pos_tex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuf.normal_tex);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, ssao.noise_tex);
        glUniform1i(glGetUniformLocation(prog_ssao,"uGPosition"),0);
        glUniform1i(glGetUniformLocation(prog_ssao,"uGNormal"),  1);
        glUniform1i(glGetUniformLocation(prog_ssao,"uNoiseTex"), 2);
        glUniformMatrix4fv(glGetUniformLocation(prog_ssao,"uProjection"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(prog_ssao,"uView"),      1,GL_FALSE,glm::value_ptr(view));
        glUniform2f(glGetUniformLocation(prog_ssao,"uScreenSize"),(float)SCR_W,(float)SCR_H);
        for (int i = 0; i < 64; ++i) {
            char buf[64]; snprintf(buf,64,"uSamples[%d]",i);
            glUniform3fv(glGetUniformLocation(prog_ssao,buf),1,glm::value_ptr(ssao.kernel[i]));
        }
        draw_fullscreen();

        // ── Pass 3: SSAO blur ─────────────────────────────────────────────────
        glBindFramebuffer(GL_FRAMEBUFFER, ssao.blur_fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog_ssao_blur);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssao.tex);
        glUniform1i(glGetUniformLocation(prog_ssao_blur,"uSSAOInput"),0);
        draw_fullscreen();

        // ── Pass 4: Lighting pass ─────────────────────────────────────────────
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0,0,SCR_W,SCR_H);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog_lighting);
        gbuf.bind_textures();  // units 0/1/2
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, ssao.blur_tex);
        glUniform1i(glGetUniformLocation(prog_lighting,"uGPosition"),   0);
        glUniform1i(glGetUniformLocation(prog_lighting,"uGNormal"),     1);
        glUniform1i(glGetUniformLocation(prog_lighting,"uGAlbedoSpec"), 2);
        glUniform1i(glGetUniformLocation(prog_lighting,"uSSAO"),        3);
        glUniform3fv(glGetUniformLocation(prog_lighting,"uViewPos"),1,glm::value_ptr(cam_pos));
        glUniform1i(glGetUniformLocation(prog_lighting,"uNumLights"),32);
        glUniform1i(glGetUniformLocation(prog_lighting,"uVisMode"),g_vis_mode);
        for (int i = 0; i < 32; ++i) {
            char buf[80];
            snprintf(buf,80,"uLights[%d].position",i);
            glUniform3fv(glGetUniformLocation(prog_lighting,buf),1,glm::value_ptr(lights[i].pos));
            snprintf(buf,80,"uLights[%d].color",i);
            glUniform3fv(glGetUniformLocation(prog_lighting,buf),1,glm::value_ptr(lights[i].col));
            snprintf(buf,80,"uLights[%d].linear",i);
            glUniform1f(glGetUniformLocation(prog_lighting,buf), lights[i].lin);
            snprintf(buf,80,"uLights[%d].quadratic",i);
            glUniform1f(glGetUniformLocation(prog_lighting,buf), lights[i].quad);
        }
        draw_fullscreen();

        glEnable(GL_DEPTH_TEST);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    gbuf.destroy();
    ssao.destroy();
    glfwTerminate();
    return 0;
}
