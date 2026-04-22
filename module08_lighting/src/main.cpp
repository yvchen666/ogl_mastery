// module08_lighting/src/main.cpp
// Blinn-Phong 光照场景：方向光/点光源/聚光灯，多几何体，light cube

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#include "shader.h"
#include "mesh.h"

// ── Camera ────────────────────────────────────────────────────────────────────
static int SCR_W = 1280, SCR_H = 720;

struct Camera {
    glm::vec3 pos   {0.0f, 2.0f, 7.0f};
    glm::vec3 front {0.0f, 0.0f,-1.0f};
    glm::vec3 up    {0.0f, 1.0f, 0.0f};
    float yaw   = -90.0f;
    float pitch =  -15.0f;
    float speed =   3.0f;
    float sensitivity = 0.1f;
    float fov   =  45.0f;

    glm::mat4 view() const { return glm::lookAt(pos, pos+front, up); }
    glm::mat4 projection(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
    }
    void update_front() {
        glm::vec3 f;
        f.x = std::cos(glm::radians(yaw))*std::cos(glm::radians(pitch));
        f.y = std::sin(glm::radians(pitch));
        f.z = std::sin(glm::radians(yaw))*std::cos(glm::radians(pitch));
        front = glm::normalize(f);
    }
};

static Camera g_cam;
static float g_last_x = SCR_W/2.0f, g_last_y = SCR_H/2.0f;
static bool  g_first_mouse = true;
static float g_dt = 0.0f, g_last_frame = 0.0f;
static int   g_light_mode = 3; // 0=dir, 1=point, 2=spot, 3=all
static bool  g_use_spot   = true;

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    SCR_W=w; SCR_H=h; glViewport(0,0,w,h);
}
static void mouse_cb(GLFWwindow*, double xpos, double ypos) {
    if (g_first_mouse) {
        g_last_x=(float)xpos; g_last_y=(float)ypos; g_first_mouse=false;
    }
    float dx = (float)xpos - g_last_x;
    float dy = g_last_y - (float)ypos;
    g_last_x=(float)xpos; g_last_y=(float)ypos;
    g_cam.yaw   += dx * g_cam.sensitivity;
    g_cam.pitch += dy * g_cam.sensitivity;
    g_cam.pitch = glm::clamp(g_cam.pitch, -89.0f, 89.0f);
    g_cam.update_front();
}
static void scroll_cb(GLFWwindow*, double, double dy) {
    g_cam.fov = glm::clamp(g_cam.fov - (float)dy, 1.0f, 90.0f);
}
static void key_cb(GLFWwindow* win, int key, int, int action, int) {
    if (action==GLFW_PRESS) {
        if (key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win,true);
        if (key==GLFW_KEY_1) g_light_mode=0;
        if (key==GLFW_KEY_2) g_light_mode=1;
        if (key==GLFW_KEY_3) g_light_mode=2;
        if (key==GLFW_KEY_4) g_light_mode=3;
        if (key==GLFW_KEY_SPACE) g_use_spot=!g_use_spot;
    }
}
static void process_movement(GLFWwindow* win) {
    float spd = g_cam.speed * g_dt;
    glm::vec3 right = glm::normalize(glm::cross(g_cam.front, g_cam.up));
    if (glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS) g_cam.pos += spd * g_cam.front;
    if (glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS) g_cam.pos -= spd * g_cam.front;
    if (glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS) g_cam.pos -= spd * right;
    if (glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS) g_cam.pos += spd * right;
}

// ── Upload Phong material uniforms ────────────────────────────────────────────
static void set_material(Shader& s,
                          glm::vec3 amb, glm::vec3 diff, glm::vec3 spec,
                          float shin) {
    s.set_vec3 ("uMatAmbient",   amb);
    s.set_vec3 ("uMatDiffuse",   diff);
    s.set_vec3 ("uMatSpecular",  spec);
    s.set_float("uMatShininess", shin);
}

// ── Set model + normal matrix ─────────────────────────────────────────────────
static void set_model(Shader& s, const glm::mat4& model) {
    s.set_mat4("uModel", model);
    glm::mat3 nm = glm::mat3(glm::transpose(glm::inverse(model)));
    s.set_mat3("uNormalMatrix", nm);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,6);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(SCR_W,SCR_H,
        "Module 08 - Lighting | 1=DirLight 2=PointLights 3=SpotLight 4=All | WASD+Mouse",
        nullptr,nullptr);
    if(!win){std::cerr<<"glfwCreateWindow failed\n";return 1;}
    glfwMakeContextCurrent(win);
    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    glfwSetFramebufferSizeCallback(win,framebuffer_size_cb);
    glfwSetCursorPosCallback(win,mouse_cb);
    glfwSetScrollCallback(win,scroll_cb);
    glfwSetKeyCallback(win,key_cb);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"GLAD init failed\n";return 1;
    }
    glEnable(GL_DEPTH_TEST);

    Shader phong_sh ("shaders/phong.vert",      "shaders/phong.frag");
    Shader light_sh ("shaders/light_cube.vert", "shaders/light_cube.frag");

    // ── Meshes ──
    Mesh cube   = Mesh::make_cube(1.0f);
    Mesh sphere = Mesh::make_sphere(0.5f, 32, 32);
    Mesh plane  = Mesh::make_quad(10.0f);
    Mesh lcube  = Mesh::make_cube(0.2f); // light marker

    // ── Scene object positions ──
    std::vector<glm::vec3> cube_positions = {
        {-2,0.5f,-2}, {0,0.5f,-2}, {2,0.5f,-2},
        {-1,0.5f, 0}, {1,0.5f, 0},
    };
    std::vector<glm::vec3> sphere_positions = {
        {-2,0.5f,1}, {0,1.0f,1}, {2,0.5f,1},
    };

    // ── Point light positions ──
    glm::vec3 point_light_pos[4] = {
        { 3.0f, 2.0f, 3.0f},
        {-3.0f, 2.0f, 3.0f},
        { 3.0f, 2.0f,-3.0f},
        {-3.0f, 2.0f,-3.0f},
    };
    glm::vec3 point_light_colors[4] = {
        {1.0f,0.8f,0.6f},
        {0.5f,0.8f,1.0f},
        {0.8f,1.0f,0.5f},
        {1.0f,0.5f,0.8f},
    };

    g_cam.update_front();

    while(!glfwWindowShouldClose(win)) {
        float cur = (float)glfwGetTime();
        g_dt = cur - g_last_frame;
        g_last_frame = cur;

        process_movement(win);

        glClearColor(0.05f,0.05f,0.08f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        float aspect = (float)SCR_W/(float)SCR_H;
        glm::mat4 view = g_cam.view();
        glm::mat4 proj = g_cam.projection(aspect);

        // ── Upload common light uniforms ──
        phong_sh.use();
        phong_sh.set_mat4("uView",       view);
        phong_sh.set_mat4("uProjection", proj);
        phong_sh.set_vec3("uViewPos",    g_cam.pos);
        phong_sh.set_int ("uLightMode",  g_light_mode);
        phong_sh.set_bool("uUseSpotLight", g_use_spot);

        // Directional light
        phong_sh.set_vec3("uDirLight.direction", glm::normalize(glm::vec3(-0.5f,-1.0f,-0.3f)));
        phong_sh.set_vec3("uDirLight.ambient",   glm::vec3(0.1f));
        phong_sh.set_vec3("uDirLight.diffuse",   glm::vec3(0.6f,0.6f,0.7f));
        phong_sh.set_vec3("uDirLight.specular",  glm::vec3(0.5f));

        // Point lights (orbit slowly)
        phong_sh.set_int("uNumPointLights", 4);
        for(int i=0;i<4;i++){
            float angle = cur * 0.5f + i * 3.14159f * 0.5f;
            glm::vec3 pos = point_light_pos[i];
            pos.x += std::cos(angle)*0.5f;
            pos.z += std::sin(angle)*0.5f;
            std::string base = "uPointLights["+std::to_string(i)+"].";
            phong_sh.set_vec3 ((base+"position").c_str(),  pos);
            phong_sh.set_float((base+"constant").c_str(),  1.0f);
            phong_sh.set_float((base+"linear").c_str(),    0.09f);
            phong_sh.set_float((base+"quadratic").c_str(), 0.032f);
            phong_sh.set_vec3 ((base+"ambient").c_str(),   point_light_colors[i]*0.1f);
            phong_sh.set_vec3 ((base+"diffuse").c_str(),   point_light_colors[i]);
            phong_sh.set_vec3 ((base+"specular").c_str(),  glm::vec3(1.0f));
            // Update orbit pos for light cube rendering
            point_light_pos[i].x += 0.0f; // keep original
        }

        // Spot light (follows camera)
        phong_sh.set_vec3 ("uSpotLight.position",    g_cam.pos);
        phong_sh.set_vec3 ("uSpotLight.direction",   g_cam.front);
        phong_sh.set_float("uSpotLight.cutOff",      std::cos(glm::radians(12.5f)));
        phong_sh.set_float("uSpotLight.outerCutOff", std::cos(glm::radians(17.5f)));
        phong_sh.set_float("uSpotLight.constant",    1.0f);
        phong_sh.set_float("uSpotLight.linear",      0.09f);
        phong_sh.set_float("uSpotLight.quadratic",   0.032f);
        phong_sh.set_vec3 ("uSpotLight.ambient",     glm::vec3(0.05f));
        phong_sh.set_vec3 ("uSpotLight.diffuse",     glm::vec3(1.0f,0.9f,0.8f));
        phong_sh.set_vec3 ("uSpotLight.specular",    glm::vec3(1.0f));

        // ── Draw floor plane ──
        set_material(phong_sh,
            glm::vec3(0.1f,0.1f,0.12f),
            glm::vec3(0.4f,0.4f,0.45f),
            glm::vec3(0.2f), 16.0f);
        {
            glm::mat4 m = glm::mat4(1.0f);
            m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1,0,0));
            set_model(phong_sh, m);
            plane.draw();
        }

        // ── Draw cubes ──
        for(auto& p : cube_positions) {
            set_material(phong_sh,
                glm::vec3(0.05f,0.05f,0.1f),
                glm::vec3(0.2f,0.4f,0.8f),
                glm::vec3(0.6f), 64.0f);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), p);
            m = glm::rotate(m, cur*0.3f, glm::vec3(0,1,0));
            set_model(phong_sh, m);
            cube.draw();
        }

        // ── Draw spheres ──
        for(auto& p : sphere_positions) {
            set_material(phong_sh,
                glm::vec3(0.08f,0.02f,0.02f),
                glm::vec3(0.8f,0.2f,0.2f),
                glm::vec3(1.0f), 128.0f);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), p);
            set_model(phong_sh, m);
            sphere.draw();
        }

        // ── Draw light cubes (unlit markers) ──
        light_sh.use();
        light_sh.set_mat4("uView",       view);
        light_sh.set_mat4("uProjection", proj);

        for(int i=0;i<4;i++){
            float angle = cur * 0.5f + i * 3.14159f * 0.5f;
            glm::vec3 pos = point_light_pos[i];
            pos.x += std::cos(angle)*0.5f;
            pos.z += std::sin(angle)*0.5f;
            glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
            light_sh.set_mat4("uModel", m);
            light_sh.set_vec3("uLightColor", point_light_colors[i]);
            lcube.draw();
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    cube.destroy(); sphere.destroy(); plane.destroy(); lcube.destroy();
    phong_sh.destroy(); light_sh.destroy();
    glfwTerminate();
    return 0;
}
