// module09_normal_maps/src/main.cpp
// 演示：砖墙平面 — 无法线贴图 / 法线贴图 / 视差贴图
// 按 1/2/3 切换，光源绕场景旋转

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>

#include "shader.h"
#include "mesh.h"
#include "mesh_tbn.h"
#include "tangent_calculator.h"
#include "texture.h"

// ── Window / Camera ───────────────────────────────────────────────────────────
static int SCR_W = 1280, SCR_H = 720;

struct Camera {
    glm::vec3 pos  {0,0.5f,3.0f};
    glm::vec3 front{0,0,-1};
    glm::vec3 up   {0,1,0};
    float yaw=-90.f, pitch=-10.f, speed=3.f, sens=0.1f, fov=45.f;
    glm::mat4 view() const { return glm::lookAt(pos,pos+front,up); }
    glm::mat4 proj(float a) const { return glm::perspective(glm::radians(fov),a,0.1f,100.f); }
    void update() {
        glm::vec3 f;
        f.x=std::cos(glm::radians(yaw))*std::cos(glm::radians(pitch));
        f.y=std::sin(glm::radians(pitch));
        f.z=std::sin(glm::radians(yaw))*std::cos(glm::radians(pitch));
        front=glm::normalize(f);
    }
};

static Camera g_cam;
static float  g_lx=SCR_W/2.f, g_ly=SCR_H/2.f;
static bool   g_first=true;
static float  g_dt=0, g_last_t=0;
static int    g_mode=0;  // 0=no normal, 1=normal map, 2=parallax

static void fb_cb(GLFWwindow*,int w,int h){SCR_W=w;SCR_H=h;glViewport(0,0,w,h);}
static void mouse_cb(GLFWwindow*,double x,double y){
    if(g_first){g_lx=(float)x;g_ly=(float)y;g_first=false;}
    float dx=(float)x-g_lx, dy=g_ly-(float)y;
    g_lx=(float)x;g_ly=(float)y;
    g_cam.yaw+=dx*g_cam.sens;
    g_cam.pitch=glm::clamp(g_cam.pitch+dy*g_cam.sens,-89.f,89.f);
    g_cam.update();
}
static void scroll_cb(GLFWwindow*,double,double dy){
    g_cam.fov=glm::clamp(g_cam.fov-(float)dy,1.f,90.f);
}
static void key_cb(GLFWwindow* win,int key,int,int action,int){
    if(action==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win,true);
        if(key==GLFW_KEY_1) g_mode=0;
        if(key==GLFW_KEY_2) g_mode=1;
        if(key==GLFW_KEY_3) g_mode=2;
    }
}
static void process_move(GLFWwindow* win){
    float s=g_cam.speed*g_dt;
    glm::vec3 right=glm::normalize(glm::cross(g_cam.front,g_cam.up));
    if(glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS) g_cam.pos+=s*g_cam.front;
    if(glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS) g_cam.pos-=s*g_cam.front;
    if(glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS) g_cam.pos-=s*right;
    if(glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS) g_cam.pos+=s*right;
}

// ── Procedural textures ────────────────────────────────────────────────────────
// Simple brick-like diffuse texture (CPU generated)
static GLuint make_brick_diffuse(int size=256) {
    std::vector<uint8_t> pixels(size*size*3);
    for(int y=0;y<size;y++) {
        for(int x=0;x<size;x++) {
            int brickH=32, brickW=64, mortar=3;
            int row = y/brickH;
            int offsetX = (row%2)*(brickW/2);
            int lx = (x+offsetX) % brickW;
            int ly = y % brickH;
            bool is_mortar = (lx < mortar) || (ly < mortar);
            uint8_t r,g,b;
            if(is_mortar){r=180;g=180;b=180;}
            else {
                // brick color with slight variation
                float noise = 0.85f + 0.15f*std::sin(x*0.3f)*std::cos(y*0.17f);
                r=(uint8_t)(180*noise);
                g=(uint8_t)(90*noise);
                b=(uint8_t)(60*noise);
            }
            int i=(y*size+x)*3;
            pixels[i+0]=r; pixels[i+1]=g; pixels[i+2]=b;
        }
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

// Normal map: mostly blue (0.5,0.5,1.0) with bump at brick edges
static GLuint make_brick_normal(int size=256) {
    std::vector<uint8_t> pixels(size*size*3);
    for(int y=0;y<size;y++) {
        for(int x=0;x<size;x++) {
            int brickH=32, brickW=64, mortar=3;
            int row = y/brickH;
            int offsetX = (row%2)*(brickW/2);
            int lx = (x+offsetX) % brickW;
            int ly = y % brickH;
            bool near_h = (lx < mortar || lx > brickW-mortar);
            bool near_v = (ly < mortar || ly > brickH-mortar);

            float nx=0,ny=0,nz=1;
            if(near_h) { nx = (lx<mortar) ? -0.5f : 0.5f; nz=0.8f; }
            if(near_v) { ny = (ly<mortar) ? -0.5f : 0.5f; nz=0.8f; }
            float len=std::sqrt(nx*nx+ny*ny+nz*nz);
            nx/=len; ny/=len; nz/=len;
            // encode to [0,1]
            int i=(y*size+x)*3;
            pixels[i+0]=(uint8_t)((nx*0.5f+0.5f)*255);
            pixels[i+1]=(uint8_t)((ny*0.5f+0.5f)*255);
            pixels[i+2]=(uint8_t)((nz*0.5f+0.5f)*255);
        }
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

// Depth/height map (for parallax): bright = high, dark = low
static GLuint make_brick_height(int size=256) {
    std::vector<uint8_t> pixels(size*size);
    for(int y=0;y<size;y++) {
        for(int x=0;x<size;x++) {
            int brickH=32, brickW=64, mortar=3;
            int row=y/brickH;
            int offsetX=(row%2)*(brickW/2);
            int lx=(x+offsetX)%brickW;
            int ly=y%brickH;
            bool is_mortar=(lx<mortar)||(ly<mortar);
            pixels[y*size+x] = is_mortar ? 30 : 200;
        }
    }
    GLuint id;
    glGenTextures(1,&id);
    glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,size,size,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,0);
    return id;
}

// ── Build a plane mesh with TBN data ─────────────────────────────────────────
static MeshTBN make_plane_tbn(float size=2.0f, float uv_scale=2.0f) {
    float h = size*0.5f;
    // 4 vertices of a flat quad in XY plane facing +Z
    std::vector<Vertex> base_verts = {
        {{-h,-h,0},{0,0,1},{0*uv_scale,        0*uv_scale}},
        {{ h,-h,0},{0,0,1},{1.0f*uv_scale,      0*uv_scale}},
        {{ h, h,0},{0,0,1},{1.0f*uv_scale,  1.0f*uv_scale}},
        {{-h, h,0},{0,0,1},{0*uv_scale,     1.0f*uv_scale}},
    };
    std::vector<uint32_t> indices = {0,1,2, 2,3,0};

    TangentData td = calc_tangents(base_verts, indices);

    std::vector<VertexTBN> verts;
    for(size_t i=0;i<base_verts.size();i++){
        VertexTBN v;
        v.position  = base_verts[i].position;
        v.normal    = base_verts[i].normal;
        v.uv        = base_verts[i].uv;
        v.tangent   = td.tangents[i];
        v.bitangent = td.bitangents[i];
        verts.push_back(v);
    }
    MeshTBN m; m.upload(verts, indices); return m;
}

// ── Light cube (simple) ────────────────────────────────────────────────────────
static GLuint g_light_vao=0, g_light_vbo=0;
static void init_light_cube(){
    float v[]={
        -0.1f,-0.1f,-0.1f, 0.1f,-0.1f,-0.1f, 0.1f,0.1f,-0.1f,
         0.1f, 0.1f,-0.1f,-0.1f, 0.1f,-0.1f,-0.1f,-0.1f,-0.1f,
        -0.1f,-0.1f, 0.1f, 0.1f,-0.1f, 0.1f, 0.1f,0.1f, 0.1f,
         0.1f, 0.1f, 0.1f,-0.1f, 0.1f, 0.1f,-0.1f,-0.1f, 0.1f,
        -0.1f, 0.1f, 0.1f,-0.1f, 0.1f,-0.1f,-0.1f,-0.1f,-0.1f,
        -0.1f,-0.1f,-0.1f,-0.1f,-0.1f, 0.1f,-0.1f, 0.1f, 0.1f,
         0.1f, 0.1f, 0.1f, 0.1f, 0.1f,-0.1f, 0.1f,-0.1f,-0.1f,
         0.1f,-0.1f,-0.1f, 0.1f,-0.1f, 0.1f, 0.1f, 0.1f, 0.1f,
        -0.1f,-0.1f,-0.1f, 0.1f,-0.1f,-0.1f, 0.1f,-0.1f, 0.1f,
         0.1f,-0.1f, 0.1f,-0.1f,-0.1f, 0.1f,-0.1f,-0.1f,-0.1f,
        -0.1f, 0.1f,-0.1f, 0.1f, 0.1f,-0.1f, 0.1f, 0.1f, 0.1f,
         0.1f, 0.1f, 0.1f,-0.1f, 0.1f, 0.1f,-0.1f, 0.1f,-0.1f,
    };
    glGenVertexArrays(1,&g_light_vao);
    glGenBuffers(1,&g_light_vbo);
    glBindVertexArray(g_light_vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_light_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,6);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(SCR_W,SCR_H,
        "Module 09 - Normal Maps | 1=Flat 2=NormalMap 3=Parallax | WASD+Mouse",
        nullptr,nullptr);
    if(!win){std::cerr<<"glfwCreateWindow failed\n";return 1;}
    glfwMakeContextCurrent(win);
    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    glfwSetFramebufferSizeCallback(win,fb_cb);
    glfwSetCursorPosCallback(win,mouse_cb);
    glfwSetScrollCallback(win,scroll_cb);
    glfwSetKeyCallback(win,key_cb);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"GLAD init failed\n";return 1;
    }
    glEnable(GL_DEPTH_TEST);

    Shader nm_sh("shaders/normal_map.vert", "shaders/normal_map.frag");

    // light cube shader (reuse a minimal vert/frag)
    // We'll write a simple inline shader for the light indicator
    // (no separate .vert/.frag file needed – embed in code)
    const char* lv_src = R"(
#version 460 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)";
    const char* lf_src = R"(
#version 460 core
out vec4 FragColor;
void main(){ FragColor = vec4(1.0,1.0,0.5,1.0); }
)";
    // Compile inline
    auto compile_inline = [](const char* vsrc, const char* fsrc) -> GLuint {
        auto compile_shader = [](GLenum type, const char* src) {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh,1,&src,nullptr);
            glCompileShader(sh);
            return sh;
        };
        GLuint v=compile_shader(GL_VERTEX_SHADER,vsrc);
        GLuint f=compile_shader(GL_FRAGMENT_SHADER,fsrc);
        GLuint p=glCreateProgram();
        glAttachShader(p,v); glAttachShader(p,f);
        glLinkProgram(p);
        glDeleteShader(v); glDeleteShader(f);
        return p;
    };
    GLuint light_prog = compile_inline(lv_src, lf_src);

    // Textures
    GLuint tex_diff   = make_brick_diffuse();
    GLuint tex_normal = make_brick_normal();
    GLuint tex_height = make_brick_height();

    // Try loading from assets/ first
    {
        Texture2D t = Texture2D::from_file("assets/brick_diffuse.png");
        if(t.id){ glDeleteTextures(1,&tex_diff); tex_diff=t.id; }
    }
    {
        Texture2D t = Texture2D::from_file("assets/brick_normal.png");
        if(t.id){ glDeleteTextures(1,&tex_normal); tex_normal=t.id; }
    }
    {
        Texture2D t = Texture2D::from_file("assets/brick_height.png");
        if(t.id){ glDeleteTextures(1,&tex_height); tex_height=t.id; }
    }

    MeshTBN plane = make_plane_tbn(3.0f, 3.0f);
    init_light_cube();

    g_cam.update();

    // Bind texture units once
    nm_sh.use();
    nm_sh.set_int("uDiffuseMap", 0);
    nm_sh.set_int("uNormalMap",  1);
    nm_sh.set_int("uDepthMap",   2);
    nm_sh.set_float("uHeightScale", 0.05f);

    while(!glfwWindowShouldClose(win)){
        float cur=(float)glfwGetTime();
        g_dt=cur-g_last_t; g_last_t=cur;
        process_move(win);

        glClearColor(0.08f,0.08f,0.1f,1.f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        float aspect=(float)SCR_W/(float)SCR_H;
        glm::mat4 view = g_cam.view();
        glm::mat4 proj = g_cam.proj(aspect);

        // Orbiting light
        float lrad=2.0f;
        glm::vec3 lpos{
            lrad*std::cos(cur*0.7f),
            1.2f + 0.3f*std::sin(cur*0.4f),
            lrad*std::sin(cur*0.7f)
        };

        // ── Draw plane ──
        glm::mat4 model(1.0f);
        // rotate plane to face camera front-on
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1,0,0));
        glm::mat3 nm = glm::mat3(glm::transpose(glm::inverse(model)));

        nm_sh.use();
        nm_sh.set_mat4("uModel",        model);
        nm_sh.set_mat4("uView",         view);
        nm_sh.set_mat4("uProjection",   proj);
        nm_sh.set_mat3("uNormalMatrix", nm);
        nm_sh.set_vec3("uLightPos",     lpos);
        nm_sh.set_vec3("uViewPos",      g_cam.pos);
        nm_sh.set_bool("uUseNormalMap", g_mode >= 1);
        nm_sh.set_bool("uUseParallax",  g_mode >= 2);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex_diff);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, tex_normal);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, tex_height);
        plane.draw();

        // ── Draw light marker ──
        glm::mat4 mvp = proj * view * glm::translate(glm::mat4(1.f), lpos);
        glUseProgram(light_prog);
        glUniformMatrix4fv(glGetUniformLocation(light_prog,"uMVP"),
                           1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(g_light_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    plane.destroy();
    glDeleteTextures(1,&tex_diff);
    glDeleteTextures(1,&tex_normal);
    glDeleteTextures(1,&tex_height);
    glDeleteVertexArrays(1,&g_light_vao);
    glDeleteBuffers(1,&g_light_vbo);
    nm_sh.destroy();
    glDeleteProgram(light_prog);
    glfwTerminate();
    return 0;
}
