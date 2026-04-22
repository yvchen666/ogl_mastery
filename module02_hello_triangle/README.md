# Module 02 — Hello Triangle

## 目录

1. [模块目的](#1-模块目的)
2. [架构图：CPU数据到帧缓冲](#2-架构图cpu数据到帧缓冲)
3. [OpenGL 对象模型](#3-opengl-对象模型)
4. [VAO/VBO/EBO 关系图](#4-vaovboebo-关系图)
5. [顶点属性布局详解](#5-顶点属性布局详解)
6. [着色器编译链接完整流程](#6-着色器编译链接完整流程)
7. [OpenGL 调试回调](#7-opengl-调试回调)
8. [双缓冲原理](#8-双缓冲原理)
9. [NDC 坐标系](#9-ndc-坐标系)
10. [常见坑](#10-常见坑)
11. [构建运行与延伸阅读](#11-构建运行与延伸阅读)

---

## 1. 模块目的

本模块实现 OpenGL 的"Hello World"——在屏幕上绘制一个彩色三角形。

这个程序看起来简单，但它涉及了 OpenGL 编程的所有核心机制：

- **GLFW**：创建操作系统窗口并获取 OpenGL 上下文
- **GLAD**：运行时加载 OpenGL 函数指针
- **VAO/VBO**：将 CPU 端的顶点数组上传到 GPU 显存，并描述其格式
- **GLSL 着色器**：顶点着色器负责位置变换，片元着色器负责颜色输出
- **渲染循环**：事件处理 → 清屏 → 绘制 → 交换缓冲

理解这个程序的每一行，是后续所有模块的基础。

---

## 2. 架构图：CPU数据到帧缓冲

```
CPU 端（main.cpp）                    GPU 端

  float vertices[] = {              ┌─────────────────────────────────┐
    -0.5, -0.5, 0, 1, 0, 0,        │  VBO（GL_ARRAY_BUFFER）          │
     0.5, -0.5, 0, 0, 1, 0,        │  ┌────────────────────────────┐  │
     0.0,  0.5, 0, 0, 0, 1,        │  │ v0: [-0.5,-0.5,0, 1,0,0]  │  │
  };                                │  │ v1: [ 0.5,-0.5,0, 0,1,0]  │  │
       │                            │  │ v2: [ 0.0, 0.5,0, 0,0,1]  │  │
       │ glBufferData               │  └────────────────────────────┘  │
       ▼                            │                                   │
  glBindVertexArray(vao) ──────────►│  VAO                             │
  glVertexAttribPointer(0, ...)     │  ┌──────────────────────────────┐│
  glVertexAttribPointer(1, ...)     │  │ attrib[0]: loc=0, size=3,    ││
                                    │  │   stride=24, offset=0        ││
                                    │  │ attrib[1]: loc=1, size=3,    ││
                                    │  │   stride=24, offset=12       ││
                                    │  │ GL_ARRAY_BUFFER → VBO id     ││
                                    │  └──────────────────────────────┘│
                                    └─────────────────────────────────┘
                                                    │
                                    glDrawArrays(GL_TRIANGLES, 0, 3)
                                                    │
                                                    ▼
                                    ┌─────────────────────────────────┐
                                    │  顶点着色器（triangle.vert）      │
                                    │  in vec3 aPos  (location=0)     │
                                    │  in vec3 aColor (location=1)    │
                                    │  gl_Position = vec4(aPos, 1.0)  │
                                    │  vColor = aColor                │
                                    └──────────────┬──────────────────┘
                                                   │ vColor（插值）
                                                   ▼
                                    ┌─────────────────────────────────┐
                                    │  光栅化（硬件固定）               │
                                    │  三角形 → 片元（含插值属性）      │
                                    └──────────────┬──────────────────┘
                                                   │
                                                   ▼
                                    ┌─────────────────────────────────┐
                                    │  片元着色器（triangle.frag）      │
                                    │  in vec3 vColor（插值后颜色）     │
                                    │  FragColor = vec4(vColor, 1.0)  │
                                    └──────────────┬──────────────────┘
                                                   │
                                                   ▼
                                    ┌─────────────────────────────────┐
                                    │  帧缓冲（后缓冲）                 │
                                    │  彩色三角形已渲染完成             │
                                    └──────────────┬──────────────────┘
                                                   │ glfwSwapBuffers
                                                   ▼
                                              屏幕显示
```

---

## 3. OpenGL 对象模型

### 3.1 名称（Name）是什么

OpenGL 对象不是 C++ 对象，它是驱动内部的资源，通过一个 `GLuint` 整数**名称**访问：

```cpp
GLuint vbo;
glGenBuffers(1, &vbo);  // vbo = 例如 5（由驱动分配，总是正整数）
```

`glGenBuffers` 只是"注册"了一个名称，此时 GPU 上还没有分配任何内存。只有第一次绑定（`glBindBuffer`）时，对象才真正在 OpenGL 内部创建。

这是历史遗留设计：在早期 OpenGL 实现中，名称分配和对象创建分离可以允许客户端提前分配名称用于共享。

**DSA（Direct State Access，4.5+）**提供了更现代的 API：
```cpp
GLuint vbo;
glCreateBuffers(1, &vbo);  // 同时分配名称 + 创建对象，无需后续 bind
glNamedBufferData(vbo, sizeof(vertices), vertices, GL_STATIC_DRAW);  // 无需绑定即可操作
```

### 3.2 目标（Target）是什么

同一类对象（如 Buffer 对象）有多个**绑定点（Binding Point）**，称为目标：

```
GL_ARRAY_BUFFER          — 顶点数据（VBO）
GL_ELEMENT_ARRAY_BUFFER  — 索引数据（EBO/IBO）
GL_UNIFORM_BUFFER        — Uniform 块数据（UBO）
GL_SHADER_STORAGE_BUFFER — Shader Storage Block（SSBO）
GL_COPY_READ_BUFFER      — 缓冲区拷贝源
GL_COPY_WRITE_BUFFER     — 缓冲区拷贝目标
GL_TRANSFORM_FEEDBACK_BUFFER — 变换反馈
GL_ATOMIC_COUNTER_BUFFER — 原子计数器
```

每个目标同时只能绑定一个缓冲对象。当你调用 `glBufferData(GL_ARRAY_BUFFER, ...)` 时，操作的是当前绑定到 `GL_ARRAY_BUFFER` 目标的对象，而不是你指定名称的对象。

### 3.3 绑定点意味着什么

```cpp
// 状态机中的 GL_ARRAY_BUFFER 绑定点示意：
// 状态机当前状态：
//   GL_ARRAY_BUFFER → NULL（初始）

glBindBuffer(GL_ARRAY_BUFFER, vbo);
// 状态机当前状态：
//   GL_ARRAY_BUFFER → vbo（= 5）

glBufferData(GL_ARRAY_BUFFER, ...);
// 等价于：操作名称为 5 的 Buffer 对象
// "当前绑定到 GL_ARRAY_BUFFER 的对象" = vbo = 5

glBindBuffer(GL_ARRAY_BUFFER, 0);
// 解绑：GL_ARRAY_BUFFER → NULL
// 此时再调用 glBufferData 会产生错误（或操作默认缓冲，取决于实现）
```

---

## 4. VAO/VBO/EBO 关系图

```
VAO（Vertex Array Object）
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  GL_ELEMENT_ARRAY_BUFFER 绑定                               │
│  ┌────────────────────────────┐                            │
│  │ EBO（若有索引绘制）         │                            │
│  │ [0, 1, 2,  0, 2, 3, ...]  │                            │
│  └────────────────────────────┘                            │
│                                                             │
│  顶点属性表（attrib array）：                                │
│  ┌──────────┬──────────┬─────────┬──────────┬───────────┐  │
│  │ attrib # │ enabled  │ VBO 绑定│  格式     │  stride/  │  │
│  │          │ (bool)   │ (GLuint)│ size/type │  offset   │  │
│  ├──────────┼──────────┼─────────┼──────────┼───────────┤  │
│  │    0     │  TRUE    │    5    │  3/FLOAT  │  24 / 0   │  │
│  │    1     │  TRUE    │    5    │  3/FLOAT  │  24 / 12  │  │
│  │    2     │  FALSE   │    -    │     -     │    -      │  │
│  │   ...    │  ...     │   ...   │   ...    │   ...     │  │
│  └──────────┴──────────┴─────────┴──────────┴───────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘

VBO（id = 5）
┌─────────────────────────────────────────────────────────────┐
│ [GPU 显存中的原始字节数据]                                    │
│  字节 0-11:  顶点0的位置  (-0.5, -0.5, 0.0) = 3×4字节       │
│  字节12-23: 顶点0的颜色  (1.0,  0.0,  0.0) = 3×4字节        │
│  字节24-35: 顶点1的位置  (0.5, -0.5,  0.0)                  │
│  字节36-47: 顶点1的颜色  (0.0,  1.0,  0.0)                  │
│  字节48-59: 顶点2的位置  (0.0,  0.5,  0.0)                  │
│  字节60-71: 顶点2的颜色  (0.0,  0.0,  1.0)                  │
└─────────────────────────────────────────────────────────────┘
```

**关键理解**：
- VAO 存储的是"描述符"（how to read），不存储数据（what to read）
- VAO 中的每个属性记录了"从哪个 VBO 的哪个偏移处，按什么格式读取"
- 一个 VAO 可以引用多个不同的 VBO（每个属性可以来自不同 VBO）
- EBO 绑定 **存储在 VAO 内部**（这与 VBO 不同！VBO 绑定只是在配置时用）

---

## 5. 顶点属性布局详解

### 5.1 Interleaved（交错）格式

本例使用的格式，位置和颜色交错排列：

```
内存布局（Interleaved / AOS - Array of Structures）：
字节:  0    4    8   12   16   20   24   28   32   36   40   44   48 ...
       |Px0 |Py0 |Pz0 |Cr0 |Cg0 |Cb0 |Px1 |Py1 |Pz1 |Cr1 |Cg1 |Cb1 | ...
       |<───── 顶点0，24字节 ─────>|<───── 顶点1，24字节 ─────>| ...

glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0 );  // 位置
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12);  // 颜色
                      ↑  ↑                      ↑   ↑
                    loc size                  stride offset
```

**优点**：相同顶点的属性在内存中相邻，访问时缓存友好。
**缺点**：如果只需要位置（例如深度 pass），也需要跨越颜色数据，带宽浪费。

### 5.2 Separate（分离）格式

不同属性放在不同 VBO 中：

```
内存布局（Separate / SOA - Structure of Arrays）：
VBO_positions:  [Px0, Py0, Pz0,  Px1, Py1, Pz1,  Px2, Py2, Pz2]
VBO_colors:     [Cr0, Cg0, Cb0,  Cr1, Cg1, Cb1,  Cr2, Cg2, Cb2]

glBindBuffer(GL_ARRAY_BUFFER, vbo_positions);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);  // stride=12（紧密排列）

glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
```

**优点**：深度 pass 只需绑定 VBO_positions，颜色数据不占带宽。
**缺点**：需要维护多个 VBO，VAO 配置稍复杂。

### 5.3 stride 和 offset 计算规则

```
stride  = 该属性来源的 VBO 中，相邻两个顶点之间的字节距离
        = 如果是 interleaved，stride = 所有属性的总大小
        = 如果是 separate 且紧密排列，stride = 该属性自身的大小（或 0）

offset  = 该属性在一个顶点数据块中的起始字节位置
        = 所有排在它前面的属性的总大小之和
```

`stride = 0` 是特殊值，含义是"紧密排列（tightly packed）"，等价于 `stride = size * sizeof(type)`。

---

## 6. 着色器编译链接完整流程

```
                     ┌─────────────────────────────┐
文本文件             │  triangle.vert（GLSL源码）    │
"shaders/            └──────────────┬──────────────┘
 triangle.vert"                     │ read_file()
                                    ▼
                     ┌─────────────────────────────┐
                     │ glCreateShader(GL_VERTEX_SHADER)
                     │ → vert_shader_id = 1        │
                     └──────────────┬──────────────┘
                                    │ glShaderSource(1, 1, &src, nullptr)
                                    ▼
                     ┌─────────────────────────────┐
                     │ glCompileShader(1)           │
                     │ 驱动将 GLSL 编译为 GPU 指令  │
                     │ 检查：glGetShaderiv(         │
                     │   1, GL_COMPILE_STATUS, &ok) │
                     └──────────────┬──────────────┘
                                    │（frag_shader 同理）
                                    ▼
                     ┌─────────────────────────────┐
                     │ glCreateProgram()            │
                     │ → program_id = 3            │
                     └──────────────┬──────────────┘
                                    │ glAttachShader(3, vert_shader_id)
                                    │ glAttachShader(3, frag_shader_id)
                                    ▼
                     ┌─────────────────────────────┐
                     │ glLinkProgram(3)             │
                     │ 链接阶段：                   │
                     │  - 解析 VS out ↔ FS in 变量 │
                     │  - 分配 uniform locations   │
                     │  - 验证接口兼容性            │
                     │ 检查：glGetProgramiv(        │
                     │   3, GL_LINK_STATUS, &ok)   │
                     └──────────────┬──────────────┘
                                    │ glDeleteShader（可选，链接后可删）
                                    ▼
                     ┌─────────────────────────────┐
                     │ glUseProgram(3)              │
                     │ 之后的 draw call 使用此程序  │
                     └─────────────────────────────┘
```

### 6.1 着色器编译的时机

着色器编译在 `glCompileShader` 时触发，但驱动可能推迟实际编译到 `glLinkProgram` 或 `glUseProgram`。真正的 GPU 原生代码编译（ISA 生成）通常发生在：
1. `glLinkProgram`（理论上）
2. 第一次 `glUseProgram` + `glDrawArrays`（实际上许多驱动在此时才编译）

这是为什么第一帧可能卡顿（Shader Compilation Stutter）的原因。解决方案：预热着色器，或使用程序二进制缓存（`GL_ARB_get_program_binary`）。

### 6.2 Separate Shader Programs

OpenGL 4.1+ 支持分离着色器程序（`GL_ARB_separate_shader_objects`），可以独立切换 VS 和 FS：

```cpp
// 创建 Vertex Shader Program（独立程序，只含 VS）
GLuint vert_prog = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vert_src);

// 创建 Program Pipeline 组合 VS 和 FS
GLuint pipeline;
glGenProgramPipelines(1, &pipeline);
glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT,   vert_prog);
glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, frag_prog);
glBindProgramPipeline(pipeline);
```

---

## 7. OpenGL 调试回调

### 7.1 为什么比 glGetError 好

**glGetError 的问题**：
```cpp
glDrawArrays(GL_TRIANGLES, 0, 3);
GLenum err = glGetError();  // 阻塞！等待 GPU 完成所有命令，返回第一个错误
```

- **同步阻塞**：每次调用都强制 GPU 与 CPU 同步，帧率大幅下降
- **只返回第一个错误**：如果有多个错误，只能看到第一个，需要多次调用
- **没有上下文信息**：只有错误码（`GL_INVALID_OPERATION` 等），不知道哪个调用出错

**调试回调的优势**：
```cpp
glEnable(GL_DEBUG_OUTPUT);
glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);  // 同步模式：回调发生在出错的 API 调用内
glDebugMessageCallback(gl_debug_callback, nullptr);
// 之后不需要 glGetError，错误会自动触发回调
```

- **异步/同步可选**：`GL_DEBUG_OUTPUT_SYNCHRONOUS` 时，调用栈指向出错的 API 调用
- **丰富信息**：source（API/着色器/驱动）、type（错误/性能/已废弃）、severity（高/中/低）
- **可过滤**：只关注特定 source/type/severity 的消息

### 7.2 Severity 级别

| Severity | 含义 | 示例 |
|---------|------|------|
| `GL_DEBUG_SEVERITY_HIGH` | 必须修复，可能导致崩溃或未定义行为 | 访问未绑定对象、缓冲越界 |
| `GL_DEBUG_SEVERITY_MEDIUM` | 重要警告，可能导致错误行为 | 使用已废弃功能、着色器编译警告 |
| `GL_DEBUG_SEVERITY_LOW` | 轻微建议，通常可忽略 | 冗余状态设置 |
| `GL_DEBUG_SEVERITY_NOTIFICATION` | 纯信息（非错误）| 缓冲区内存分配通知 |

### 7.3 在回调中设置断点

调试时，可以在回调中设置条件断点：

```cpp
void APIENTRY gl_debug_callback(GLenum, GLenum type, GLuint, GLenum severity,
    GLsizei, const GLchar* msg, const void*) {
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        // 在这里设置断点 → 在 GL_DEBUG_OUTPUT_SYNCHRONOUS 模式下，
        // 调用栈的上层就是触发错误的 glXxx 调用
        __builtin_trap();  // 或 __debugbreak()（MSVC）
    }
    std::cerr << msg << "\n";
}
```

---

## 8. 双缓冲原理

### 8.1 为什么需要双缓冲

如果直接渲染到显示器正在读取的帧缓冲（前缓冲），会出现**画面撕裂（Tearing）**：

```
时间轴：
  显示器扫描：从上到下逐行读取帧缓冲
  GPU 渲染：同时写入同一帧缓冲

  若 GPU 渲染到一半，显示器恰好扫描到同一位置 →
  屏幕上半部分显示旧帧，下半部分显示新帧 → 水平撕裂线
```

双缓冲解决方案：
- **前缓冲（Front Buffer）**：显示器当前读取的缓冲，只读
- **后缓冲（Back Buffer）**：GPU 渲染目标，只写
- `glfwSwapBuffers`：在垂直消隐期（VBlank）交换两个缓冲的角色

### 8.2 垂直同步（VSync）

`glfwSwapBuffers` 默认等待垂直消隐期（显示器扫描完一帧后的空白期）才交换。

```
显示器刷新率 60Hz → VBlank 间隔 ≈ 16.67ms
glfwSwapBuffers 会等待下一个 VBlank → 帧率被锁定在 60fps

可以通过 glfwSwapInterval(0) 关闭 VSync：
  帧率不再受限，但可能出现撕裂，且 GPU 会满载运行（发热增加）
```

### 8.3 三缓冲

某些平台支持三缓冲（Triple Buffering）：
- 两个后缓冲交替渲染
- 一个前缓冲显示
- GPU 渲染完一帧后可以立即开始下一帧，不需要等待 VBlank
- 减少了延迟（latency），同时避免撕裂

---

## 9. NDC 坐标系

### 9.1 定义

**NDC（Normalized Device Coordinates，归一化设备坐标）**是透视除法后的坐标空间：

```
NDC 范围：[-1, 1]³

X 轴：向右为正（-1 = 左边缘，+1 = 右边缘）
Y 轴：向上为正（-1 = 底边缘，+1 = 顶边缘）
Z 轴：默认范围 [-1, 1]（-1 = 近平面，+1 = 远平面）
     注意：OpenGL 使用右手坐标系（观察空间 Z 向外，裁剪后映射到 [-1,1]）
     DX 的 NDC Z 范围是 [0, 1]
```

### 9.2 本例中为什么不需要投影矩阵

本例的顶点坐标：
```cpp
-0.5f, -0.5f, 0.0f  // 左下
 0.5f, -0.5f, 0.0f  // 右下
 0.0f,  0.5f, 0.0f  // 顶部
```

这些坐标已经处于 NDC 范围内（所有分量在 [-1, 1]），不需要任何变换。

顶点着色器直接输出：
```glsl
gl_Position = vec4(aPos, 1.0);  // w=1，透视除法 x/w=x，无变换
```

在实际应用中，顶点着色器会应用 MVP（Model × View × Projection）矩阵：
```glsl
uniform mat4 mvp;
gl_Position = mvp * vec4(aPos, 1.0);
```

### 9.3 视口变换

NDC 坐标到屏幕像素坐标的映射由 `glViewport` 定义：

```cpp
glViewport(0, 0, 800, 600);  // x=0, y=0, width=800, height=600

// 映射公式：
screen_x = (NDC_x + 1) / 2 * 800 = 400 * (NDC_x + 1)
screen_y = (NDC_y + 1) / 2 * 600 = 300 * (NDC_y + 1)

// 示例：
// NDC (-1, -1) → 屏幕 (0, 0)（左下角）
// NDC ( 1,  1) → 屏幕 (800, 600)（右上角）
// NDC ( 0,  0) → 屏幕 (400, 300)（中心）
```

---

## 10. 常见坑

### 坑 1：忘记 glEnableVertexAttribArray

```cpp
// 错误示例：配置了属性但忘记启用
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);
// 忘记：glEnableVertexAttribArray(0);

// 结果：着色器中 aPos 的值是未定义的（通常是全零）
// 三角形显示为一个点（所有顶点坐标 (0,0,0)）
```

**检查方法**：用调试回调，驱动会输出"attribute at index 0 is not enabled"警告。

### 坑 2：VAO 绑定顺序错误

```cpp
// 错误示例：在绑定 VAO 之前配置属性
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);  // 这不会记录到 vao！
glBindVertexArray(vao);  // 太迟了
```

**正确顺序**：
1. `glBindVertexArray(vao)` — 先绑定 VAO
2. `glBindBuffer(GL_ARRAY_BUFFER, vbo)` — 绑定 VBO
3. `glBufferData(...)` — 上传数据
4. `glVertexAttribPointer(...)` — 配置属性（此时会记录到 VAO）
5. `glEnableVertexAttribArray(...)` — 启用属性（记录到 VAO）

### 坑 3：着色器路径问题

```cpp
compile_shader(GL_VERTEX_SHADER, "shaders/triangle.vert");
```

这是相对路径，相对于**程序的工作目录**（不是源码目录，不是可执行文件目录）。

工作目录取决于如何启动程序：
- 从 IDE 启动：通常是项目根目录
- 从终端启动：是终端的当前目录
- 从 build 目录启动：是 build 目录

CMakeLists.txt 中的 `POST_BUILD` 命令将 shaders/ 复制到可执行文件旁边，保证从 build 目录运行时路径正确。

### 坑 4：Core Profile 无默认 VAO

在 OpenGL Compatibility Profile（兼容模式）中，有一个默认的 VAO（id=0），不需要显式创建。但 Core Profile（4.0+）移除了默认 VAO：

```cpp
// Core Profile 中，没有 glBindVertexArray 直接调用 glVertexAttribPointer 会报错：
// GL_INVALID_OPERATION: no current vertex array object
```

**解决**：始终在调用 `glVertexAttribPointer` 之前绑定一个 VAO。

### 坑 5：着色器错误日志缓冲区太小

```cpp
char log[512];  // 着色器编译错误信息可能远超 512 字节
glGetShaderInfoLog(s, 512, nullptr, log);  // 日志会被截断！
```

**解决**：先查询实际日志长度：
```cpp
GLint log_len;
glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
std::string log(log_len, '\0');
glGetShaderInfoLog(shader, log_len, nullptr, log.data());
```

### 坑 6：EBO 存储在 VAO 中（与 VBO 不同）

```cpp
glBindVertexArray(vao);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);  // 这个绑定被存入 VAO！

glBindVertexArray(0);  // 解绑 VAO

glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);  // 解绑 EBO（但 VAO 里还记着它）
glBindVertexArray(vao);                     // 重新绑定 VAO → EBO 也自动恢复绑定
```

相比之下，`GL_ARRAY_BUFFER` 绑定**不**存储在 VAO 中（VAO 只存储 `glVertexAttribPointer` 调用时的当前 VBO 引用）。这个不对称性非常容易混淆。

---

## 11. 构建运行与延伸阅读

### 构建

```bash
# 在 ogl_mastery 根目录：
CXX=g++-10 CC=gcc-10 cmake -B build && cmake --build build -j$(nproc)

# 运行（从 build 目录，确保 shaders/ 在工作目录下）：
cd build && ./module02_hello_triangle
```

预期结果：800x600 窗口，深灰色背景上显示一个彩色三角形（左下红、右下绿、顶部蓝）。按 ESC 退出。

### 调试技巧

1. 如果窗口是黑色的：检查着色器是否编译链接成功（控制台有无错误输出）
2. 如果着色器路径报错：确认从 build 目录运行，且 shaders/ 已被 POST_BUILD 复制
3. 如果出现 OpenGL 错误：确认 Debug Context 已启用，查看调试回调输出
4. 使用 RenderDoc 帧捕获：捕获一帧，可以看到 VAO/VBO 内容、每个 draw call 的 VS/FS 输入输出

### 延伸阅读

- **learnopengl.com** — https://learnopengl.com/
  Joey de Vries 的教程，与本模块高度对应，图文并茂，中文翻译质量好

- **OpenGL Wiki: Vertex Specification** — https://www.khronos.org/opengl/wiki/Vertex_Specification
  VAO/VBO 机制的官方参考

- **OpenGL Wiki: Debug Output** — https://www.khronos.org/opengl/wiki/Debug_Output
  调试回调的完整文档

- **GLFW 文档** — https://www.glfw.org/docs/latest/
  窗口、输入、上下文管理的完整参考

- **GLAD 生成器** — https://glad.dav1d.de/
  生成你指定版本的 OpenGL 加载代码

---

*下一模块：module03_glsl_deep — 深入 GLSL 语言特性，UBO、精度限定符、内置变量。*
