# Module 14 — Advanced OpenGL

> OpenGL 4.6 Core Profile · C++17
> 演示：实例化渲染（10000 小行星）、SSBO 大数组、间接绘制、UBO 管理、Compute Shader 轨道更新

---

## 目录

1. [实例化渲染](#1-实例化渲染)
2. [SSBO 与 UBO 对比](#2-ssbo-与-ubo-对比)
3. [std430 对齐规则](#3-std430-对齐规则)
4. [间接绘制](#4-间接绘制)
5. [UBO 最佳实践](#5-ubo-最佳实践)
6. [Compute Shader 深度](#6-compute-shader-深度)
7. [GPU Driven 渲染架构概述](#7-gpu-driven-渲染架构概述)
8. [内存屏障与同步](#8-内存屏障与同步)
9. [常见陷阱](#9-常见陷阱)
10. [代码结构](#10-代码结构)
11. [编译与运行](#11-编译与运行)

---

## 1. 实例化渲染

### 1.1 问题：大量 Draw Call 的驱动开销

传统渲染每个物体需要一次 draw call。当场景中有 10000 个小行星时：

```
// 错误做法：10000 次 draw call
for (auto& asteroid : asteroids) {
    upload_model_matrix(asteroid.transform);  // GPU uniform upload
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
}
```

每次 `glDrawElements` 都会产生：
- **驱动验证开销**：检查状态机、绑定合法性
- **命令缓冲提交**：CPU→GPU 通道切换
- **同步等待**（部分情况）

10000 次 draw call 在大多数 GPU 上会严重受 CPU 限制（CPU-bound），即使 GPU 完全空闲，CPU 也忙于提交命令。

**实测参考**：NVIDIA GTX 1080 在 60fps 预算（16ms/帧）内能处理约 3000-5000 个 draw call。Intel 集显约 1000-2000 个。

### 1.2 解决方案：glDrawArraysInstanced / glDrawElementsInstanced

```cpp
// 一次 draw call，渲染 N 个实例
glDrawElementsInstanced(GL_TRIANGLES,
    mesh.index_count,      // 索引数量（单个实例）
    GL_UNSIGNED_INT,       // 索引类型
    nullptr,               // 索引偏移
    instance_count);       // 实例数量
```

GPU 自动重复执行顶点着色器 `instance_count` 次，每次递增 `gl_InstanceID`。

### 1.3 gl_InstanceID 的用途

`gl_InstanceID` 是顶点着色器内置变量，值为当前实例的 0-based 索引。

使用方法：

```glsl
// 方法 A：通过 glVertexAttribDivisor（顶点属性方式）
layout(location = 3) in mat4 aInstanceMatrix;  // 4 个 vec4 slot (3,4,5,6)
// 然后用 glVertexAttribDivisor(3, 1) 等设置每实例更新

// 方法 B：通过 SSBO 索引（本模块采用）
layout(std430, binding = 0) readonly buffer InstanceData {
    mat4 uModelMatrices[];
};
mat4 model = uModelMatrices[gl_InstanceID];
```

### 1.4 顶点属性方式 vs SSBO 方式

| 对比项 | 顶点属性（glVertexAttribDivisor）| SSBO |
|--------|----------------------------------|------|
| 数据大小限制 | 顶点缓冲最大 ~2GB | SSBO 最大 ~128MB-2GB |
| 驱动兼容性 | OpenGL 3.3+ | OpenGL 4.3+ |
| 随机访问 | 无（只能顺序） | 支持（任意索引） |
| 着色器写入 | 不支持 | 支持 |
| 代码简洁度 | 稍复杂（多个 attrib pointer） | 简洁 |
| 性能 | 接近 | 接近（现代 GPU 差异极小） |

**结论**：现代 OpenGL（4.3+）推荐 SSBO，更灵活，尤其适合 Compute Shader 与渲染管线共享数据。

---

## 2. SSBO 与 UBO 对比

### 2.1 核心差异

| 特性 | UBO（Uniform Buffer Object） | SSBO（Shader Storage Buffer Object）|
|------|------------------------------|--------------------------------------|
| OpenGL 版本 | 3.1+ | 4.3+ |
| 最小保证大小 | 16KB（`GL_MAX_UNIFORM_BLOCK_SIZE`，通常 64KB）| 128MB（`GL_MAX_SHADER_STORAGE_BLOCK_SIZE`，通常 2GB+）|
| 着色器写入 | 不支持（readonly）| 支持（可读写）|
| 布局规范 | std140 | std430 或 std140 |
| 访问速度 | 更快（驱动可在常量缓存中缓存）| 稍慢（走普通内存路径）|
| 原子操作 | 不支持 | 支持（atomicAdd 等）|
| 适用场景 | 相机矩阵、光照参数等小数据 | 大数组、粒子系统、间接参数 |

### 2.2 UBO 的使用场景

UBO 专为小型共享数据设计。标准用法：

```glsl
// 定义（std140 布局）
layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;        // 64 bytes
    mat4 uProjection;  // 64 bytes
    vec3 uCamPos;      // 12 bytes
    float _pad;        // 4 bytes → total 144 bytes
};
```

```cpp
// C++ 端
GLuint ubo;
glGenBuffers(1, &ubo);
glBindBuffer(GL_UNIFORM_BUFFER, ubo);
glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraBlock), nullptr, GL_DYNAMIC_DRAW);
glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);  // binding = 0
```

所有绑定到 binding 0 的着色器自动共享同一份相机数据，无需每个 program 单独设 uniform。

### 2.3 SSBO 的使用场景

SSBO 用于大规模数据或需要 GPU 写入的场景：

```glsl
layout(std430, binding = 0) readonly buffer InstanceData {
    mat4 uModelMatrices[];  // 10000 * 64B = 640KB，超过 UBO 限制
};
```

---

## 3. std430 对齐规则

SSBO 默认使用 std430 布局，与 UBO 的 std140 有重要差异。

### 3.1 std140 规则（UBO 常用）

| 类型 | base alignment | 实际占用 |
|------|----------------|----------|
| float | 4 | 4 |
| vec2 | 8 | 8 |
| vec3 | **16** | 12+4（填充到 16）|
| vec4 | 16 | 16 |
| mat4 | 16（每列）| 64 |
| float[N] | **16**（每元素）| N * 16（即使 float 只需 4！）|
| vec4[N] | 16 | N * 16 |

**注意**：std140 中 `float` 数组每个元素占 16 字节，因为数组元素的 base alignment 至少为 16。

### 3.2 std430 规则（SSBO 常用）

std430 修正了 std140 中数组对齐过大的问题：

| 类型 | base alignment | 实际占用 |
|------|----------------|----------|
| float | 4 | 4 |
| vec2 | 8 | 8 |
| vec3 | **16** | **12+4**（vec3 仍然坑！）|
| vec4 | 16 | 16 |
| mat4 | 16（每列）| 64 |
| float[N] | **4**（每元素）| N * 4（正常！）|
| vec3[N] | **16**（每元素）| N * 16（仍然坑！）|

### 3.3 最大的陷阱：vec3 在 std430 中

**vec3 在 std430 中每个元素仍然占 16 字节！**

```glsl
// 错误：以为 vec3 数组是紧凑的 12 字节每元素
layout(std430, binding = 0) buffer Data {
    vec3 positions[];  // 实际每元素 16 字节，有 4 字节 padding！
};

// 正确方案1：用 vec4
layout(std430, binding = 0) buffer Data {
    vec4 positions[];  // 明确 16 字节对齐，与 GPU 行为一致
};

// 正确方案2：用 float 数组
layout(std430, binding = 0) buffer Data {
    float data[];  // positions: data[i*3], data[i*3+1], data[i*3+2]
};
```

**C++ 端**同样需要注意：
```cpp
// C++ 中 glm::vec3 是 12 字节（紧凑），但 GPU SSBO 的 vec3 是 16 字节
// 如果用 glm::vec3 上传数据到 vec3[] SSBO，数据会错位！
// 解决：C++ 端也用 glm::vec4，或用 struct { float x,y,z,w; }
```

### 3.4 std140 vs std430 的关键区别

```
std140: float[3] = [0..15][16..31][32..47]  每个占 16 字节
std430: float[3] = [0..3][4..7][8..11]      每个占 4 字节
```

std430 使 SSBO 更紧凑，特别适合大型数组传输，减少显存浪费。

---

## 4. 间接绘制

### 4.1 传统 vs 间接绘制

传统流程：
```
CPU: 计算可见物体 → 提交 N 个 draw call
GPU: 执行 draw call
```

间接绘制流程：
```
CPU: 上传 DrawCommand 数组到 GPU buffer
GPU: 直接从 buffer 读取命令执行
```

关键优势：**Draw Command 可以由 GPU 端（Compute Shader）生成**，CPU 完全不参与。

### 4.2 DrawElementsIndirectCommand 结构

```cpp
struct DrawElementsIndirectCommand {
    GLuint count;           // 字段1: 每次 draw 的索引数量
    GLuint instance_count;  // 字段2: 实例数量（0 表示跳过该 draw call）
    GLuint first_index;     // 字段3: 索引缓冲中的起始位置（偏移，单位：索引个数）
    GLuint base_vertex;     // 字段4: 加到每个索引的偏移（合并顶点缓冲时使用）
    GLuint base_instance;   // 字段5: 加到 gl_InstanceID 的偏移
};
```

**字段详解**：

- `count`：单次 draw 的索引数量。对于一个有 36 个面的立方体（12 个三角形），count = 36。
- `instance_count`：实例数量。GPU Culling 后，未通过视锥体测试的 draw call 可将此设为 0，GPU 自动跳过。
- `first_index`：允许多个 draw call 共享同一个大型索引缓冲（合并索引缓冲优化）。
- `base_vertex`：允许多个 draw call 共享同一个大型顶点缓冲，每个 mesh 有不同的顶点起始偏移。
- `base_instance`：与 gl_InstanceID 的关系：`gl_InstanceID = local_instance_id + base_instance`（OpenGL 4.2+）。用于从大型 instance 数据数组中选取对应片段。

### 4.3 使用方式

```cpp
// 绑定 draw command buffer
glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer);

// 执行一次 CPU 调用，触发 N 个 draw call
glMultiDrawElementsIndirect(
    GL_TRIANGLES,                   // 图元类型
    GL_UNSIGNED_INT,                // 索引类型
    nullptr,                        // offset = 0（从 buffer 开始读）
    cmd_count,                      // draw command 数量
    0                               // stride = 0（紧凑排列）
);
```

### 4.4 GPU Frustum Culling 的前置知识

间接绘制的最大价值是配合 Compute Shader 做 GPU 端视锥体剔除：

```
帧开始
│
├── Compute Pass: Frustum Culling
│     输入：所有物体的 AABB（SSBO），相机视锥体（UBO）
│     输出：DrawElementsIndirectCommand 数组（SSBO）
│         ├── 可见物体：instance_count = 1
│         └── 不可见物体：instance_count = 0
│
├── glMemoryBarrier(GL_COMMAND_BARRIER_BIT)  ← 关键！
│
└── Draw Pass: glMultiDrawElementsIndirect
      CPU 完全不知道哪些物体可见，直接让 GPU 执行
```

这种方式将视锥体剔除的 CPU 开销降至 0，特别适合百万级物体的场景。

---

## 5. UBO 最佳实践

### 5.1 Binding Point 分配策略

推荐为全局数据分配固定 binding：

```
binding = 0: 相机矩阵（CameraBlock）
binding = 1: 全局光照（LightBlock）
binding = 2: 材质参数（MaterialBlock）（可选）
binding = 3: 时间/动画（TimeBlock）（可选）
```

着色器中声明时使用 `binding = N`：

```glsl
layout(std140, binding = 0) uniform CameraBlock { ... };
layout(std140, binding = 1) uniform LightBlock  { ... };
```

C++ 端将 UBO 绑定到对应 binding：

```cpp
glBindBufferBase(GL_UNIFORM_BUFFER, 0, camera_ubo);  // binding = 0
glBindBufferBase(GL_UNIFORM_BUFFER, 1, lights_ubo);  // binding = 1
```

### 5.2 跨着色器共享

同一个 UBO（相同 binding）可以被任意多个着色器程序共享，无需为每个 program 单独设置 uniform。

```cpp
// 只需一次绑定，所有使用 binding=0 的着色器自动使用
glBindBufferBase(GL_UNIFORM_BUFFER, 0, camera_ubo);

// 更新数据（所有相关着色器立即可见）
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraBlock), &block);
```

### 5.3 显式 Binding 声明（推荐）

OpenGL 4.2+ 支持在着色器中直接声明 binding（不需要 C++ 端 glUniformBlockBinding）：

```glsl
// 方式1（旧，需要 C++ 端配合）
uniform CameraBlock { ... };  // 然后 glUniformBlockBinding(prog, idx, 0)

// 方式2（推荐，binding 固定在着色器中）
layout(std140, binding = 0) uniform CameraBlock { ... };
```

### 5.4 UBO 更新频率分类

| 更新频率 | 内容 | 建议用法 |
|----------|------|----------|
| 每帧 | 相机矩阵、时间 | GL_DYNAMIC_DRAW + glBufferSubData |
| 场景切换 | 光照参数 | GL_STATIC_DRAW 或 GL_DYNAMIC_DRAW |
| 不变 | 材质常量 | GL_STATIC_DRAW |

---

## 6. Compute Shader 深度

### 6.1 工作组模型

Compute Shader 将工作分层组织：

```
Global Dispatch: glDispatchCompute(gx, gy, gz)
│
├── Work Group [0,0,0]  ─  Work Group [1,0,0]  ─ ... ─ Work Group [gx-1,gy-1,gz-1]
│         │
│     Invocations（线程）
│     layout(local_size_x=256, local_size_y=1, local_size_z=1) in;
│     每个工作组有 256×1×1 = 256 个线程
```

内置变量：

| 变量 | 类型 | 含义 |
|------|------|------|
| `gl_LocalInvocationID` | uvec3 | 当前线程在工作组内的 3D 索引 [0,local_size-1] |
| `gl_WorkGroupID` | uvec3 | 当前工作组在全局中的 3D 索引 [0,num_groups-1] |
| `gl_GlobalInvocationID` | uvec3 | 全局唯一线程 ID = WorkGroupID * LocalSize + LocalInvocationID |
| `gl_LocalInvocationIndex` | uint | LocalInvocationID 的线性化版本 |
| `gl_NumWorkGroups` | uvec3 | dispatch 时传入的工作组数量 |

计算全局线程 ID 的标准方式：

```glsl
uint idx = gl_GlobalInvocationID.x;  // 1D 情形
// 等价于：
uint idx = gl_WorkGroupID.x * gl_WorkGroupSize.x + gl_LocalInvocationID.x;
```

### 6.2 工作组大小选择

```glsl
layout(local_size_x = 256) in;  // 本模块使用
```

推荐 local_size_x 为 64、128 或 256：
- **太小**（如 1）：调度开销大，SM 利用率低
- **太大**（如 1024）：寄存器压力增大，可能降低 occupancy
- **256**：对大多数 GPU 是良好的默认值

### 6.3 共享内存（Shared Memory）

工作组内的线程可以通过共享内存高速通信：

```glsl
shared float s_data[256];  // 工作组共享内存，最大约 48KB

void main() {
    uint local_idx = gl_LocalInvocationID.x;

    // 每个线程加载一个元素到共享内存
    s_data[local_idx] = some_global_data[gl_GlobalInvocationID.x];

    // 等待工作组内所有线程完成加载
    barrier();

    // 现在可以安全读取任意 s_data[i]
    float neighbor = s_data[(local_idx + 1) % 256];
    // ...
}
```

**Bank Conflict**：GPU 共享内存分为 32 个 bank，若多个线程同时访问同一 bank 的不同地址，会串行化（性能下降）。

访问模式 `s_data[i]`（stride = 1）通常无 bank conflict。访问模式 `s_data[i * 32]` 会让所有线程访问同一 bank，产生严重冲突。

### 6.4 内存屏障详解

Compute Shader 中有多种屏障，混淆会导致数据竞争或死锁：

| 函数 | 作用域 | 用途 |
|------|--------|------|
| `barrier()` | 工作组内 | 同步线程执行点（所有线程到达才继续）|
| `memoryBarrier()` | 全局 | 确保当前线程的内存写入对其他线程可见（不阻塞执行）|
| `groupMemoryBarrier()` | 工作组内 | 工作组内的内存可见性屏障 |
| `memoryBarrierShared()` | 工作组内 | 仅针对共享内存的可见性屏障 |
| `memoryBarrierBuffer()` | 全局 | 仅针对 buffer（SSBO）的可见性屏障 |

常见用法（Reduce 操作）：

```glsl
shared float s_partial[256];

void main() {
    uint idx = gl_LocalInvocationID.x;
    s_partial[idx] = data[gl_GlobalInvocationID.x];

    barrier();  // ← 等待所有线程写入共享内存

    // Parallel reduce
    for (uint stride = 128; stride > 0; stride >>= 1) {
        if (idx < stride) {
            s_partial[idx] += s_partial[idx + stride];
        }
        barrier();  // ← 每次迭代都需要 barrier
    }
    if (idx == 0) result[gl_WorkGroupID.x] = s_partial[0];
}
```

---

## 7. GPU Driven 渲染架构概述

现代 AAA 游戏引擎（如 Unreal Engine 5 Nanite）采用 GPU Driven 架构，将几乎所有 CPU 提交工作转移到 GPU：

```
CPU 帧开始
│
├── 上传场景数据（每帧变化部分，极少量）
│
GPU 工作
├── Compute Pass 1: 场景更新（位置、动画）
│     SSBO: 变换矩阵数组
│
├── Compute Pass 2: Frustum Culling + LOD 选择
│     输入：物体 AABB（SSBO）+ 视锥体（UBO）
│     输出：DrawIndirectCommand 数组（SSBO）
│
├── glMemoryBarrier(GL_COMMAND_BARRIER_BIT)
│
├── Draw Pass: glMultiDrawElementsIndirect
│     GPU 自己决定画什么、画多少
│
└── 渲染完成
```

**与传统架构的对比**：

| 项目 | 传统 | GPU Driven |
|------|------|------------|
| CPU draw call 数 | N（物体数） | 1（MultiDrawIndirect）|
| 视锥体剔除 | CPU | GPU Compute |
| LOD 选择 | CPU | GPU Compute |
| 帧同步等待 | 多次 | 最少化 |

---

## 8. 内存屏障与同步

### 8.1 GPU 内部写入可见性问题

GPU 的执行是异步的，写入共享内存或 buffer 后，不保证立即对其他 shader stage 或线程可见。

### 8.2 glMemoryBarrier（CPU 端调用）

在 Compute Shader 写入 SSBO 之后，如果后续要用这些数据进行绘制，需要插入全局内存屏障：

```cpp
// 1. Dispatch Compute Shader（更新 SSBO 中的变换矩阵）
glDispatchCompute(groups, 1, 1);

// 2. 确保 SSBO 写入对后续顶点着色器可见
glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

// 3. 现在可以用这个 SSBO 进行绘制
glDrawElementsInstanced(...);
```

常用 barrier bit：

| Bit | 含义 |
|-----|------|
| `GL_SHADER_STORAGE_BARRIER_BIT` | SSBO 读写可见性 |
| `GL_UNIFORM_BARRIER_BIT` | UBO 写入可见性 |
| `GL_COMMAND_BARRIER_BIT` | Indirect Draw Buffer 写入可见性 |
| `GL_TEXTURE_FETCH_BARRIER_BIT` | Texture 写入可见性 |
| `GL_ALL_BARRIER_BITS` | 全部（保守，性能最差）|

### 8.3 着色器内部屏障 vs CPU 端屏障

- **着色器内 `barrier()`**：工作组内线程同步（只在 Compute/TCS 中有效）
- **CPU 端 `glMemoryBarrier()`**：跨 stage 的全局同步

两者解决不同层面的问题：着色器内 barrier 同步线程，glMemoryBarrier 同步 pipeline stage。

---

## 9. 常见陷阱

### 9.1 SSBO Binding 冲突

多个 SSBO 使用相同 binding 点会相互覆盖：

```glsl
// 错误：两个 SSBO 都用 binding = 0
layout(std430, binding = 0) buffer A { float a[]; };
layout(std430, binding = 0) buffer B { float b[]; };  // 覆盖 A！

// 正确：使用不同 binding
layout(std430, binding = 0) buffer A { float a[]; };
layout(std430, binding = 1) buffer B { float b[]; };
```

### 9.2 vec3 在 SSBO 中的对齐问题

已在第 3 节详述。**结论：在 SSBO 中永远不要用 vec3 数组，改用 vec4 或 float 数组。**

```cpp
// C++ 端：glm::vec3 是 12 字节紧凑，但 SSBO vec3 是 16 字节
// 若直接上传 std::vector<glm::vec3>，数据会错位
// 解决方案：
struct SafeVec3 { float x, y, z, _pad; };  // 手动 pad 到 16 字节
std::vector<SafeVec3> data;
```

### 9.3 Compute 工作组边界处理

Dispatch 时必须处理边界，否则越界访问：

```cpp
int n = 10000;
int group_size = 256;
int groups = (n + group_size - 1) / group_size;  // ceil(n/group_size)
glDispatchCompute(groups, 1, 1);
```

着色器中：
```glsl
uint idx = gl_GlobalInvocationID.x;
if (idx >= uint(total_count)) return;  // 防越界
```

不加越界检查可能导致写入相邻 SSBO 数据、访问非法内存（行为未定义）。

### 9.4 忘记 glMemoryBarrier

Compute Shader 写入 SSBO，然后直接 draw 而不插入 barrier：

```cpp
// 错误：结果可能是旧数据或垃圾
glDispatchCompute(groups, 1, 1);
// 忘了 glMemoryBarrier！
glDrawElementsInstanced(...);  // 顶点着色器可能看到未更新的数据

// 正确：
glDispatchCompute(groups, 1, 1);
glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // ← 必须
glDrawElementsInstanced(...);
```

### 9.5 glUniformBlockBinding 与 layout binding 混用

若着色器中既有 `layout(binding=0)` 又在 C++ 端调用 `glUniformBlockBinding`，binding 以 **layout 声明为准**（OpenGL 4.2+），`glUniformBlockBinding` 被覆盖。建议统一使用其中一种方式。

### 9.6 indirect draw 时未绑定 DRAW_INDIRECT_BUFFER

```cpp
// 错误：忘记绑定
glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, 4, 0);
// → GL_INVALID_OPERATION

// 正确：
glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer);
glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, 4, 0);
```

---

## 10. 代码结构

```
module14_advanced_gl/
├── CMakeLists.txt
├── include/
│   ├── instance_renderer.h   # 实例化渲染封装（SSBO）
│   ├── indirect_draw.h       # 间接绘制缓冲封装
│   └── ubo_manager.h         # UBO 管理
├── src/
│   ├── main.cpp              # 主程序，4 个演示 + ImGui 切换
│   ├── instance_renderer.cpp
│   ├── indirect_draw.cpp
│   └── ubo_manager.cpp
└── shaders/
    ├── instanced.vert         # SSBO 实例化渲染
    ├── instanced.frag
    ├── ubo_demo.vert          # UBO 读取相机/光照
    ├── ubo_demo.frag
    └── orbit_update.comp      # Compute Shader：GPU 端轨道更新
```

### 10.1 InstanceRenderer 设计说明

`InstanceRenderer` 通过 SSBO（binding=0）传递变换矩阵，顶点着色器用 `gl_InstanceID` 索引：

```
CPU: upload_transforms() → glBufferSubData → SSBO
GPU: instanced.vert: model = uModelMatrices[gl_InstanceID]
```

### 10.2 IndirectDrawBuffer 设计说明

`IndirectDrawBuffer` 封装 `GL_DRAW_INDIRECT_BUFFER`，支持 `glMultiDrawElementsIndirect`：

```
CPU: upload() → glBufferSubData → Indirect Buffer
GPU: glMultiDrawElementsIndirect → 读取 4 个 DrawCommand → 4 次 draw
```

### 10.3 Compute Shader 轨道更新

`orbit_update.comp` 在 GPU 端更新粒子的圆轨道位置，使用开普勒第三定律近似（角速度 ∝ r^{-3/2}）：

```
Dispatch: ceil(10000/256) = 40 个工作组 × 256 线程 = 10240 invocations
每个 invocation 处理 1 个粒子（越界检查：idx >= 10000 则 return）
```

---

## 11. 编译与运行

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target module14_advanced_gl
./build/module14_advanced_gl/module14_advanced_gl
```

### 11.1 演示说明

| Demo | 内容 | 关键 API |
|------|------|----------|
| 0: Instanced | 10000 个小行星（静态轨道）| `glDrawElementsInstanced` |
| 1: GPU Orbit | Compute Shader 更新轨道 | `glDispatchCompute` + SSBO |
| 2: Indirect Draw | MultiDraw，4 组各 2500 实例 | `glMultiDrawElementsIndirect` |
| 3: UBO Demo | 单个大立方体，多光源（UBO）| `glBindBufferBase(GL_UNIFORM_BUFFER, ...)` |

### 11.2 操作说明

| 操作 | 功能 |
|------|------|
| ImGui 单选按钮 | 切换演示 |
| 右键拖拽 | 旋转摄像机 |
| WASD | 移动摄像机（较快）|
| ESC | 退出 |

### 11.3 性能参考

| Demo | 预期 FPS（GTX 1060）| CPU bound? |
|------|---------------------|------------|
| 循环 draw call（对比基准）| ~15 FPS | 是 |
| 0: Instanced | 200+ FPS | 否（GPU bound）|
| 1: GPU Orbit | 180+ FPS | 否 |
| 2: Indirect | 200+ FPS | 否（0 CPU 提交）|
| 3: UBO | 1000+ FPS | 否（1 个 cube）|

实例化渲染将帧率从 ~15 FPS 提升到 200+ FPS，是 10000 物体场景下最直观的 GPU 加速效果。

---

*Module 14 展示了现代 OpenGL GPU Driven 渲染管线的基础组件。完整的生产管线还包括：Occlusion Culling（HiZ）、级联 LOD（HLOD）、Mesh Shader（取代传统 VS/GS 管线）、以及 Vulkan/D3D12 的更底层控制。*
