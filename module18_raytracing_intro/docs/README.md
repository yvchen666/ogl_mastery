# Module 18 — 实时路径追踪入门

## 目录

1. [概述](#1-概述)
2. [光线追踪 vs 光栅化的根本区别](#2-光线追踪-vs-光栅化的根本区别)
3. [光线方程与光线-球体相交](#3-光线方程与光线-球体相交)
4. [光线-三角形相交：Möller-Trumbore 算法](#4-光线-三角形相交möller-trumbore-算法)
5. [BVH：包围盒层次树](#5-bvh包围盒层次树)
6. [蒙特卡洛积分与重要性采样](#6-蒙特卡洛积分与重要性采样)
7. [材质模型：Lambertian / Metal / Glass](#7-材质模型lambertian--metal--glass)
8. [时间累积降噪](#8-时间累积降噪)
9. [PCG 随机数](#9-pcg-随机数)
10. [实现细节与构建](#10-实现细节与构建)
11. [常见坑与调试技巧](#11-常见坑与调试技巧)

---

## 1. 概述

本模块实现一个完整的 **GPU 路径追踪器**（Compute Shader），功能：

- **场景**：地面 + 6 个球体，包含漫反射、金属、玻璃三种材质
- **Compute Shader 路径追踪**：每帧每像素 1 spp（sample per pixel）
- **时间累积降噪**：等权平均，帧数越多噪点越少，自动收敛
- **ACES Filmic Tone Mapping + Gamma 矫正**：正确的 HDR 显示

### 控制方式

| 操作 | 功能 |
|---|---|
| 右键拖拽 | 旋转摄像机（重置累积）|
| W/A/S/D | 摄像机移动（重置累积）|
| R | 手动重置累积缓冲 |
| ESC | 退出 |

### 效果说明

程序启动后每帧增加一个 spp。噪点会随帧数增加逐渐消失：
- 1 spp：极度噪点
- 64 spp：基本可辨认
- 256 spp：较为干净
- 1024+ spp：收敛

---

## 2. 光线追踪 vs 光栅化的根本区别

### 2.1 光栅化（本课程前 14 个模块）

```
顶点 → 变换到屏幕空间 → 光栅化（插值）→ 片段着色
   ↑ 以几何图元为中心（从三角形出发，找覆盖的像素）
```

光栅化的局限：
- **全局光照**：阴影需要 Shadow Map，环境光遮蔽需要 SSAO，反射需要 Screen Space Reflection
- 这些都是近似，都有各自的伪影（透视走样、视角限制等）

### 2.2 光线追踪（本模块）

```
像素 → 发射射线 → 场景求交 → 递归追踪（反射/折射/漫射）
   ↑ 以像素为中心（从摄像机出发，找哪个几何体被击中）
```

光线追踪的特点：
- **全局光照天然正确**：阴影、反射、折射都是物理准确的
- **计算量巨大**：每像素需要发射多条射线，每条射线可能递归多次
- 现代 GPU 有 RT Core（NVIDIA RTX）专门加速 BVH 遍历和射线-三角形求交

### 2.3 路径追踪 vs 简单光线追踪

**光线追踪**（Whitted-style Ray Tracing）：只追踪镜面反射/折射的确定性路径
**路径追踪**（Path Tracing）：在每个交点随机选择散射方向，蒙特卡洛求解渲染方程

渲染方程（James Kajiya, 1986）：
```
Lo(x, ω_o) = Le(x, ω_o)
           + ∫_{Ω} f_r(x, ω_i, ω_o) * Li(x, ω_i) * cos(θ_i) dω_i
```

- `Lo`：出射辐射度
- `Le`：自发光
- `f_r`：BRDF（双向反射分布函数）
- `Li`：入射辐射度
- `cos(θ_i)`：几何衰减因子

路径追踪通过蒙特卡洛积分近似求解这个积分。

---

## 3. 光线方程与光线-球体相交

### 3.1 光线参数方程

```
Ray(t) = O + t * D

O = 原点（Origin）
D = 方向（Direction，单位向量）
t > 0 → 光线正方向
```

### 3.2 光线-球体相交（解析解推导）

球体方程：`|P - C|² = r²`（所有距离球心 C 为 r 的点 P）

将光线代入球体方程：
```
|O + tD - C|² = r²

令 oc = O - C：
|oc + tD|² = r²

(oc + tD)·(oc + tD) = r²

dot(D,D)*t² + 2*dot(oc,D)*t + (dot(oc,oc) - r²) = 0
```

这是标准二次方程 `at² + bt + c = 0`：
```
a = dot(D, D)               （若 D 是单位向量，则 a=1）
b = 2 * dot(oc, D)
c = dot(oc, oc) - r²

判别式 Δ = b² - 4ac
```

- `Δ < 0`：光线与球不相交
- `Δ = 0`：光线切球（一个交点）
- `Δ > 0`：光线穿球（两个交点，取较小正值）

### 3.3 优化：使用 half-b

令 `half_b = b/2 = dot(oc, D)`，则：
```
t = (-half_b ± sqrt(half_b² - a*c)) / a
```

避免乘以 2 再除以 2，减少浮点运算。

### 3.4 GLSL 实现

```glsl
bool hit_sphere_obj(Ray r, Sphere s, float t_min, float t_max, out HitRecord rec) {
    vec3  oc = r.origin - s.center_radius.xyz;
    float radius = s.center_radius.w;

    float a      = dot(r.dir, r.dir);
    float half_b = dot(oc, r.dir);
    float c      = dot(oc, oc) - radius * radius;
    float disc   = half_b * half_b - a * c;

    if (disc < 0.0) return false;

    float sqrt_disc = sqrt(disc);
    // 先尝试近端交点
    float root = (-half_b - sqrt_disc) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrt_disc) / a;  // 再尝试远端
        if (root < t_min || root > t_max) return false;
    }
    // ...
}
```

### 3.5 法线方向（Front Face）

光线可以从球的外部（正面）或内部（背面）射入：
```glsl
vec3  outward_normal = (hit_pos - center) / radius;
bool  front_face     = dot(r.dir, outward_normal) < 0.0;
vec3  normal         = front_face ? outward_normal : -outward_normal;
```

始终保持法线与射线方向相反，用 `front_face` 标志记录哪侧。这对折射（玻璃材质）至关重要。

---

## 4. 光线-三角形相交：Möller-Trumbore 算法

本模块场景只有球体，但实际渲染器需要三角形求交。以下是完整推导。

### 4.1 重心坐标参数化三角形

三角形顶点 V0, V1, V2，三角形上任意点 P：
```
P = (1-u-v)*V0 + u*V1 + v*V2    其中 u≥0, v≥0, u+v≤1
```

u, v 是重心坐标（barycentric coordinates）。

### 4.2 光线方程代入

光线 `O + t*D = (1-u-v)*V0 + u*V1 + v*V2`

整理为线性系统（求 t, u, v）：
```
O - V0 = -t*D + u*(V1-V0) + v*(V2-V0)

令：
  e1 = V1 - V0
  e2 = V2 - V0
  h  = D × e2        （叉积）
  det = dot(e1, h)

  if |det| < ε → 光线平行于三角形，无交

  f = 1/det
  s = O - V0
  u = f * dot(s, h)

  if u < 0 or u > 1 → 交点在三角形外

  q = s × e1
  v = f * dot(D, q)

  if v < 0 or u+v > 1 → 交点在三角形外

  t = f * dot(e2, q)
```

### 4.3 代码实现

```glsl
bool hit_triangle(Ray r, vec3 V0, vec3 V1, vec3 V2, out float t, out vec2 uv) {
    vec3  e1  = V1 - V0;
    vec3  e2  = V2 - V0;
    vec3  h   = cross(r.dir, e2);
    float det = dot(e1, h);

    if (abs(det) < 1e-6) return false;

    float f = 1.0 / det;
    vec3  s = r.origin - V0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;

    vec3  q = cross(s, e1);
    float v = f * dot(r.dir, q);
    if (v < 0.0 || u + v > 1.0) return false;

    t = f * dot(e2, q);
    if (t < 1e-4) return false;

    uv = vec2(u, v);
    return true;
}
```

---

## 5. BVH：包围盒层次树

### 5.1 为什么需要 BVH

本模块场景有 6 个球体，线性搜索（O(N)）完全够用。
但真实场景通常有数百万三角形，O(N) 是不可接受的。

**BVH（Bounding Volume Hierarchy）** 将几何体组织成层次树，每个节点有一个 AABB（轴对齐包围盒）。光线先与 AABB 求交（廉价），若命中再递归检查子节点。

### 5.2 BVH 构建

```
1. 从根节点开始，包含所有图元
2. 计算 AABB
3. 沿最长轴，找一个分割点，将图元分为两半
4. 递归对左右子树重复，直到每个叶节点图元数 ≤ 阈值（如 4）
```

### 5.3 SAH（Surface Area Heuristic）最优分割

暴力二分（中点法）不总是最优。SAH 最小化期望求交代价：

```
Cost(split) = C_trav + (A_L/A_parent) * N_L * C_isect
                     + (A_R/A_parent) * N_R * C_isect
```

- `C_trav`：遍历一个节点的代价（约 1）
- `C_isect`：对一个图元求交的代价（约 4-8）
- `A_L, A_R`：左右子树 AABB 的表面积
- `N_L, N_R`：左右子树的图元数

遍历每个候选分割轴（X/Y/Z）和位置，选代价最小的。

### 5.4 GPU 上的 BVH 遍历

GPU 不支持递归（栈深度有限），需要用**显式栈**：

```glsl
// 用 SSBO 模拟遍历栈（不能用 local 数组太大）
layout(std430, binding = 2) buffer StackBuffer { int stack[]; };

// 迭代遍历
int stack_top = 0;
stack[stack_top++] = 0;  // 根节点

while (stack_top > 0) {
    int node_idx = stack[--stack_top];
    BVHNode node = bvh_nodes[node_idx];

    if (!hit_aabb(r, node.aabb)) continue;

    if (node.is_leaf) {
        // 对 node.primitives 逐一求交
        for (int i = node.prim_start; i < node.prim_end; ++i)
            test_triangle(r, primitives[i], rec);
    } else {
        stack[stack_top++] = node.left;
        stack[stack_top++] = node.right;
    }
}
```

### 5.5 AABB 光线求交（Slab Method）

```glsl
bool hit_aabb(Ray r, vec3 box_min, vec3 box_max) {
    vec3 inv_dir = 1.0 / r.dir;
    vec3 t_min = (box_min - r.origin) * inv_dir;
    vec3 t_max = (box_max - r.origin) * inv_dir;
    vec3 t1 = min(t_min, t_max);
    vec3 t2 = max(t_min, t_max);
    float t_near = max(max(t1.x, t1.y), t1.z);
    float t_far  = min(min(t2.x, t2.y), t2.z);
    return t_far >= t_near && t_far > 0.0;
}
```

---

## 6. 蒙特卡洛积分与重要性采样

### 6.1 蒙特卡洛积分基础

对函数 `f(x)` 在域 Ω 上的积分，用 N 个随机样本近似：

```
∫_Ω f(x) dx ≈ (1/N) * Σ_{i=1}^{N} f(X_i) / p(X_i)
```

其中 `p(X_i)` 是采样分布的概率密度函数（PDF）。

路径追踪中：
- 积分域 Ω：半球上的方向集合
- 被积函数：`f_r * Li * cosθ`（BRDF × 入射辐射度 × 几何因子）
- 每次递归选一个随机方向代表整个半球积分

### 6.2 均匀半球采样

最简单：在半球上均匀随机采样，每个方向的 PDF = `1/(2π)`：
```glsl
vec3 rand_hemisphere(vec3 normal, inout uint seed) {
    vec3 v = rand_unit_sphere(seed);
    return dot(v, normal) > 0.0 ? v : -v;
}
```

### 6.3 余弦加权采样（Lambertian 重要性采样）

Lambertian BRDF = `albedo / π`，渲染方程贡献 = `(albedo/π) * Li * cosθ`

将 PDF 选为 `p(θ) = cosθ/π`（余弦加权），使采样分布与被积函数形状匹配：

```
E[f/p] = E[(albedo/π * Li * cosθ) / (cosθ/π)] = E[albedo * Li]
```

cosθ 因子被消除，方差降低。对于漫反射表面，余弦加权采样 variance 比均匀采样小约 3 倍。

逆 CDF 采样（球面坐标）：
```
θ = arccos(sqrt(1 - r1))   （r1 ∈ [0,1) 均匀随机）
φ = 2π * r2
```

本模块实现中，`rand_unit_sphere + 法线方向` 的组合近似了余弦加权分布。

### 6.4 方差与收敛

蒙特卡洛误差：`error ∝ σ/√N`

- 误差随 `√N` 衰减（噪点要减半，需要 4x 的 spp）
- 重要性采样降低 σ，等效提升收敛速度

---

## 7. 材质模型：Lambertian / Metal / Glass

### 7.1 Lambertian 漫反射

物理基础：完全漫射表面，各方向辐射度相等（朗伯特余弦定律）。

```glsl
Ray scatter_lambertian(Ray r_in, HitRecord rec, inout uint seed) {
    vec3 scatter_dir = rec.normal + rand_unit_sphere(seed);
    if (length(scatter_dir) < 1e-5) scatter_dir = rec.normal;
    return Ray(rec.pos, normalize(scatter_dir));
}
```

能量衰减：`throughput *= albedo`（每次弹射乘以材质颜色）

### 7.2 金属反射

完美镜面反射 + 粗糙度（fuzz）：

```
反射向量：r = d - 2*(d·n)*n

GLSL 内置：reflected = reflect(normalize(r_in.dir), rec.normal)
粗糙扰动：fuzz_dir = reflected + roughness * rand_unit_sphere(seed)
```

`roughness = 0`：完美镜面；`roughness = 1`：近似漫反射

### 7.3 玻璃折射（Snell 定律推导）

Snell 定律：`n1 * sin(θ1) = n2 * sin(θ2)`

其中：
- `n1`：入射介质折射率（真空/空气 ≈ 1.0）
- `n2`：折射介质折射率（玻璃 ≈ 1.5，水 ≈ 1.33）
- `θ1`：入射角，`θ2`：折射角

当 `n1/n2 * sin(θ1) > 1` 时（全内反射），折射不存在，只有反射。

**Schlick 菲涅耳近似**（Christophe Schlick, 1994）：

真实的菲涅耳方程计算复杂。Schlick 近似：
```
R(θ) ≈ R0 + (1 - R0) * (1 - cosθ)^5
R0 = ((n1 - n2) / (n1 + n2))^2
```

从正面看（θ≈0）：反射率最低；掠射角（θ≈90°）：反射率趋向 1。

GLSL 实现（已内置 `refract` 函数）：
```glsl
// GLSL refract(I, N, eta) 执行 Snell 定律
// I: 入射方向，N: 法线，eta: n1/n2
out_dir = refract(unit_dir, rec.normal, refraction_ratio);
```

---

## 8. 时间累积降噪

### 8.1 等权平均

每帧增加 1 spp，累积到等权平均：

```
E_1 = sample_1
E_2 = (sample_1 + sample_2) / 2
E_N = (E_{N-1} * (N-1) + sample_N) / N
    = mix(E_{N-1}, sample_N, 1/N)
```

GLSL 实现：
```glsl
vec4 prev = imageLoad(uAccumTex, pixel);
float w   = 1.0 / float(uFrameCount + 1);
imageStore(uAccumTex, pixel, mix(prev, vec4(new_color, 1.0), w));
```

`uFrameCount` 从 0 开始，第 1 帧时 `w = 1/1 = 1`（完全使用新样本），第 2 帧 `w = 1/2`（等权平均）。

### 8.2 为什么等权平均

等权平均是无偏估计量：`E[E_N] = E[f(X)]`（真实积分值）

理论上，当 N → ∞ 时，噪点趋向 0，图像收敛到完美的全局光照。

### 8.3 vs TAA（指数移动平均）

TAA（Temporal Anti-Aliasing）使用指数移动平均：
```
E_N = α * E_{N-1} + (1-α) * sample_N      （α ≈ 0.9）
```

- 优点：摄像机移动时迅速适应（老样本权重衰减快）
- 缺点：有偏（无法收敛到完美图像）、运动物体有重影（ghost）

等权平均用于离线渲染（收敛为目标），TAA 用于实时渲染（每帧新内容）。

### 8.4 运动时重置

当摄像机移动时，之前的累积帧与当前视角不对应，必须重置：
```cpp
if (camera_moved) {
    g_frame_count = 0;
    // 不需要显式清除纹理，因为 w=1 时新样本直接覆盖
}
```

---

## 9. PCG 随机数

### 9.1 GPU 随机数的挑战

GPU 着色器是无状态的：
- 不能有全局随机状态（并行写入会竞争）
- 每帧同一像素需要不同种子

### 9.2 PCG（Permuted Congruential Generator）

```glsl
uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand_float(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) / float(0xffffffffu);  // [0, 1)
}
```

### 9.3 种子设计

```glsl
// 每像素不同（乘以大质数避免线性相关）
// 每帧不同（XOR 帧序号乘质数）
uint seed = uint(pixel.y * size.x + pixel.x) ^ uint(uFrameCount * 719393u);
```

关键：XOR 而不是加法，避免相邻像素种子的线性相关性。

### 9.4 PCG vs Wang Hash 的质量对比

Wang Hash（更简单）：
```glsl
uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u; seed ^= seed >> 4u;
    seed *= 0x27d4eb2du; seed ^= seed >> 15u;
    return seed;
}
```

PCG 在 TestU01 统计测试套件中通过了 BigCrush 全部测试（Wang Hash 有几项失败）。对于路径追踪，PCG 的均匀性更好，噪点分布更随机，不容易出现可见的重复图案。

---

## 10. 实现细节与构建

### 10.1 累积纹理格式

必须用 `GL_RGBA32F`（32 位浮点），因为：
- 累积值可能超过 1.0（多次反射的 HDR 累积）
- `GL_RGBA8` 精度不够（8 位 ≈ 256 级，累积多帧后量化误差明显）

```cpp
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
// image2D binding 必须匹配：
// layout(rgba32f, binding = 0) uniform image2D uAccumTex;
```

### 10.2 Image Load/Store 的 barrier

```cpp
glDispatchCompute(gx, gy, 1);
// image2D 读写需要 GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
// 然后 blit pass 才能正确读取纹理
```

### 10.3 全屏三角形（无 VBO）

```glsl
// Vertex Shader：用 gl_VertexID 生成全屏大三角形，不需要 VBO
vec2 pos = vec2((gl_VertexID & 1) * 4 - 1,
                (gl_VertexID >> 1) * 4 - 1);
// ID=0: (-1,-1), ID=1: (3,-1), ID=2: (-1,3)
// 三角形覆盖整个 [-1,1]² NDC 空间
```

### 10.4 Compute Shader 尺寸选择

```glsl
layout(local_size_x = 16, local_size_y = 16) in;
```

16×16 = 256 线程/工作组。二维 tile 比一维更好：
- 2D tile 在纹理访问时有更好的空间局部性（相邻像素在同一 tile 中）
- 与 GPU 的 warp 大小（32 或 64）对齐

### 10.5 构建

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target module18_raytracing_intro -j$(nproc)
cd build/module18_raytracing_intro
./module18_raytracing_intro
```

### 10.6 性能预期（1280×720，RTX 2060）

- 路径追踪 pass：~8ms/帧（约 125 fps 上限）
- Blit pass：<0.1ms
- 256 spp 约 2 秒收敛到较低噪点水平

---

## 11. 常见坑与调试技巧

### 坑 1：自相交（Self-Intersection / Shadow Acne）

症状：漫反射表面出现黑色斑点（像螺纹图案）。

原因：散射后的射线起点在表面上，与自身相交（t ≈ 0）。

解决：设置 `t_min = 0.001`（而不是 0），偏移射线起点：
```glsl
if (!hit_scene(r, 0.001, 1e10, rec)) { ... }  // 注意 t_min
```

### 坑 2：折射全反射未处理

```glsl
// 必须检查全内反射条件
float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
bool cannot_refract = refraction_ratio * sin_theta > 1.0;
if (cannot_refract || reflectance > rand_float(seed)) {
    out_dir = reflect(unit_dir, rec.normal);  // 反射而非折射
} else {
    out_dir = refract(unit_dir, rec.normal, refraction_ratio);
}
```

遗漏此判断：`refract` 在全反射条件下返回零向量，后续计算 NaN。

### 坑 3：RGB 值未钳位（NaN 传播）

某些路径可能产生极大值（如折射无穷次），导致 NaN 污染累积纹理。

```glsl
// 安全保护
new_color = clamp(new_color, 0.0, 1e6);
// 或
if (any(isnan(new_color)) || any(isinf(new_color))) new_color = vec3(0);
```

### 坑 4：累积缓冲格式不对

如果用 `GL_RGBA8` 替换 `GL_RGBA32F`，颜色在 0-1 之间量化，多次累积后精度损失严重，噪点无法真正收敛。症状：图像很早就"定型"，增加 spp 没有明显改善。

### 坑 5：image2D 的 barrier 遗漏

blit pass 读取纹理前必须有 `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)`。
遗漏时：blit pass 读到的可能是 Compute Shader 写入之前的旧值。
症状：画面有 1 帧延迟，或部分区域显示旧内容。

### 坑 6：相机矩阵传错（inv_view 传 view）

```cpp
// 注意：传给 Compute Shader 的是逆矩阵
glm::mat4 inv_view = glm::inverse(view);
glm::mat4 inv_proj = glm::inverse(proj);
// 不要传原始的 view/proj
```

症状：场景方向/缩放完全错误，或射线方向散乱。

### 坑 7：rand_unit_sphere 拒绝采样无限循环

```glsl
// 最坏情况下拒绝采样可能循环很多次
// 加限制避免极端情况的无限循环
int limit = 0;
do {
    p = ...;
    limit++;
} while (dot(p,p) >= 1.0 && limit < 16);
```

### 调试技巧：颜色编码调试

直接将法线可视化（`0.5 * n + 0.5`）：
```glsl
// 路径追踪难以调试，先验证求交和法线
vec3 debug_color = rec.normal * 0.5 + 0.5;
imageStore(uAccumTex, pixel, vec4(debug_color, 1.0));
```

颜色编码材质类型（0=红，1=绿，2=蓝）验证材质赋予是否正确。

---

*Module 18 — Raytracing Intro | ogl_mastery 课程第五阶段*
