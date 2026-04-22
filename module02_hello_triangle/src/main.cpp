// OpenGL 4.6 Core Profile Hello Triangle
// 演示：GLFW窗口 → GLAD加载 → 调试回调 → VAO/VBO → 着色器 → 渲染循环
//
// 编译依赖：ogl_common（包含 glad, glfw, glm, stb）
// 着色器文件：shaders/triangle.vert, shaders/triangle.frag
//
// 关键概念：
//   VAO（Vertex Array Object）  — 顶点属性配置的容器
//   VBO（Vertex Buffer Object） — GPU 端顶点数据缓冲
//   着色器程序                   — VS + FS 链接后的可执行对象

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// ── 窗口尺寸 ─────────────────────────────────────────────────────────────────
static constexpr int WIDTH  = 800;
static constexpr int HEIGHT = 600;

// ── OpenGL 调试回调 ──────────────────────────────────────────────────────────
// GL_DEBUG_OUTPUT 是 OpenGL 4.3 引入的核心特性（之前是 KHR_debug 扩展）。
// 相比 glGetError，调试回调的优势：
//   1. 可异步接收所有错误，不需要手动轮询
//   2. 提供 source/type/severity 三维分类，信息更丰富
//   3. GL_DEBUG_OUTPUT_SYNCHRONOUS 确保调用栈可追踪到出错的 API 调用
void APIENTRY gl_debug_callback(
    GLenum source,       // 错误来源：GL_DEBUG_SOURCE_API / SHADER_COMPILER / ...
    GLenum type,         // 错误类型：GL_DEBUG_TYPE_ERROR / PERFORMANCE / DEPRECATED / ...
    GLuint id,           // 错误 ID（驱动定义）
    GLenum severity,     // 严重程度：GL_DEBUG_SEVERITY_HIGH / MEDIUM / LOW / NOTIFICATION
    GLsizei /*length*/,  // msg 的字节长度（可忽略，msg 已 null-terminated）
    const GLchar* msg,   // 可读的错误描述字符串
    const void* /*userParam*/)  // 用户数据指针（通过 glDebugMessageCallback 传入）
{
    // 忽略纯通知级别的消息（非常频繁，例如缓冲区内存分配通知）
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    // 将 severity 转为可读字符串
    const char* sev_str = "UNKNOWN";
    if      (severity == GL_DEBUG_SEVERITY_HIGH)   sev_str = "HIGH";
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM)  sev_str = "MEDIUM";
    else if (severity == GL_DEBUG_SEVERITY_LOW)     sev_str = "LOW";

    std::cerr << "[GL " << sev_str << "] id=" << id << " " << msg << "\n";
}

// ── 着色器工具函数 ────────────────────────────────────────────────────────────

// 从文件路径读取文本内容，返回 std::string
static std::string read_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open file: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// 编译单个着色器（GL_VERTEX_SHADER 或 GL_FRAGMENT_SHADER）
// 返回着色器对象名称（GLuint），失败时输出错误日志
static GLuint compile_shader(GLenum type, const char* path) {
    // 1. 读取源码文件
    std::string source = read_file(path);
    const char* c_str  = source.c_str();

    // 2. 创建着色器对象，上传源码
    //    glCreateShader 返回一个新的着色器对象名称（正整数）
    GLuint shader = glCreateShader(type);
    // glShaderSource 参数：着色器对象, 字符串数量, 字符串数组指针, 各字符串长度（nullptr=自动计算）
    glShaderSource(shader, 1, &c_str, nullptr);

    // 3. 编译
    glCompileShader(shader);

    // 4. 检查编译状态
    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (!compile_status) {
        // 获取错误日志（日志大小可用 GL_INFO_LOG_LENGTH 查询）
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "SHADER COMPILE ERROR (" << path << "):\n" << log << "\n";
    }

    return shader;  // 即使编译失败也返回对象（可继续 attach，link 时会失败并给出更多信息）
}

// 链接顶点着色器和片元着色器为程序对象
// 链接后自动删除着色器对象（它们已被程序持有，不需要单独保留）
static GLuint link_program(GLuint vert_shader, GLuint frag_shader) {
    // 1. 创建程序对象
    GLuint program = glCreateProgram();

    // 2. 将着色器附加到程序
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);

    // 3. 链接（解析 in/out 变量连接、uniform 位置等）
    glLinkProgram(program);

    // 4. 检查链接状态
    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (!link_status) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "PROGRAM LINK ERROR:\n" << log << "\n";
    }

    // 5. 链接完成后删除着色器对象
    //    程序已持有编译结果，原始着色器对象可以释放
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main() {
    // ────────────────────────────────────────────────────────────────────────
    // 阶段 1：GLFW 初始化与窗口创建
    // ────────────────────────────────────────────────────────────────────────

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // 指定 OpenGL 版本：4.6 Core Profile
    // Core Profile 移除了所有 legacy OpenGL 特性（glBegin/glEnd、默认 VAO 等）
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 请求调试上下文：驱动会为每个 API 调用做额外验证，性能略低但错误信息丰富
    // 发布版本中应移除这一行
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    // 将 OpenGL 上下文与当前线程绑定（每个线程只能有一个当前上下文）
    glfwMakeContextCurrent(window);

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 2：GLAD 加载 OpenGL 函数指针
    // ────────────────────────────────────────────────────────────────────────

    // OpenGL 函数不能直接链接，需要运行时从驱动动态库中加载函数指针。
    // GLAD 自动生成加载代码，glfwGetProcAddress 提供函数指针查询接口。
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // 打印 GPU 信息（诊断用）
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "Version:  " << glGetString(GL_VERSION)  << "\n";

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 3：启用 OpenGL 调试回调
    // ────────────────────────────────────────────────────────────────────────

    // GL_DEBUG_OUTPUT：异步调试输出（推荐总是开启）
    // GL_DEBUG_OUTPUT_SYNCHRONOUS：同步模式，确保在 API 调用时立即触发回调，
    //   调用栈完整可以定位到出错的那一行 glXxx 调用（调试期必开）
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_callback, nullptr);

    // 可以过滤特定消息：glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 4：准备顶点数据
    // ────────────────────────────────────────────────────────────────────────

    // 三角形三个顶点，每个顶点包含：
    //   位置（x, y, z）：3个 float，占 12 字节
    //   颜色（r, g, b）：3个 float，占 12 字节
    // 总步长（stride）= 24 字节
    //
    // 坐标系：NDC（归一化设备坐标），范围 [-1, 1]
    //   左下 → (-0.5, -0.5)，右下 → (0.5, -0.5)，顶部 → (0.0, 0.5)
    float vertices[] = {
        // 位置 x      y      z      颜色 r    g     b
        -0.5f, -0.5f,  0.0f,         1.0f, 0.0f, 0.0f,   // 左下顶点，红色
         0.5f, -0.5f,  0.0f,         0.0f, 1.0f, 0.0f,   // 右下顶点，绿色
         0.0f,  0.5f,  0.0f,         0.0f, 0.0f, 1.0f,   // 顶部顶点，蓝色
    };

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 5：创建 VAO（Vertex Array Object）
    // ────────────────────────────────────────────────────────────────────────

    // VAO 记录了：
    //   - 每个顶点属性的格式（哪个 location，几个分量，类型，步长，偏移）
    //   - 哪个 VBO 被绑定到每个属性
    //   - 是否启用了每个属性（glEnableVertexAttribArray）
    //
    // VAO 不存储顶点数据本身，数据在 VBO 中。
    // Core Profile 强制要求使用 VAO（没有默认 VAO）。
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    // 从这里开始，所有 VBO 绑定和 glVertexAttribPointer 调用都会被记录到 vao 中

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 6：创建 VBO（Vertex Buffer Object），上传顶点数据
    // ────────────────────────────────────────────────────────────────────────

    GLuint vbo;
    glGenBuffers(1, &vbo);

    // 绑定到 GL_ARRAY_BUFFER 目标（顶点数据缓冲）
    // 注意：在 VAO 绑定的情况下绑定 VBO，VAO 会记录这个绑定关系
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // 分配显存并上传数据
    // GL_STATIC_DRAW 提示：数据上传一次，频繁读取，不频繁修改
    //   影响驱动选择将数据放在哪种显存中（VRAM vs 系统内存映射）
    // 其他选项：GL_DYNAMIC_DRAW（频繁修改）, GL_STREAM_DRAW（每帧修改）
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 7：配置顶点属性（告诉 GPU 如何从 VBO 中读取顶点数据）
    // ────────────────────────────────────────────────────────────────────────

    // glVertexAttribPointer 参数说明：
    //   index     : 属性的 location（对应着色器中的 layout(location = X)）
    //   size      : 每个属性的分量数（1/2/3/4）
    //   type      : 数据类型（GL_FLOAT, GL_INT, ...）
    //   normalized: 是否将整型归一化到 [0,1] 或 [-1,1]（对 float 无意义）
    //   stride    : 相邻两个同属性顶点之间的字节间距（0 = 紧密排列，由 size*type 自动计算）
    //   pointer   : 该属性在每个顶点中的字节偏移（相对于当前绑定的 VBO 起点）

    // 属性 0：位置（location = 0）
    // - 3 个 float（x, y, z）
    // - stride = 6 个 float = 24 字节（每个顶点总大小）
    // - offset = 0（位置在顶点开头）
    glVertexAttribPointer(
        0,                    // location = 0（与着色器 layout(location=0) 对应）
        3,                    // vec3：x, y, z
        GL_FLOAT,             // float 类型
        GL_FALSE,             // 不归一化
        6 * sizeof(float),    // stride：每个完整顶点占 6 个 float = 24 字节
        (void*)0              // offset：位置数据从字节 0 开始
    );
    // 必须显式启用每个属性！默认是禁用的，忘记这一行则着色器收到的是全零数据
    glEnableVertexAttribArray(0);

    // 属性 1：颜色（location = 1）
    // - 3 个 float（r, g, b）
    // - stride = 24 字节（同上，因为 interleaved 排列）
    // - offset = 3 个 float = 12 字节（颜色在位置数据之后）
    glVertexAttribPointer(
        1,                        // location = 1
        3,                        // vec3：r, g, b
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),        // stride
        (void*)(3 * sizeof(float)) // offset：颜色从第 4 个 float 开始（字节 12）
    );
    glEnableVertexAttribArray(1);

    // 此时 VAO 已完整记录了顶点格式，可以解绑 VBO 和 VAO
    // （解绑 VAO 不会丢失已记录的配置，VAO 内部保存了所有状态）
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 8：编译链接着色器程序
    // ────────────────────────────────────────────────────────────────────────

    // 注意：着色器路径是相对于工作目录（可执行文件所在目录）
    // CMakeLists.txt 中的 POST_BUILD 命令将 shaders/ 复制到可执行文件旁边
    GLuint program = link_program(
        compile_shader(GL_VERTEX_SHADER,   "shaders/triangle.vert"),
        compile_shader(GL_FRAGMENT_SHADER, "shaders/triangle.frag")
    );

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 9：渲染循环
    // ────────────────────────────────────────────────────────────────────────

    while (!glfwWindowShouldClose(window)) {
        // 处理输入：按 ESC 关闭窗口
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ── 清屏 ──
        // 设置清除颜色（深灰色背景）
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        // 清除颜色缓冲（如果有深度写入还需 GL_DEPTH_BUFFER_BIT）
        glClear(GL_COLOR_BUFFER_BIT);

        // ── 绘制三角形 ──
        // 激活着色器程序（之后的 draw call 使用此程序）
        glUseProgram(program);

        // 绑定 VAO（恢复顶点属性配置 + VBO 绑定）
        glBindVertexArray(vao);

        // 绘制命令：
        //   GL_TRIANGLES : 每3个顶点构成一个三角形
        //   0            : 起始顶点索引
        //   3            : 顶点数量（1个三角形 = 3个顶点）
        // 驱动将此命令提交到命令缓冲区，GPU 异步执行
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ── 交换缓冲 ──
        // 将后缓冲内容显示到屏幕（前后缓冲交换）
        // 默认开启垂直同步，会等待显示器刷新，帧率≤刷新率（通常60fps）
        glfwSwapBuffers(window);

        // 处理 GLFW 事件队列（窗口关闭、键盘、鼠标等）
        glfwPollEvents();
    }

    // ────────────────────────────────────────────────────────────────────────
    // 阶段 10：清理 OpenGL 资源
    // ────────────────────────────────────────────────────────────────────────

    // 删除顺序无严格要求，但良好习惯是按依赖关系反向删除
    glDeleteVertexArrays(1, &vao);   // VAO 中记录的 VBO 绑定也随之失效
    glDeleteBuffers(1, &vbo);         // VBO 的 GPU 内存释放
    glDeleteProgram(program);          // 程序对象（包含链接后的着色器）

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
