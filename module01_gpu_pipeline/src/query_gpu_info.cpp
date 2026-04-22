// query_gpu_info.cpp
// 查询并打印 OpenGL/GPU 基本信息
// 创建一个最小 GLFW 窗口（不显示），查询 GL 字符串
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(1,1,"",nullptr,nullptr);
    if (!w) {
        std::cerr << "GLFW: failed to create window (no display or GL 4.6 not supported)\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(w);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD: failed to load OpenGL\n";
        glfwDestroyWindow(w);
        glfwTerminate();
        return 1;
    }

    std::cout << "Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "Version:  " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL:     " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

    // 打印关键硬件限制
    GLint val;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val);
    std::cout << "Max Texture Size: " << val << "\n";
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &val);
    std::cout << "Max Vertex Attribs: " << val << "\n";
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &val);
    std::cout << "Max UBO Size: " << val << " bytes\n";
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &val);
    std::cout << "Max SSBO Size: " << val << " bytes\n";
    // Compute Shader 工作组
    GLint wg[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &wg[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &wg[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &wg[2]);
    std::cout << "Max Compute WorkGroup: " << wg[0] << "x" << wg[1] << "x" << wg[2] << "\n";

    glfwDestroyWindow(w);
    glfwTerminate();
}
