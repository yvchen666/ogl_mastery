# Module 03 — GLSL Deep Dive

## 目录

1. [模块目的](#1-模块目的)
2. [架构图：GLSL 数据流](#2-架构图glsl-数据流)
3. [GLSL 完整数据类型表](#3-glsl-完整数据类型表)
4. [限定符详解](#4-限定符详解)
5. [std140 对齐规则完整表格](#5-std140-对齐规则完整表格)
6. [std430 vs std140 差异](#6-std430-vs-std140-差异)
7. [内置函数分类](#7-内置函数分类)
8. [gl_FragCoord 详解](#8-gl_fragcoord-详解)
9. [精度限定符与移动端兼容性](#9-精度限定符与移动端兼容性)
10. [常见坑](#10-常见坑)
11. [构建运行与延伸阅读](#11-构建运行与延伸阅读)

---

## 1. 模块目的

本模块以一个动态演示程序为载体，深入讲解 GLSL（OpenGL Shading Language）的各种特性。

程序展示了：
- **UBO（Uniform Buffer Object）**：用 std140 布局从 CPU 向着色器传递结构化数据（时间、分辨率）
- **gl_FragCoord**：利用硬件提供的片元屏幕坐标实现棋盘格效果
- **时间动画**：sin/cos 函数 + uniform 时间产生循环颜色变化
- **smoothstep**：实现无锯齿边缘渐变，演示 GLSL 数学函数的正确用法

理解 GLSL 的数据类型、限定符、内存布局（尤其是 std140 的 vec3 对齐坑）是避免大量 bug 的关键。

---

## 2. 架构图：GLSL 数据流

```
CPU 端（main.cpp）                           GPU 着色器

                                            ┌─────────────────────────┐
FrameData {                                 │  demo.vert              │
  uTime   = glfwGetTime()                   │                         │
  uAspect = 800.0/600.0     ──UBO──────────►│  layout(std140,         │
  uResX   = 800.0           binding=0       │    binding=0) uniform   │
  uResY   = 600.0                           │    FrameData { ... }    │
}                                           │                         │
                                            │  layout(loc=0) in vec2 aPos
VBO 顶点数据：                               │  layout(loc=1) in vec2 aUV
[-1,-1, 0,0]  ←─ 左下顶点 ─────VAO──────►  │                         │
[ 1,-1, 1,0]  ←─ 右下顶点                  │  out vec2 vUV           │
[ 1, 1, 1,1]  ←─ 右上顶点                  │  out vec2 vScreenPos    │
[-1, 1, 0,1]  ←─ 左上顶点                  │                         │
                                            │  gl_Position = vec4(...)│
EBO 索引：                                   └────────────┬────────────┘
[0,1,2, 0,2,3] ──────────────────────────────────────────┤
                                                          │ 光栅化：插值 vUV、vScreenPos
                                                          ▼
                                            ┌─────────────────────────┐
                                            │  demo.frag              │
                                            │                         │
                                            │  访问 UBO：uTime、uRes  │
                                            │  访问 gl_FragCoord.xy   │
                                            │  访问插值的 vUV         │
                                            │                         │
                                            │  效果1: 棋盘格           │
                                            │    floor(fragCoord/50)  │
                                            │    mod(x+y, 2)          │
                                            │                         │
                                            │  效果2: 时间动画         │
                                            │    sin(uTime + vUV*π)  │
                                            │    cos(uTime*0.7 + ...) │
                                            │                         │
                                            │  效果3: 边缘渐变         │
                                            │    smoothstep(0, 0.02,  │
                                            │      vUV.x) * ...       │
                                            │                         │
                                            │  out vec4 FragColor     │
                                            └────────────┬────────────┘
                                                          │
                                                    帧缓冲（屏幕）
```

---

## 3. GLSL 完整数据类型表

### 3.1 标量类型

| 类型 | 描述 | GPU 存储 |
|------|------|---------|
| `bool` | 布尔值 | 32 位整数（GPU 通常无 1 位布尔） |
| `int` | 32 位有符号整数 | INT32 |
| `uint` | 32 位无符号整数 | UINT32 |
| `float` | 32 位 IEEE 754 浮点 | FP32 |
| `double` | 64 位双精度浮点 | FP64（需要 `GL_ARB_gpu_shader_fp64`） |

### 3.2 向量类型

前缀 `b`=bool, `i`=int, `u`=uint, `d`=double（无前缀=float）

| 类型 | 描述 |
|------|------|
| `vec2`, `vec3`, `vec4` | 2/3/4 分量 float 向量 |
| `ivec2`, `ivec3`, `ivec4` | int 向量 |
| `uvec2`, `uvec3`, `uvec4` | uint 向量 |
| `bvec2`, `bvec3`, `bvec4` | bool 向量 |
| `dvec2`, `dvec3`, `dvec4` | double 向量 |

向量构造函数和分量访问：
```glsl
vec3 v = vec3(1.0, 2.0, 3.0);
float x = v.x;   // = v.r = v.s = v[0]
vec2  xy = v.xy; // swizzle：取 x, y 分量
vec3  zzz = v.zzz; // swizzle：复制 z 三次
vec4  rgba = vec4(v.zyx, 1.0);  // 重排分量并扩展
```

### 3.3 矩阵类型

GLSL 矩阵是**列主序（Column-Major）**存储！

| 类型 | 描述 | 存储 |
|------|------|------|
| `mat2` | 2×2 float 矩阵 | 2 列，每列 vec2 |
| `mat3` | 3×3 float 矩阵 | 3 列，每列 vec3 |
| `mat4` | 4×4 float 矩阵 | 4 列，每列 vec4 |
| `mat2x3` | 2列3行 | 2 列，每列 vec3 |
| `mat3x2` | 3列2行 | 3 列，每列 vec2 |
| `dmat4` | double 版 mat4 | — |

```glsl
mat4 m = mat4(1.0);  // 单位矩阵（对角线为1）

// 列主序：m[col][row]
vec4 col0 = m[0];    // 第 0 列
float m01 = m[0][1]; // 第 0 列，第 1 行（不是 m[0][1] = row0 col1！）

// 矩阵乘法：M * v（向量作为列向量）
vec4 result = m * vec4(1.0, 2.0, 3.0, 1.0);
```

### 3.4 采样器类型

采样器是纹理绑定点的引用，不能有默认值，必须通过 uniform 传入：

| 类型 | 说明 |
|------|------|
| `sampler2D` | 2D 纹理 |
| `sampler3D` | 3D 纹理 |
| `samplerCube` | 立方体贴图 |
| `sampler2DArray` | 2D 纹理数组 |
| `sampler2DShadow` | 2D 深度纹理（PCF 用） |
| `isampler2D` | 整数格式 2D 纹理 |
| `usampler2D` | 无符号整数 2D 纹理 |

```glsl
uniform sampler2D uAlbedo;
uniform sampler2DArray uShadowMaps;

vec4 color = texture(uAlbedo, vUV);  // 采样
float depth = texture(uShadowMaps, vec3(vUV, layer));
```

### 3.5 图像类型（Image）

与采样器不同，图像类型允许随机读写（需要 `imageLoad`/`imageStore`）：

```glsl
layout(rgba8, binding = 0) uniform image2D uImage;
vec4 texel = imageLoad(uImage, ivec2(gl_FragCoord.xy));
imageStore(uImage, ivec2(gl_FragCoord.xy), vec4(1.0));
```

### 3.6 原子计数器

```glsl
layout(binding = 0, offset = 0) uniform atomic_uint uCounter;
uint prev = atomicCounterIncrement(uCounter);
```

---

## 4. 限定符详解

### 4.1 存储限定符

| 限定符 | 适用阶段 | 说明 |
|--------|----------|------|
| `in` | VS（顶点属性）、FS（插值变量）| 输入，只读 |
| `out` | VS（传递给下阶段）、FS（输出颜色） | 输出，只写 |
| `uniform` | 所有阶段 | CPU 传入，每个 draw call 期间不变，只读 |
| `buffer` | 所有阶段（SSBO） | CPU 传入，可读写，运行时大小 |
| `shared` | Compute Shader | 工作组内共享内存 |

### 4.2 插值限定符（用于 varying 变量）

| 限定符 | 说明 | 使用场景 |
|--------|------|---------|
| `smooth`（默认） | 透视正确插值 | 大多数情况（UV、颜色、法线） |
| `noperspective` | 线性插值（屏幕空间） | 某些屏幕空间效果（如 screen-space UV） |
| `flat` | 不插值，使用"驱动顶点"的值 | 整数属性（object ID）、需要不插值的整型数据 |
| `centroid` | 将采样点移到像素中心（MSAA 用） | 防止 MSAA 边缘 UV 超出范围 |

```glsl
// 顶点着色器输出
flat     out int  vObjectID;    // 整数不能插值，必须 flat
smooth   out vec2 vUV;          // 透视正确插值 UV（默认）
noperspective out vec2 vScreenUV; // 屏幕空间线性插值
```

注意：`flat` 变量的值来自图元的"驱动顶点"（provoking vertex），默认是图元的最后一个顶点（`GL_LAST_VERTEX_CONVENTION`）。

### 4.3 layout 限定符

```glsl
// 顶点输入 location
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

// 片元输出 location（对应 draw buffer）
layout(location = 0) out vec4 FragColor;   // GL_COLOR_ATTACHMENT0
layout(location = 1) out vec4 FragNormal;  // GL_COLOR_ATTACHMENT1（MRT 用）

// UBO binding point
layout(std140, binding = 0) uniform FrameData { ... };
layout(std430, binding = 1) buffer ParticleData { Particle particles[]; };

// 图像 binding + 格式
layout(rgba8, binding = 0) uniform image2D uOutput;

// Compute Shader 工作组大小
layout(local_size_x = 16, local_size_y = 16) in;
```

### 4.4 精度限定符（主要用于 GLSL ES / 移动端）

见第 9 节。

---

## 5. std140 对齐规则完整表格

std140 是 UBO 的标准内存布局，保证了不同驱动实现的跨平台一致性（代价是可能有 padding）。

### 5.1 基本规则

**基础对齐（Base Alignment）**：该类型在 std140 中的对齐字节数。

| 类型 | 大小（字节） | 基础对齐（字节） |
|------|------------|----------------|
| `bool`, `int`, `uint`, `float` | 4 | 4 |
| `double` | 8 | 8 |
| `vec2` | 8 | 8 |
| **`vec3`** | **12** | **16（！关键坑）** |
| `vec4` | 16 | 16 |
| `dvec2` | 16 | 16 |
| `dvec3` | 24 | 32 |
| `dvec4` | 32 | 32 |
| `mat2`（作为2个vec2的数组） | 16 | 8（每列）|
| `mat3`（作为3个vec4的数组） | 48 | 16（每列，vec3→vec4）|
| `mat4`（作为4个vec4的数组） | 64 | 16（每列）|

### 5.2 vec3 对齐是 16 字节的原因（最重要的坑）

OpenGL 规范规定：在 std140 中，3 分量向量（vec3/ivec3/uvec3）的基础对齐等于 4 分量向量，即 **16 字节**。

但 vec3 的实际大小是 12 字节。这意味着：**vec3 后面有 4 字节隐含 padding，除非下一个成员正好填满它**。

```glsl
// 着色器中：
layout(std140, binding = 0) uniform BadExample {
    float a;     // 偏移  0，大小 4
    vec3  b;     // 基础对齐 16 → 偏移 16（跳过 4→16，插入 12 字节 padding！）
    float c;     // 偏移 28，大小 4
    // 末尾 padding 到 32 字节（下一个 vec3 对齐到 16）
};  // 总大小：32 字节（不是 4+12+4=20！）
```

对应 C++ 结构体：
```cpp
// 错误写法（以为是紧密排列）：
struct BadExample_Wrong {
    float a;      // 偏移 0
    float b[3];   // 偏移 4  ← 错！std140 的 vec3 偏移是 16
    float c;      // 偏移 16 ← 错！
};  // C++ sizeof = 20，但 std140 期望 32

// 正确写法（手动填充 padding）：
struct BadExample_Correct {
    float a;         // 偏移 0
    float _pad0[3];  // 填充 12 字节，使 b 对齐到 16
    float b[3];      // 偏移 16
    float c;         // 偏移 28
    // 总 32 字节
};

// 更推荐：完全避免 vec3，改用 float + float + float 或 vec4
struct BadExample_Better {
    float a;    // 偏移  0
    float bx;   // 偏移  4
    float by;   // 偏移  8
    float bz;   // 偏移 12
    float c;    // 偏移 16
    // 末尾 padding 到 32（下一个元素对齐要求）
};  // 实际上只需 20 字节有效数据，但如果是最后一个成员，glm 会自动对齐
```

### 5.3 本模块实际使用的结构体

```cpp
// C++ 端：
struct FrameData {
    float uTime;    // 偏移  0，大小 4
    float uAspect;  // 偏移  4，大小 4
    float uResX;    // 偏移  8，大小 4
    float uResY;    // 偏移 12，大小 4
};  // 总大小 16 字节

// GLSL 端（std140）：
layout(std140, binding = 0) uniform FrameData {
    float uTime;      // 偏移  0，对齐 4
    float uAspect;    // 偏移  4，对齐 4
    vec2  uResolution; // 偏移  8，对齐 8（vec2 没有 padding 问题！）
};  // 总大小 16 字节

// 两端完全匹配：C++ sizeof(FrameData) = 16 = GLSL FrameData 的大小
```

这里将 uResX/uResY 合并为 vec2 uResolution（在 GLSL 端）。
C++ 端的内存布局 `[float, float, float, float]` 和 GLSL 端 `[float, float, vec2]` 在 std140 下偏移完全一致（因为 vec2 基础对齐 8，已经自然对齐到偏移 8）。

### 5.4 数组规则

**std140 中，数组的每个元素被向上对齐到 16 字节（即使元素本身更小）**：

```glsl
layout(std140) uniform Lights {
    vec4  positions[16];  // 每个 vec4 = 16 字节，没问题
    float intensities[16]; // 每个 float 在 std140 中占 16 字节（不是 4！）
    // 即 intensities[0] 在偏移 0，intensities[1] 在偏移 16，...
};
// C++ 端不能直接用 float[16]，每个元素要填充到 16 字节！
```

结构体数组：
```glsl
struct Light { vec3 pos; float intensity; };
layout(std140) uniform LightBlock {
    Light lights[8];
    // Light.pos:       偏移 0（vec3，基础对齐 16）
    // Light.intensity: 偏移 12（float，塞进 vec3 的 padding 里，因为 vec3 大小12，对齐16）
    // Light 总大小：16 字节（刚好 16 对齐）
    // lights[0] 偏移 0，lights[1] 偏移 16，...
};
```

---

## 6. std430 vs std140 差异

std430 是 SSBO（Shader Storage Buffer Object）的默认布局，也可用于 UBO（4.3+）。

### 6.1 主要差异

| 规则 | std140 | std430 |
|------|--------|--------|
| vec3 对齐 | 16 字节 | **12 字节**（按实际大小）|
| float/int 数组元素 | 16 字节（强制对齐到 16）| 4 字节（按实际大小）|
| vec2 数组元素 | 16 字节 | 8 字节 |
| struct 成员 | 对齐到 16 | 对齐到成员自然对齐 |

### 6.2 std430 的 vec3 对齐（依然有坑）

在 std430 中，vec3 的基础对齐是 **4 字节的倍数取 4 倍** = 12 字节（这是 std430 改进的地方）。

但 vec3 的大小仍然是 12 字节，而下一个 float 成员可以紧接在 offset+12 处。这使得 std430 在很多情况下与 C++ 的自然布局更接近。

```glsl
// std430（SSBO）：
layout(std430, binding = 1) buffer ParticleData {
    // 假设一个粒子：position(vec3) + lifetime(float) + velocity(vec3) + mass(float)
    // std430: pos@0(12B) + life@12(4B) + vel@16(12B) + mass@28(4B) = 32B/粒子
    // std140: pos@0(16B) + life@16(4B)+padding(12B) + vel@32(16B) + mass@48(4B)+pad(12B) = 64B/粒子!
};
```

即使是 std430，建议也避免裸 vec3，改用 vec4 最为稳妥。

### 6.3 何时用哪种

- **UBO（uniform block）**：必须用 std140（或 shared、packed，但后两者不可移植）
- **SSBO（buffer block）**：默认 std430，也可指定 std140
- **Compute Shader 的 shared 内存**：不需要布局限定符，由编译器决定

---

## 7. 内置函数分类

### 7.1 几何函数

```glsl
float d = dot(a, b);         // 点积：a·b = |a||b|cos(θ)
vec3  c = cross(a, b);       // 叉积：方向垂直于 a 和 b（右手定则）
vec3  n = normalize(v);      // 归一化：v / length(v)
float l = length(v);         // 向量长度：sqrt(dot(v,v))
float d = distance(p1, p2);  // 两点距离：length(p2-p1)

// 反射：入射向量 i，法线 n（归一化），反射方向
vec3 r = reflect(i, n);   // = i - 2*dot(n,i)*n

// 折射：入射向量 i，法线 n，折射率比 eta = n1/n2
// 当 eta*sin(θ) > 1 时（全反射），结果为 vec3(0)
vec3 t = refract(i, n, eta);

// 面朝向修正：确保法线朝向与 i 相同侧
vec3 fn = faceforward(n, i, nref);  // dot(nref, i) < 0 ? n : -n
```

### 7.2 数学函数

```glsl
// 三角函数（参数单位：弧度）
float s = sin(angle);
float c = cos(angle);
float t = tan(angle);
float a = asin(x);    // 反正弦，范围 [-π/2, π/2]
float a = atan(y, x); // 二参数反正切，范围 (-π, π]

// 指数/对数
float e = exp(x);    // e^x
float l = log(x);    // ln(x)，x>0
float p = pow(x, y); // x^y，x 必须 >= 0（否则未定义）
float s = sqrt(x);   // √x
float r = inversesqrt(x);  // 1/√x（通常硬件指令，比 1.0/sqrt(x) 快）

// 取整/截断
float f = floor(x);  // 向下取整
float c = ceil(x);   // 向上取整
float r = round(x);  // 四舍五入
float t = trunc(x);  // 截断（向零取整）
float f = fract(x);  // 小数部分：x - floor(x)，范围 [0, 1)

// 范围/插值
float c = clamp(x, 0.0, 1.0);  // 将 x 限制在 [min, max]
float m = mix(a, b, t);         // 线性插值：a*(1-t) + b*t
float s = step(edge, x);        // x < edge ? 0.0 : 1.0（阶跃函数）

// smoothstep：Hermite 插值，比 step 更平滑
// smoothstep(edge0, edge1, x)：
//   t = clamp((x-edge0)/(edge1-edge0), 0, 1)
//   return t*t*(3 - 2*t)  ← 三次 Hermite
float ss = smoothstep(0.0, 1.0, x);  // 在 [0,1] 做平滑过渡

// 绝对值/符号
float a = abs(x);
float s = sign(x);  // -1, 0, 或 1

// 最大/最小
float mx = max(a, b);
float mn = min(a, b);

// 取模（行为与 C++ 的 fmod 不同：结果与 y 同号）
float m = mod(x, y);  // x - y*floor(x/y)
```

### 7.3 纹理函数

```glsl
// 基础采样（自动 mipmap 选择，基于导数）
vec4 c = texture(sampler2D s, vec2 uv);

// 指定 LOD 级别采样（无导数环境可用，如 Vertex Shader）
vec4 c = textureLod(sampler2D s, vec2 uv, float lod);

// 带偏移的采样（用于 PCF 等）
vec4 c = textureOffset(sampler2D s, vec2 uv, ivec2 offset);

// 查询纹理尺寸
ivec2 sz = textureSize(sampler2D s, int lod);  // 指定 mip 级别的尺寸

// 手动 LOD 偏置
vec4 c = texture(sampler2D s, vec2 uv, float bias);  // bias 加到计算出的 LOD
```

### 7.4 导数函数（Fragment Shader 专用）

```glsl
// dFdx(p)：p 在屏幕 x 方向（水平）的偏导数
// dFdy(p)：p 在屏幕 y 方向（垂直）的偏导数
// fwidth(p)：abs(dFdx(p)) + abs(dFdy(p))，用于 anti-aliasing

// 实现：GPU 以 2x2 像素为一个"quad"同时计算，用相邻像素的差值估计导数
// 因此：
//   1. 导数的精度是"每像素"，不是"连续的"
//   2. 在 discard 之后，quad 中某些线程已提前退出，导数结果未定义
//   3. 在流程控制（if/for）的非均匀分支内，导数未定义

float dx = dFdx(vUV.x);   // UV 在屏幕 x 方向的变化率（用于 mipmap 选择等）
float dy = dFdy(vUV.y);
float fw = fwidth(vUV.x); // 用于 SDF font 渲染的边缘反走样

// 精细版（GL_ARB_derivative_control / OpenGL 4.5+）
float dx_fine   = dFdxFine(x);    // 更精确，但更耗时（差分范围更小）
float dx_coarse = dFdxCoarse(x);  // 更快，但精度低（差分范围更大）
```

### 7.5 原子操作（Compute/Fragment Shader）

```glsl
// 用于 image 或 SSBO 的原子操作（防止多线程竞争）
uint old = atomicAdd(buffer_var, value);
uint old = atomicMin(buffer_var, value);
uint old = atomicMax(buffer_var, value);
uint old = atomicAnd(buffer_var, value);
uint old = atomicOr(buffer_var, value);
uint old = atomicXor(buffer_var, value);
uint old = atomicExchange(buffer_var, value);
uint old = atomicCompSwap(buffer_var, compare, value);  // CAS
```

---

## 8. gl_FragCoord 详解

### 8.1 基本属性

`gl_FragCoord` 是片元着色器的内置输入变量，类型为 `vec4`：

```
gl_FragCoord.x  : 片元中心的窗口 x 坐标（像素单位）
gl_FragCoord.y  : 片元中心的窗口 y 坐标（像素单位）
gl_FragCoord.z  : 片元的深度值，范围 [0.0, 1.0]（映射到 glDepthRange 指定的范围）
gl_FragCoord.w  : 1 / clip_w（裁剪空间 w 分量的倒数，用于透视正确插值）
```

### 8.2 坐标原点

**OpenGL 默认：左下角为原点，y 轴向上**

```
窗口坐标（800×600）：
  (0.5, 0.5)     — 左下角像素中心
  (799.5, 0.5)   — 右下角像素中心
  (0.5, 599.5)   — 左上角像素中心
  (799.5, 599.5) — 右上角像素中心
```

注意：坐标是像素**中心**，所以是 0.5 而不是 0.0。

可以通过 `glClipControl(GL_UPPER_LEFT, ...)` 改为左上角原点（D3D 风格），但这会影响其他坐标约定，需谨慎。

### 8.3 本模块中的使用

```glsl
// 棋盘格效果：
vec2 checker = floor(gl_FragCoord.xy / 50.0);
// 对于 800×600 窗口：
//   像素 (25, 25) → checker = (0, 0) → 偶数格
//   像素 (75, 25) → checker = (1, 0) → 奇数格
//   像素 (75, 75) → checker = (1, 1) → 偶数格
float c = mod(checker.x + checker.y, 2.0);  // 0.0 或 1.0
```

### 8.4 gl_FragCoord.z 的精度分布

由于透视非线性，`gl_FragCoord.z` 的精度集中在近平面附近：

```
Z 值 0.0  → 近平面（near）
Z 值 1.0  → 远平面（far）

精度分布：
  [0.0, 0.01]  约占 near~2*near 的范围（50% 的精度）
  [0.99, 1.0]  约占 far/2~far 的范围（50% 的精度）

实际上对于 near=0.1, far=1000：
  Z 在 [0, 0.5] 范围对应 eye-z 在 [0.1, 0.2]（仅 0.1m 的世界距离）
  Z 在 [0.5, 1.0] 范围对应 eye-z 在 [0.2, 1000]（999.8m！）
```

这就是 Z-fighting 的根本原因，也是 Reverse-Z 的动机（详见 module01）。

---

## 9. 精度限定符与移动端兼容性

### 9.1 精度限定符

GLSL ES（用于 OpenGL ES / WebGL / 移动端）要求显式指定精度：

| 限定符 | float 范围 | float 精度 | int 范围 |
|--------|-----------|-----------|---------|
| `highp` | [-2^62, 2^62] | 相对精度 2^-16 | 32 位 |
| `mediump` | [-2^14, 2^14] | 相对精度 2^-10 | 16 位 |
| `lowp` | [-2, 2] | 绝对精度 2^-8 | 10 位 |

```glsl
// GLSL ES 需要指定默认精度：
precision highp float;   // 所有 float 默认 highp
precision mediump int;   // 所有 int 默认 mediump

// 或单独指定某个变量：
mediump vec3 color;
highp   vec2 uv;
lowp    float alpha;
```

### 9.2 桌面 OpenGL 的精度

桌面 OpenGL（非 ES）中，精度限定符**被接受但被忽略**（所有操作都以 32 位浮点执行）。

因此写桌面 OpenGL 着色器时，可以不写精度限定符。但如果打算同时兼容 WebGL/ES，应加上：

```glsl
#ifdef GL_ES
  precision highp float;
  precision highp int;
#endif
```

### 9.3 移动端优化建议

移动 GPU（Qualcomm Adreno、ARM Mali、Apple GPU）对精度敏感：

- `highp` 在某些 GPU 上（旧款 Mali）会使用软件模拟，比 `mediump` 慢 2-4 倍
- UV 坐标通常用 `mediump` 足够
- 光照计算可能需要 `highp`（法线归一化精度敏感）
- 颜色最终输出通常 `lowp` 足够（输出缓冲通常是 8 位/通道）

```glsl
// 移动端友好写法：
varying mediump vec2 vUV;       // UV 不需要高精度
varying highp   vec3 vNormal;   // 法线需要高精度
varying mediump vec3 vColor;    // 颜色 mediump 足够
```

---

## 10. 常见坑

### 坑 1：std140 vec3 对齐（最常见！）

这是 OpenGL UBO 最臭名昭著的问题：

```cpp
// 着色器：
layout(std140, binding = 0) uniform Material {
    vec3  color;      // 偏移 0，但基础对齐 16！
    float roughness;  // 偏移 12（vec3 大小=12，下一个 float 可以塞进去）
    // 实际上：vec3 占 0-11，roughness 占 12-15，总 16 字节（巧合对齐！）
    // 但如果是：
    float metallic;   // 假设这是第一个
    vec3  color;      // 偏移 16（跳过 4→16，12 字节 padding！）
};

// C++ 端直接这样写是错的：
struct Material {
    float metallic;  // 4 字节
    float color[3];  // 12 字节，紧跟着（偏移 4）
    // 但 std140 的 color（vec3）偏移是 16！
};
```

**解决方案：永远不要在 UBO 中使用裸 `vec3`，改用 `vec4`（浪费 4 字节但无 bug）。**

```glsl
// 安全写法：
layout(std140, binding = 0) uniform Material {
    vec4  color_and_roughness;  // xyz=color, w=roughness，完全对齐
    vec4  other_params;         // x=metallic, yzw=保留
};
```

### 坑 2：sampler 绑定点混乱

```cpp
// 着色器中：
uniform sampler2D uAlbedo;    // binding 通过 glUniform1i 设置
uniform sampler2D uNormal;

// C++ 端：
glUseProgram(program);
glUniform1i(glGetUniformLocation(program, "uAlbedo"), 0);  // texture unit 0
glUniform1i(glGetUniformLocation(program, "uNormal"), 1);  // texture unit 1

// 绑定纹理到对应的 texture unit：
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, albedo_tex);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, normal_tex);
```

更现代的做法（OpenGL 4.2+，`GL_ARB_shading_language_420pack`）：
```glsl
layout(binding = 0) uniform sampler2D uAlbedo;  // 直接指定 texture unit，无需 glUniform1i
layout(binding = 1) uniform sampler2D uNormal;
```

### 坑 3：flat varying 用于整数

```glsl
// 错误：整数不能被插值，必须 flat
out int vObjectID;     // 编译错误或未定义行为！

// 正确：
flat out int vObjectID;  // flat = 不插值，使用驱动顶点的值
```

注意：`flat` 的"驱动顶点"默认是图元的最后一个顶点（可通过 `glProvokingVertex` 修改）。

### 坑 4：discard 和 Early Z 的性能影响

```glsl
void main() {
    vec4 color = texture(uAlbedo, vUV);
    if (color.a < 0.5) discard;  // Alpha Test
    // 后续着色...
}
```

使用 `discard` 会禁用 Early Z（驱动无法提前丢弃片元），因为深度写入只能在着色器执行完后（知道是否 discard）才能确定。

解决：将需要 `discard` 的物体单独 pass 渲染，其他不透明物体照常受益于 Early Z。

### 坑 5：gl_FragCoord 原点

桌面 OpenGL：左下角为原点，y 向上。
WebGL/OpenGL ES：同上。
Direct3D/Vulkan：左上角为原点，y 向下。

如果将 Shadertoy 的着色器搬到 OpenGL，通常需要翻转 y：
```glsl
vec2 fragCoord = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
```

### 坑 6：dFdx/dFdy 在 discard 后未定义

```glsl
void main() {
    if (someCondition) discard;  // 提前退出

    // 问题：如果同一 2x2 quad 中的某些线程 discard 了，
    // 剩余线程调用 dFdx 时，相邻线程已不存在，结果未定义！
    float lod = textureQueryLod(uTex, vUV).x;  // 内部使用导数
    vec4 c = texture(uTex, vUV);  // 内部也使用导数（mipmap 选择）
}
```

解决：在 `discard` 之前完成所有需要导数的纹理采样，或使用 `textureLod` 手动指定 LOD。

---

## 11. 构建运行与延伸阅读

### 构建

```bash
# 在 ogl_mastery 根目录：
CXX=g++-10 CC=gcc-10 cmake -B build && cmake --build build -j$(nproc)

# 运行（从 build 目录）：
cd build && ./module03_glsl_deep
```

预期结果：800×600 窗口，显示一个动态效果矩形：
- 棋盘格背景（50像素格子）
- 内部颜色随时间缓慢变换（sin/cos 动画）
- 矩形边缘有 2% 宽度的平滑渐变
按 ESC 退出。

### 验证 std140 布局工具

使用 `glGetActiveUniformBlockiv` 可以查询着色器中 UBO 成员的实际偏移：

```cpp
GLuint block_idx = glGetUniformBlockIndex(program, "FrameData");
GLint num_uniforms;
glGetActiveUniformBlockiv(program, block_idx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &num_uniforms);

std::vector<GLint> uniform_indices(num_uniforms);
glGetActiveUniformBlockiv(program, block_idx,
    GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, uniform_indices.data());

for (int i = 0; i < num_uniforms; ++i) {
    GLuint idx = uniform_indices[i];
    GLint offset;
    glGetActiveUniformsiv(program, 1, &idx, GL_UNIFORM_OFFSET, &offset);
    // 打印 offset，验证与 C++ 结构体匹配
}
```

这是调试 std140 对齐问题的最可靠方法。

### 延伸阅读

- **GLSL 4.60 规范（官方）**
  https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf
  完整的语言参考，第 4 章（Variables and Types）和第 7 章（Built-in Variables）是本模块的对应内容

- **OpenGL Wiki: Interface Block**
  https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)
  UBO/SSBO 的详细说明，包括 std140/std430 布局规则

- **OpenGL Wiki: Uniform Buffer Object**
  https://www.khronos.org/opengl/wiki/Uniform_Buffer_Object
  UBO 的使用指南

- **Shadertoy** — https://www.shadertoy.com/
  基于片元着色器的艺术/技术演示集合，本模块的 demo.frag 受 Shadertoy 风格启发
  注意：Shadertoy 使用 `iTime`、`iResolution`、`fragCoord` 等 uniform 名，y 轴需注意

- **The Book of Shaders** — https://thebookofshaders.com/
  系统讲解 GLSL 数学（sin/cos/step/smoothstep），非常适合本模块内容

- **glm（OpenGL Mathematics）**
  https://github.com/g-truc/glm
  C++ 矩阵向量库，与 GLSL 接口完全对应，处理 std140 时用 `glm::vec4` 替代 `glm::vec3`

---

*本模块配套代码：`src/main.cpp`（主程序）, `src/ubo_helper.cpp`（UBO 封装），`shaders/demo.vert/.frag`（演示着色器）。*
*构建：`CXX=g++-10 CC=gcc-10 cmake -B build && cmake --build build -j$(nproc)`*
