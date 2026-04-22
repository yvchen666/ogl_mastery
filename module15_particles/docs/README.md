# Module 15 — GPU Particle System

## 目录

1. [概述](#1-概述)
2. [GPU 粒子 vs CPU 粒子架构对比](#2-gpu-粒子-vs-cpu-粒子架构对比)
3. [SSBO（Shader Storage Buffer Object）](#3-ssboshader-storage-buffer-object)
4. [Compute Shader 工作组设计](#4-compute-shader-工作组设计)
5. [GLSL 随机数：原理与哈希函数](#5-glsl-随机数原理与哈希函数)
6. [Billboard 技术](#6-billboard-技术)
7. [软粒子（Soft Particles）](#7-软粒子soft-particles)
8. [加法混合（Additive Blending）](#8-加法混合additive-blending)
9. [Compute 与渲染管线同步](#9-compute-与渲染管线同步)
10. [构建与运行](#10-构建与运行)
11. [常见坑与调试技巧](#11-常见坑与调试技巧)

---

## 1. 概述

本模块实现一个完整的 **GPU 粒子系统**，全部物理模拟在 GPU 上完成：

- **100,000 个粒子**实时模拟，包含重力、初速度、颜色渐变、生命周期管理
- **Compute Shader** 每帧更新粒子状态；死亡粒子原地复位，无 CPU→GPU 传输
- **Billboard** 渲染：始终朝向摄像机的四边形，用相机坐标系右向量/上向量展开
- **加法混合**（`GL_ONE, GL_ONE`）实现粒子发光效果
- 帧率显示于窗口标题

### 控制方式

| 键/操作 | 功能 |
|---|---|
| W/A/S/D | 摄像机移动 |
| 右键拖拽 | 视角旋转 |
| ESC | 退出 |

---

## 2. GPU 粒子 vs CPU 粒子架构对比

### 2.1 CPU 粒子（传统方案）

```
┌───────────────────────────────────────────────────────┐
│                     CPU 帧循环                         │
│  for each particle:                                    │
│    velocity.y -= gravity * dt;                         │
│    position += velocity * dt;                          │
│    if (age > life) reset();                            │
│  glBufferSubData(GL_ARRAY_BUFFER, ..., data);  // 上传 │
└───────────────────────────────────────────────────────┘
         │ 每帧全量传输 n * sizeof(Particle) 字节
         ▼
┌─────────────────┐
│      GPU        │
│  仅负责渲染      │
└─────────────────┘
```

CPU 粒子的问题：
- **带宽瓶颈**：100,000 粒子 × 48 bytes = 4.8 MB/帧；60fps 时约 290 MB/s
- **CPU 占用**：单线程循环更新 10 万粒子约 1-2ms
- **PCIe 延迟**：数据必须先在 CPU 写完，再经 PCIe 总线推送到 GPU

### 2.2 GPU 粒子（本模块方案）

```
┌──────────────────────────────────────────────────────────┐
│                     GPU 帧循环                            │
│                                                           │
│  ① Compute Shader：更新物理（SSBO 读写，全并行）            │
│     glDispatchCompute(ceil(N/256), 1, 1)                 │
│     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)       │
│                                                           │
│  ② Vertex Shader：从 SSBO 读位置，展开 Billboard           │
│     glDrawArrays(GL_TRIANGLE_STRIP, 0, N*4)              │
│                                                           │
│  粒子数据 **始终驻留 VRAM**，CPU 不触碰                     │
└──────────────────────────────────────────────────────────┘
```

GPU 粒子的优势：
- **零同步开销**：物理数据从不离开 VRAM
- **完全并行**：10 万粒子并发更新，每 warp 32 线程同时运行
- **可扩展**：1,000,000 粒子同样流畅，仅增加 Compute 调度时间

### 2.3 性能数据对比（10万粒子，RTX 2060）

| 方案 | CPU 占用 | GPU 占用 | PCIe 带宽/帧 |
|---|---|---|---|
| CPU 粒子 | ~2ms | ~0.5ms | ~5MB |
| GPU 粒子 | <0.1ms | ~1ms | 0 |

---

## 3. SSBO（Shader Storage Buffer Object）

### 3.1 与 UBO 的区别

| 特性 | UBO | SSBO |
|---|---|---|
| 大小限制 | ~64KB（`GL_MAX_UNIFORM_BLOCK_SIZE`）| 几乎无限（受 VRAM 限制）|
| 着色器写入 | 只读 | 可读写 |
| 原子操作 | 不支持 | 支持（`atomicAdd` 等）|
| 随机访问 | 是 | 是 |

SSBO 完全适合粒子系统：数据量大（> 64KB），且 Compute Shader 需要写入。

### 3.2 std430 内存布局

```glsl
layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};
```

`std430` 规则（与 `std140` 的关键区别）：
- **vec4**：16 字节对齐，占 16 字节 ✓
- **float**：4 字节对齐 ✓
- **struct**：对齐到最大成员的对齐值

本模块的 `Particle` 结构体：
```glsl
struct Particle {
    vec4 position;   // offset 0,  size 16
    vec4 velocity;   // offset 16, size 16
    vec4 color;      // offset 32, size 16
};  // total: 48 bytes，无填充
```

C++ 端对应：
```cpp
struct Particle {
    glm::vec4 position;  // 16 bytes
    glm::vec4 velocity;  // 16 bytes
    glm::vec4 color;     // 16 bytes
};  // sizeof = 48，与 GLSL std430 完全一致
```

### 3.3 SSBO 创建与绑定

```cpp
glGenBuffers(1, &ssbo);
glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
glBufferData(GL_SHADER_STORAGE_BUFFER,
             n * sizeof(Particle),
             init_data.data(),
             GL_DYNAMIC_DRAW);          // GPU 频繁读写
glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);  // binding = 0
```

`GL_DYNAMIC_DRAW` 提示驱动将数据放在 GPU 可快速读写的显存区域。

---

## 4. Compute Shader 工作组设计

### 4.1 工作组层次

OpenGL Compute Shader 的执行单位：

```
Dispatch(X, Y, Z)
  └─ Global invocations: X * local_x × Y * local_y × Z * local_z
       └─ Work group [gx, gy, gz]:
            └─ local invocations: local_x * local_y * local_z
                 └─ gl_GlobalInvocationID = gl_WorkGroupID * local_size + gl_LocalInvocationID
```

### 4.2 为什么选 local_size_x = 256

```glsl
layout(local_size_x = 256) in;
```

选择理由：
1. **Warp/Wavefront 对齐**：NVIDIA GPU 的 warp = 32 线程，256 = 8 warp；AMD 的 wavefront = 64 线程，256 = 4 wavefront。两家都整除。
2. **寄存器占用**：本 shader 寄存器需求约 16-32，256 线程/工作组不会超出 SM 寄存器池
3. **一维粒子数据**：粒子天然是 1D 数据，只需 X 维度

### 4.3 边界保护

```glsl
uint idx = gl_GlobalInvocationID.x;
uint total = uint(particles.length());
if (idx >= total) return;  // 关键：最后一个工作组可能有多余线程
```

调度：`ceil(100000 / 256) = 391` 个工作组，共 `391 × 256 = 100096` 个线程，多出 96 个需要保护。

### 4.4 局部共享内存（本模块未用，但要了解）

对于需要邻居粒子交互的系统（如粒子间排斥力），可用：
```glsl
shared Particle local_cache[256];
local_cache[gl_LocalInvocationIndex] = particles[idx];
barrier();  // 等所有线程写完
// 然后访问 local_cache[j] 做邻居交互
```

---

## 5. GLSL 随机数：原理与哈希函数

### 5.1 为什么没有内置随机数

GLSL 没有 `rand()` 是因为 GPU 着色器是**无状态的确定性函数**：
- 没有全局变量（并行执行，有全局状态会导致竞争）
- 每帧同一 invocation 需要产生**不同**随机数（否则粒子每帧重置到同一位置）

### 5.2 Wang Hash

```glsl
float hash(uint n) {
    n  = (n << 13U) ^ n;
    n  = n * (n * n * 15731U + 789221U) + 1376312589U;
    return float(n & uvec1(0x7fffffffU).x) / float(0x7fffffff);
}
```

原理：通过整数乘法、XOR、移位的非线性组合，使输入的微小变化引起输出的剧烈变化（雪崩效应）。输入为粒子 idx 与时间的组合，保证每帧不同。

### 5.3 更好的选择：PCG Hash

```glsl
uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
```

PCG（Permuted Congruential Generator）在统计质量测试（TestU01）中比 Wang hash 表现更好，均匀性更高，本模块 README 介绍用途，shader 中使用 Wang hash 够用。

### 5.4 多维随机数

一个种子派生多个随机浮点数：
```glsl
uint s0 = idx * 1664525U + uint(uTime * 1000.0) * 22695477U;
float r0 = rand(s0);
float r1 = rand(s0 + 1U);
float r2 = rand(s0 + 2U);
```

`s0 + 1U`、`s0 + 2U` 依然经过哈希，输出独立性良好。

---

## 6. Billboard 技术

### 6.1 为什么不用 gl_PointSize

`gl_PointSize` 是最简单的粒子方案：每个粒子画一个像素点，用 `GL_POINTS` 图元。但有致命缺点：
- `GL_POINT_SIZE_RANGE` 上限通常为 64-128 像素
- 1920×1080 下 64px 粒子视觉上很小
- 不能自定义形状（只能是正方形）

### 6.2 Billboard 原理

Billboard 是一个**始终正对摄像机**的四边形。实现方法：从 View 矩阵提取摄像机的世界空间右向量和上向量。

View 矩阵 `V` 是世界到相机空间的变换：
```
V = [ right.x  right.y  right.z  -dot(right, eye) ]
    [    up.x     up.y     up.z  -dot(up,    eye) ]
    [ -fwd.x  -fwd.y   -fwd.z   dot(fwd,   eye) ]
    [      0       0        0                  1 ]
```

所以：
```glsl
vec3 cam_right = vec3(uView[0][0], uView[1][0], uView[2][0]);  // V 第一列
vec3 cam_up    = vec3(uView[0][1], uView[1][1], uView[2][1]);  // V 第二列
```

四边形顶点展开：
```glsl
vec3 world_pos = p.position.xyz
               + cam_right * offset.x * size
               + cam_up    * offset.y * size;
```

### 6.3 Triangle Strip 四边形

```
顶点 0 (左上) ─── 顶点 2 (右上)
       │  ╲             │
       │    ╲           │
       │      ╲         │
顶点 1 (左下) ─── 顶点 3 (右下)
```

`GL_TRIANGLE_STRIP` 绘制：0-1-2 组成三角形一，1-2-3 组成三角形二，4 个顶点完整覆盖四边形。

N 个粒子 = `N * 4` 个顶点，`gl_VertexID / 4` = 粒子索引，`gl_VertexID % 4` = 角点索引。

### 6.4 退化情况

当摄像机正上方看（`cam_front` 平行于世界 Y 轴）时，`cross(cam_front, world_up)` 趋向零向量，right 向量退化。解决方案：当 `|cam_front.y| > 0.999` 时，改用世界 Z 轴作为辅助方向。

---

## 7. 软粒子（Soft Particles）

### 7.1 硬切割问题

标准粒子在与不透明几何体相交时出现锐利的切割线——粒子四边形被深度测试截断，看起来不自然。

### 7.2 完整软粒子实现（需要深度纹理）

```glsl
uniform sampler2D uDepthTex;  // 绑定场景深度缓冲

float scene_depth = texture(uDepthTex, gl_FragCoord.xy / uResolution).r;
float scene_z = linearize_depth(scene_depth, near, far);
float frag_z  = linearize_depth(gl_FragCoord.z, near, far);
float diff    = scene_z - frag_z;
float soft    = clamp(diff / uSoftRange, 0.0, 1.0);
alpha *= soft;  // 靠近遮挡物时 alpha 趋向 0
```

### 7.3 本模块简化版

本模块未绑定场景深度缓冲，改用圆形粒子边缘渐变模拟软粒子效果：
```glsl
float dist  = dot(d, d) * 4.0;   // 圆形 SDF
float alpha = (1.0 - dist) * vColor.a;
```

---

## 8. 加法混合（Additive Blending）

### 8.1 Alpha 混合 vs 加法混合

| 混合模式 | 公式 | 效果 |
|---|---|---|
| Alpha 混合 | `src * src.a + dst * (1 - src.a)` | 透明叠加，粒子遮挡背景 |
| 加法混合 | `src * 1 + dst * 1` | 纯加法，粒子越多越亮 |

### 8.2 为什么用加法混合

火焰、光效、魔法粒子是**自发光**的：多个发光粒子叠加时应该更亮，而不是只显示最上层。加法混合天然模拟了真实光照的叠加。

```cpp
glBlendFunc(GL_ONE, GL_ONE);  // src * 1 + dst * 1
```

### 8.3 加法混合的顺序无关性

Alpha 混合需要从后向前排序（否则半透明物体遮挡顺序错误），加法混合**不需要排序**（A+B = B+A）。这是 GPU 粒子系统通常选用加法混合的重要原因——排序 10 万粒子的开销是不可接受的。

### 8.4 关闭深度写入

```cpp
glDepthMask(GL_FALSE);  // 粒子不写入深度缓冲
```

原因：加法混合粒子不应该遮挡后方粒子（深度测试仍然开启，粒子被不透明几何体遮挡正常）。

---

## 9. Compute 与渲染管线同步

### 9.1 问题描述

Compute Shader 写入 SSBO，Vertex Shader 读取同一 SSBO。GPU 管线中 Compute 和 Render 是异步的，需要显式同步：

```
Compute Shader 写 SSBO
     ↓
glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)  ← 关键！
     ↓
Vertex Shader 读 SSBO
```

### 9.2 遗漏 barrier 的后果

Vertex Shader 可能读到上一帧的粒子位置（缓存未失效），或者 Compute 尚未完成时就开始渲染，导致粒子位置撕裂/闪烁。

### 9.3 常用 barrier 标志

```cpp
GL_SHADER_STORAGE_BARRIER_BIT   // SSBO 读写同步
GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT  // VBO 同步
GL_TEXTURE_FETCH_BARRIER_BIT    // 纹理读取同步
GL_UNIFORM_BARRIER_BIT          // UBO 同步
GL_ALL_BARRIER_BITS             // 全部（性能差，调试用）
```

### 9.4 与 CPU 同步（glFinish vs glFlush）

- `glFlush()`：将命令队列提交给 GPU，但不等待完成
- `glFinish()`：等待 GPU 全部命令完成（阻塞 CPU，性能极差）
- `glFenceSync()`：精细化等待，可查询 GPU 是否完成特定点

GPU 粒子系统正常运行不需要 `glFinish()`，只需 `glMemoryBarrier()`。

---

## 10. 构建与运行

### 10.1 依赖

- OpenGL 4.6 支持（验证：`GL_ARB_compute_shader`, `GL_ARB_shader_storage_buffer_object`）
- GLFW 3.3+, GLAD（OpenGL 4.6 Core Profile）

### 10.2 构建

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target module15_particles -j$(nproc)
```

### 10.3 运行

```bash
cd build/module15_particles  # 确保 shaders/ 目录在旁边
./module15_particles
```

### 10.4 验证 Compute Shader 支持

```cpp
GLint max_compute_work_group_count[3];
glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_compute_work_group_count[0]);
// 应该 >= 65535 in x dimension
```

---

## 11. 常见坑与调试技巧

### 坑 1：粒子数超过 local_size × groups

```glsl
// 永远要做边界检查
if (idx >= particles.length()) return;
```

### 坑 2：工作组数量超限

`GL_MAX_COMPUTE_WORK_GROUP_COUNT` 在 X 维度通常为 65535。10 万粒子 / 256 = 391 个工作组，安全。但如果粒子数扩展到 1000 万，需要二维 dispatch：
```cpp
// 1000万 / 256 = 39063 工作组（超过 65535 的 z 维 dispatch 不会超）
// 改为二维：如 200 × 196
```

### 坑 3：遗漏 glMemoryBarrier

症状：粒子闪烁、位置错误、部分粒子卡在旧位置。
诊断：在 `glDispatchCompute` 后加 `glFinish()`，如果问题消失，说明是 barrier 问题。

### 坑 4：Billboard 朝向退化

当摄像机从正上方俯视时，`cam_up` 可能指向任意方向，导致粒子旋转跳变。
```glsl
// 当 |cam_front . world_up| > 0.99 时切换辅助向量
vec3 world_up = abs(cam_front.y) < 0.99 ? vec3(0,1,0) : vec3(0,0,1);
```

### 坑 5：加法混合导致白屏

加法混合下，粒子密度过高会使所有像素趋向 (1,1,1)（白色）。
解决：限制粒子的最大亮度（在 frag shader 中 `min(color.rgb, vec3(0.5))`），或减少粒子数。

### 坑 6：SSBO std430 vs C++ 对齐不匹配

如果 C++ 结构体插入了编译器填充字节，SSBO 中的数据偏移会错位。
验证方法：`static_assert(sizeof(Particle) == 48, "Particle size mismatch");`

### 坑 7：Vertex Buffer 忘绑空 VAO

Compute Shader + Billboard 方案完全从 SSBO 读数据，不需要 VBO，但 OpenGL 要求渲染调用时必须有 VAO 绑定：
```cpp
glGenVertexArrays(1, &vao);
// 不需要设置任何顶点属性，只需存在并绑定
glBindVertexArray(vao);
glDrawArrays(...);
```

### 坑 8：粒子全部聚集在原点

检查 Compute Shader 的随机种子是否每帧变化（`uTime` uniform 是否正确更新）。如果粒子死亡后全部重置到同一速度，说明随机函数的输入不含时间参量。

### 调试技巧：粒子数量渐进测试

从 1000 个粒子开始测试，验证正确性后再扩展到 10 万，避免 debug 时被性能问题干扰。

---

*Module 15 — GPU Particle System | ogl_mastery 课程第五阶段*
