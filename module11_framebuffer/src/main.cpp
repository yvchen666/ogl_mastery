// module11_framebuffer/src/main.cpp
// HDR framebuffer + post-processing demo
// Keys: 1=No post (passthrough TM)  2=Reinhard  3=ACES  4=Bloom  5=Sobel  6=DoF
//       +/- adjust exposure         R=rotate lights

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "framebuffer.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ─── State ───────────────────────────────────────────────────────────────────
static int   SCR_W = 1280, SCR_H = 720;
static int   g_mode    = 1;     // 1-6
static float g_exposure = 1.0f;
static bool  g_rotate  = true;

static void fb_size_cb(GLFWwindow*, int w, int h) {
    SCR_W = w; SCR_H = h;
    glViewport(0, 0, w, h);
}
static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, true);
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6) {
            g_mode = key - GLFW_KEY_0;
            const char* names[] = {"","Passthrough","Reinhard","ACES","Bloom","Sobel","DoF"};
            printf("Mode: %s\n", names[g_mode]);
        }
        if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD)    g_exposure *= 1.2f;
        if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) g_exposure /= 1.2f;
        if (key == GLFW_KEY_R) g_rotate = !g_rotate;
    }
}

// ─── Shader utils ────────────────────────────────────────────────────────────
static std::string read_file(const char* p) {
    std::ifstream f(p); if (!f) throw std::runtime_error(std::string("open: ")+p);
    std::ostringstream s; s << f.rdbuf(); return s.str();
}
static GLuint compile(GLenum t, const char* src) {
    GLuint s = glCreateShader(t); glShaderSource(s,1,&src,nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char l[512]; glGetShaderInfoLog(s,512,nullptr,l); throw std::runtime_error(l); }
    return s;
}
static GLuint make_prog(const char* vp, const char* fp) {
    auto vs = read_file(vp); auto fs = read_file(fp);
    GLuint v = compile(GL_VERTEX_SHADER, vs.c_str());
    GLuint f = compile(GL_FRAGMENT_SHADER, fs.c_str());
    GLuint p = glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char l[512]; glGetProgramInfoLog(p,512,nullptr,l); throw std::runtime_error(l); }
    glDeleteShader(v); glDeleteShader(f); return p;
}

// ─── Geometry ─────────────────────────────────────────────────────────────────
struct Mesh { GLuint vao{0},vbo{0},ebo{0}; GLsizei nc{0}; void draw() const { glBindVertexArray(vao); glDrawElements(GL_TRIANGLES,nc,GL_UNSIGNED_INT,nullptr); } };

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

static void draw_fullscreen_quad() {
    // Uses the fullscreen.vert vertex-ID trick — no VAO needed
    static GLuint dummy_vao = 0;
    if (!dummy_vao) glGenVertexArrays(1, &dummy_vao);
    glBindVertexArray(dummy_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(SCR_W, SCR_H, "module11 HDR Framebuffer + Post-Process", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, fb_size_cb);
    glfwSetKeyCallback(win, key_cb);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    // ── Programs ──────────────────────────────────────────────────────────────
    GLuint prog_scene    = make_prog("shaders/hdr_scene.vert",   "shaders/hdr_scene.frag");
    GLuint prog_tonemap  = make_prog("shaders/fullscreen.vert",  "shaders/tonemap.frag");
    GLuint prog_bright   = make_prog("shaders/fullscreen.vert",  "shaders/bloom_bright.frag");
    GLuint prog_blur_h   = make_prog("shaders/fullscreen.vert",  "shaders/blur_horizontal.frag");
    GLuint prog_blur_v   = make_prog("shaders/fullscreen.vert",  "shaders/blur_vertical.frag");
    GLuint prog_bloom    = make_prog("shaders/fullscreen.vert",  "shaders/bloom_composite.frag");
    GLuint prog_post     = make_prog("shaders/fullscreen.vert",  "shaders/postprocess.frag");

    // ── HDR FBO (GL_RGBA16F) ──────────────────────────────────────────────────
    Framebuffer hdr_fbo;
    hdr_fbo.create(SCR_W, SCR_H,
                   {{ 0, GL_RGBA16F, GL_COLOR_ATTACHMENT0 }}, true);

    // Ping-pong FBOs for Gaussian blur
    Framebuffer ping_fbo, pong_fbo;
    ping_fbo.create(SCR_W, SCR_H, {{ 0, GL_RGBA16F, GL_COLOR_ATTACHMENT0 }}, false);
    pong_fbo.create(SCR_W, SCR_H, {{ 0, GL_RGBA16F, GL_COLOR_ATTACHMENT0 }}, false);

    // ── Geometry ──────────────────────────────────────────────────────────────
    Mesh cube = build_cube();

    struct CubeInst { glm::vec3 pos; float s; glm::vec3 col; };
    CubeInst objs[] = {
        {{ 0,0, 0}, 1.0f, {0.8f, 0.4f, 0.2f}},
        {{ 2,0,-1}, 1.2f, {0.3f, 0.6f, 0.9f}},
        {{-2,0, 1}, 0.8f, {0.5f, 0.9f, 0.3f}},
        {{ 0,0, 3}, 1.5f, {0.9f, 0.7f, 0.1f}},
    };

    // HDR point lights (intensities > 1 intentionally)
    struct Light { glm::vec3 pos; glm::vec3 col; };
    Light lights[] = {
        {{ 0.0f, 3.0f,  0.0f}, {5.0f, 5.0f,  5.0f}},
        {{ 2.0f, 1.5f,  2.0f}, {0.0f, 0.0f, 15.0f}},
        {{-2.0f, 1.5f, -2.0f}, {15.0f,0.0f,  0.0f}},
        {{ 3.0f, 1.0f,  3.0f}, {0.0f,15.0f,  0.0f}},
    };

    printf("Keys: 1=Passthrough 2=Reinhard 3=ACES 4=Bloom 5=Sobel 6=DoF  +/-=exposure  R=rotate\n");

    float t = 0;
    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float dt  = now - t; t = now;
        if (g_rotate)
            for (int i = 0; i < 4; ++i) {
                float angle = t * 0.5f + i * 1.5708f;
                lights[i].pos.x = cosf(angle) * 3.0f;
                lights[i].pos.z = sinf(angle) * 3.0f;
            }

        glm::mat4 view = glm::lookAt(glm::vec3(4,5,8), glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),(float)SCR_W/SCR_H,0.1f,100.0f);

        // ── Pass 1: render scene to HDR FBO ──────────────────────────────────
        hdr_fbo.bind();
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog_scene);
        glUniformMatrix4fv(glGetUniformLocation(prog_scene,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog_scene,"uProjection"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(prog_scene,"uViewPos"),4,5,8);
        for (int i = 0; i < 4; ++i) {
            char buf[64];
            snprintf(buf,64,"uLights[%d].position",i);
            glUniform3fv(glGetUniformLocation(prog_scene,buf),1,glm::value_ptr(lights[i].pos));
            snprintf(buf,64,"uLights[%d].color",i);
            glUniform3fv(glGetUniformLocation(prog_scene,buf),1,glm::value_ptr(lights[i].col));
        }
        for (auto& o : objs) {
            glm::mat4 model = glm::scale(glm::translate(glm::mat4(1),o.pos),glm::vec3(o.s));
            glUniformMatrix4fv(glGetUniformLocation(prog_scene,"uModel"),1,GL_FALSE,glm::value_ptr(model));
            glUniform3fv(glGetUniformLocation(prog_scene,"uObjectColor"),1,glm::value_ptr(o.col));
            cube.draw();
        }

        // ── Pass 2: post-process ──────────────────────────────────────────────
        Framebuffer::unbind();
        glViewport(0,0,SCR_W,SCR_H);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        if (g_mode == 4) {
            // ── Bloom pipeline ────────────────────────────────────────────────
            // 2a: extract bright regions
            ping_fbo.bind();
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(prog_bright);
            glUniform1i(glGetUniformLocation(prog_bright,"uHdrBuffer"),0);
            glUniform1f(glGetUniformLocation(prog_bright,"uThreshold"),1.0f);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdr_fbo.color_tex(0));
            draw_fullscreen_quad();

            // 2b: 5 iterations of horizontal+vertical blur
            bool horiz = true;
            for (int i = 0; i < 10; ++i) {
                (horiz ? pong_fbo : ping_fbo).bind();
                glClear(GL_COLOR_BUFFER_BIT);
                glUseProgram(horiz ? prog_blur_h : prog_blur_v);
                glUniform1i(glGetUniformLocation(horiz ? prog_blur_h : prog_blur_v,"uImage"),0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (horiz ? ping_fbo : pong_fbo).color_tex(0));
                draw_fullscreen_quad();
                horiz = !horiz;
            }
            // 2c: composite
            Framebuffer::unbind(); glViewport(0,0,SCR_W,SCR_H);
            glUseProgram(prog_bloom);
            glUniform1i(glGetUniformLocation(prog_bloom,"uHdrBuffer"),0);
            glUniform1i(glGetUniformLocation(prog_bloom,"uBloomBlur"),1);
            glUniform1f(glGetUniformLocation(prog_bloom,"uBloomStrength"),1.0f);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdr_fbo.color_tex(0));
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, ping_fbo.color_tex(0));
            draw_fullscreen_quad();
        } else if (g_mode == 5) {
            // Sobel edge detection
            glUseProgram(prog_post);
            glUniform1i(glGetUniformLocation(prog_post,"uColorBuffer"),0);
            glUniform1i(glGetUniformLocation(prog_post,"uEffect"),0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdr_fbo.color_tex(0));
            draw_fullscreen_quad();
        } else if (g_mode == 6) {
            // DoF
            glUseProgram(prog_post);
            glUniform1i(glGetUniformLocation(prog_post,"uColorBuffer"),0);
            glUniform1i(glGetUniformLocation(prog_post,"uEffect"),1);
            glUniform1f(glGetUniformLocation(prog_post,"uFocusDepth"),0.5f);
            glUniform1f(glGetUniformLocation(prog_post,"uDofStrength"),2.0f);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdr_fbo.color_tex(0));
            draw_fullscreen_quad();
        } else {
            // Tone mapping modes 1-3
            int tm_mode = g_mode - 1; // 0=passthrough,1=Reinhard,2=Exposure→map to 0/1/2
            // actual uniform: 0=pass,1=Reinhard,2=Exposure,3=ACES
            // mode key 1→tm0, 2→tm1(Reinhard), 3→tm3(ACES)
            int uni_mode = (g_mode == 1) ? 0 : (g_mode == 2) ? 1 : 3;
            glUseProgram(prog_tonemap);
            glUniform1i(glGetUniformLocation(prog_tonemap,"uHdrBuffer"),0);
            glUniform1i(glGetUniformLocation(prog_tonemap,"uMode"),uni_mode);
            glUniform1f(glGetUniformLocation(prog_tonemap,"uExposure"),g_exposure);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdr_fbo.color_tex(0));
            draw_fullscreen_quad();
        }

        glEnable(GL_DEPTH_TEST);
        glfwSwapBuffers(win); glfwPollEvents();
    }
    hdr_fbo.destroy(); ping_fbo.destroy(); pong_fbo.destroy();
    glfwTerminate();
}
