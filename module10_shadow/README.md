# Module 10: Shadow Mapping — Hard Shadow, PCF, PCSS

## 目录

1. [Shadow Map 两 Pass 原理](#1-shadow-map-两-pass-原理)
2. [光源空间变换矩阵推导](#2-光源空间变换矩阵推导)
3. [深度偏移：Shadow Acne 的成因与最优 bias](#3-深度偏移shadow-acne-的成因与最优-bias)
4. [Peter Panning 与正面剔除技巧](#4-peter-panning-与正面剔除技巧)
5. [PCF：为什么不能对深度值滤波](#5-pcf为什么不能对深度值滤波)
6. [Poisson 盘采样与旋转消除规律性](#6-poisson-盘采样与旋转消除规律性)
7. [PCSS 完整算法推导](#7-pcss-完整算法推导)
8. [CSM 原理（简述）](#8-csm-原理简述)
9. [常见坑与调试方法](#9-常见坑与调试方法)
10. [代码结构说明](#10-代码结构说明)
11. [延伸阅读](#11-延伸阅读)

---

## 1. Shadow Map 两 Pass 原理

阴影贴图（Shadow Map）是实时渲染中最基础的阴影技术，由 Lance Williams 在 1978 年提出。
核心思想是：**如果一个点在光源的视角下不可见，它就处于阴影中**。

### 两 Pass 流程

```
Pass 1（深度 Pass，从光源视角渲染）
┌──────────────────────────────┐
│ 绑定 Shadow Map FBO           │
│ 从光源位置/方向渲染场景        │
│ 只写深度缓冲（关闭颜色输出）   │
│ 输出: depth_tex[u,v] = d_min  │
└──────────────────────────────┘
            │
            ▼
Pass 2（光照 Pass，从相机视角渲染）
┌──────────────────────────────┐
│ 对每个片元 P：                │
│   将 P 变换到光源空间 → P_ls  │
│   透视除法 → NDC → UV 坐标    │
│   采样 shadow_map[UV] = d_blocker │
│   比较 P_ls.z vs d_blocker    │
│   P_ls.z > d_blocker → shadow │
└──────────────────────────────┘
```

### 深度贴图 FBO 创建

深度 Pass 使用一个只包含深度附件的 FBO：

```cpp
glGenTextures(1, &depth_tex);
glBindTexture(GL_TEXTURE_2D, depth_tex);
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
             width, height, 0,
             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                       GL_TEXTURE_2D, depth_tex, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);
```

**关键点**：
- `GL_CLAMP_TO_BORDER` + 白色边界：Shadow Map 范围之外的区域默认为已光照（不产生错误阴影）
- `glDrawBuffer(GL_NONE)` + `glReadBuffer(GL_NONE)`：明确告知 GPU 不需要颜色附件

---

## 2. 光源空间变换矩阵推导

### 平行光（定向光）的正交投影

平行光可视为无限远处的光源，使用**正交投影矩阵**来构造光源视图：

```
光源空间 MVP = P_ortho × V_light × M_world
```

**View 矩阵**：将光源方向放置在场景包围球中心的后方：

```glsl
vec3 light_pos = scene_center - normalize(light_dir) * scene_radius * 2.0;
mat4 view = lookAt(light_pos, scene_center, vec3(0,1,0));
```

**正交投影矩阵**（覆盖场景包围球）：

```
P_ortho = ortho(-r, r, -r, r, near, far)

其中 r = scene_radius × 1.2（留 20% 余量）
near = 0.1
far  = scene_radius × 6.0
```

正交投影矩阵的显式形式：

```
P_ortho =
┌ 2/(r-l)    0        0       -(r+l)/(r-l) ┐
│   0      2/(t-b)    0       -(t+b)/(t-b) │
│   0        0     -2/(f-n)   -(f+n)/(f-n) │
└   0        0        0            1        ┘
```

对于对称正交投影（l=-r, b=-t）：

```
P_ortho =
┌ 1/r   0    0         0    ┐
│  0   1/t   0         0    │
│  0    0   -2/(f-n)  -(f+n)/(f-n) │
└  0    0    0         1    ┘
```

### NDC 到贴图坐标的转换

OpenGL NDC 范围是 `[-1, 1]`，而纹理坐标范围是 `[0, 1]`：

```glsl
vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
projCoords = projCoords * 0.5 + 0.5;  // NDC → [0,1]
```

转换矩阵形式：

```
T_bias =
┌ 0.5  0   0  0.5 ┐
│  0  0.5  0  0.5 │
│  0   0  0.5 0.5 │
└  0   0   0   1  ┘
```

---

## 3. 深度偏移：Shadow Acne 的成因与最优 bias

### Shadow Acne 的成因

Shadow Acne（阴影条纹/阴影痤疮）是由**深度精度不足**造成的自遮挡现象。

设想光线以角度 θ 照射一个平面：
- 深度贴图中，该平面被离散地存储为一系列"台阶"
- 当我们从相机视角采样该平面时，由于光线方向与平面不平行，同一个几何面的不同点可能对应深度贴图中相邻但不同的像素
- 结果：该点的实际深度 `z_actual` 可能略大于深度贴图中读取的 `z_stored`，产生错误的自遮挡

```
         光源
          |
          |  θ
    ──────┼──────  （平面）
     正确的  |  错误采样
     未遮挡  |  到的台阶
```

### 最优 bias 公式

固定 bias（如 `bias = 0.005`）在浅角度光照时不够，在陡峭角度时过大。

**自适应 bias**（基于法线与光源方向夹角）：

```glsl
float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);
```

- 当 `N·L = 1`（光线垂直入射）：`bias = 0.005`（最小偏移）
- 当 `N·L = 0`（光线掠角入射）：`bias = 0.05`（最大偏移）
- 线性插值保证平滑过渡

### 数学推导

设深度贴图分辨率为 `N×N`，场景深度范围为 `[near, far]`，则深度精度为：

```
δz = (far - near) / 2^24  （24位深度缓冲）
```

光线以角度 θ 照射时，相邻两个纹素之间的实际深度差约为：

```
Δz = (scene_radius × 2) / N × tan(θ)
```

因此最小 bias 应满足：`bias ≥ Δz / 2`。

自适应公式中 `(1 - dot(N,L)) ≈ sin²(θ/2) ≈ θ²/4`（小角度近似），与 `tan(θ) ≈ θ` 成正比，因此理论上符合需要的自适应性。

---

## 4. Peter Panning 与正面剔除技巧

### Peter Panning 是什么

当 bias 过大时，阴影会与投射物体分离，看起来像物体"飘在空中"——这就是 Peter Panning。

```
没有 Peter Panning:       有 Peter Panning:
    ████                      ████
    ████                      ████
  ████████                    ████████
  ████████                    ████████
  ▓▓▓▓▓▓▓▓  ← 阴影紧贴
              物体底部      ▓▓▓▓▓▓▓▓  ← 阴影与物体分离
```

### 正面剔除（Front-Face Culling）技巧

在深度 Pass 中，将 Cull Face 改为剔除**正面**：

```cpp
glEnable(GL_CULL_FACE);
glCullFace(GL_FRONT);   // depth pass: 剔除正面
// ... 渲染场景到深度贴图 ...
glCullFace(GL_BACK);    // 恢复正常
```

**原理**：
- 大多数 Shadow Acne 发生在物体的正面（光照面），因为光线以锐角照射
- 剔除正面后，深度贴图只包含背面的深度，相当于给 bias 加了一层"物体厚度"的隐式偏移
- 这样物体正面的阴影比较时，与存储的背面深度比较，自然产生合适的偏移

**局限性**：
- 对于薄物体（如草地、布料）效果不好，因为正面和背面相距极近
- 某些悬浮几何形状可能出现阴影丢失

---

## 5. PCF：为什么不能对深度值滤波

### 核心问题

一个常见的误解是：只要对深度贴图进行双线性插值，就能得到软阴影。

**这是错误的**。深度值本身是非线性的，并且深度贴图边界上的深度值不连续。

考虑阴影边界处的情况：
- 左侧纹素：遮挡物深度 `d = 0.3`
- 右侧纹素：没有遮挡物，深度 `d = 1.0`（最大值）

如果对深度值进行线性插值得到 `d_interp = 0.65`，然后与接收点深度 `z = 0.5` 比较：
- `z > d_interp`？ `0.5 > 0.65`？ 否 → 判定为未遮挡

但实际上片元处于阴影中（左侧遮挡物确实在遮挡它）！插值后的深度失去了原有的几何含义。

### 正确做法：PCF（Percentage Closer Filtering）

PCF 对的是**比较结果**进行过滤，而不是深度值本身：

```glsl
float shadow = 0.0;
for (int i = 0; i < N; ++i) {
    float sampleDepth = texture(shadowMap, uv + offset[i]).r;
    // 先比较，再累加 0/1 结果
    shadow += (currentDepth - bias > sampleDepth) ? 1.0 : 0.0;
}
shadow /= N;  // 最终得到 [0,1] 的软阴影值
```

每个采样点做独立的深度比较，返回 0（未遮挡）或 1（遮挡），最终平均值就是阴影强度。这样即使在深度不连续的边界处，比较结果也是有意义的。

### 核大小与软阴影质量

PCF 核越大，阴影越软。但大核的问题：
- `N` 个采样 → `N` 次纹理读取 → 性能开销 O(N)
- 均匀网格采样有明显的规律性（带状噪声）

解决方案：使用 **Poisson 盘**采样并加入旋转扰动（见下节）。

---

## 6. Poisson 盘采样与旋转消除规律性

### Poisson 盘分布

Poisson 盘（Poisson Disk）是一种在圆盘内均匀分布、任意两点间距不小于某阈值的点集。

优点：
- 比均匀网格采样的规律性低
- 比完全随机采样的分布更均匀
- 采样点数少（16个）即可接近蓝噪声质量

本模块使用的 16 点 Poisson 盘：

```glsl
const vec2 POISSON16[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    // ... (完整见 shadow_lighting.frag)
);
```

### 旋转消除规律性

即使 Poisson 盘分布良好，相邻像素使用相同的采样模式仍会产生可见的重复图案。

解决方案：对每个片元使用不同的随机旋转角：

```glsl
float angle = rand(projCoords.xy) * 6.28318;
float cosA  = cos(angle), sinA = sin(angle);

for (int i = 0; i < 16; ++i) {
    // 旋转 Poisson 采样点
    vec2 rotated = vec2(
        cosA * POISSON16[i].x - sinA * POISSON16[i].y,
        sinA * POISSON16[i].x + cosA * POISSON16[i].y
    );
    // ... 使用旋转后的偏移采样
}
```

其中随机函数基于片元位置（固定 seed），每帧保持一致，避免闪烁。

---

## 7. PCSS 完整算法推导

PCSS（Percentage Closer Soft Shadows，Randima Fernando 2005）是目前游戏中常用的软阴影算法，能够根据接收点到遮挡物的距离动态调整阴影半影宽度。

### 物理基础

半影（Penumbra）的宽度与以下因素有关：

```
w_penumbra = w_light × (d_receiver - d_blocker) / d_blocker
```

其中：
- `w_light`：光源在光源空间中的虚拟大小（texels）
- `d_receiver`：接收阴影的片元到光源的距离
- `d_blocker`：遮挡物到光源的平均距离

**几何推导**（相似三角形）：

```
光源宽度 w_light
    ┌─────────────────────────┐
    │     ← w_light →         │
    │                          │
    │                          │  d_blocker（遮挡物到光源距离）
    │       ████████           │
    │       ████████           │
    │       ┌──────┐           │  d_receiver（接收面到光源距离）
    │       │      │← w_pen → │
    └───────┴──────┴───────────┘

相似三角形：w_pen / w_light = (d_receiver - d_blocker) / d_blocker
```

### PCSS 三步算法

#### 步骤 1：Blocker Search（遮挡物搜索）

在接收点周围搜索**遮挡物的平均深度**（只统计比接收点更靠近光源的深度）：

```glsl
float searchRadius = uLightSize * (receiverDepth - 0.1) / receiverDepth;
float blockerSum = 0.0;
int   blockerCount = 0;

for (int i = 0; i < 16; ++i) {
    vec2 offset = POISSON16[i] * texelSize * searchRadius * 20.0;
    float sDepth = texture(shadowMap, uv + offset).r;
    if ((receiverDepth - bias) > sDepth) {
        blockerSum += sDepth;
        ++blockerCount;
    }
}

if (blockerCount == 0) return 0.0;  // 完全光照
float avgBlocker = blockerSum / float(blockerCount);
```

搜索半径随接收点到光源距离动态缩放，这确保了近处物体搜索范围更大（光源角度更大）。

#### 步骤 2：半影宽度计算

```glsl
float penumbraWidth = (receiverDepth - avgBlocker) / avgBlocker * uLightSize;
```

- `avgBlocker` 越小（遮挡物越近）→ 半影越宽（更软的阴影）
- `avgBlocker ≈ receiverDepth`（遮挡物紧贴接收面）→ 半影很窄（接近硬阴影）

#### 步骤 3：可变核 PCF

使用步骤 2 计算得到的半影宽度作为 PCF 核半径：

```glsl
float shadow = 0.0;
for (int i = 0; i < 16; ++i) {
    vec2 offset = POISSON16[i] * texelSize * penumbraWidth * 30.0;
    float sDepth = texture(shadowMap, uv + offset).r;
    shadow += (receiverDepth - bias) > sDepth ? 1.0 : 0.0;
}
return shadow / 16.0;
```

### PCSS 与 PCF 的对比

| 特性 | Hard Shadow | PCF | PCSS |
|------|-------------|-----|------|
| 每片元采样次数 | 1 | N（固定） | 2N（两步） |
| 半影大小 | 无 | 固定 | 动态（基于距离） |
| 物理真实性 | 低 | 中 | 高 |
| 性能开销 | 极低 | 中 | 高 |

---

## 8. CSM 原理（简述）

级联阴影贴图（Cascaded Shadow Maps, CSM）用于解决大场景中 Shadow Map 分辨率不足的问题。

### 问题根源

单张 Shadow Map 需要覆盖整个视锥体（可能跨越几百米），导致近处的阴影分辨率极低（"像素化阴影"）。

### 核心思想

将相机视锥体沿深度方向分割为 `K` 个子视锥体（通常 K=3~4），每个子视锥体独立生成一张 Shadow Map：

```
近平面 ──── 级联0 ──── 级联1 ──── 级联2 ──── 远平面
         （高分辨率）  （中分辨率）  （低分辨率）
```

### 对数分割公式

子视锥体的深度分割点：

```
C_i = λ × near × (far/near)^(i/K)  +  (1-λ) × (near + (i/K) × (far-near))
```

其中 `λ ∈ [0,1]` 控制对数分割与线性分割的混合：
- `λ = 1`：纯对数分割（近处分辨率最高，符合人眼感知）
- `λ = 0`：纯线性分割

### 各级联正交投影

对每个子视锥体，计算其 8 个顶点在光源空间中的 AABB，构造对应的正交投影矩阵。

在着色器中：

```glsl
// 根据片元深度选择级联
float depth = gl_FragCoord.z;
int cascade = 0;
for (int i = 0; i < NUM_CASCADES; ++i) {
    if (depth < cascadeSplits[i]) {
        cascade = i;
        break;
    }
}
// 使用对应级联的 Shadow Map 和变换矩阵
float shadow = sample_shadow(shadowMaps[cascade],
                              lightSpaceMatrices[cascade] * worldPos,
                              bias);
```

完整的 CSM 实现见 module17_scene_editor。

---

## 9. 常见坑与调试方法

### 坑 1：深度贴图精度问题

**症状**：大场景中阴影出现异常跳变或条纹。

**原因**：
- 使用了 `GL_DEPTH_COMPONENT16` 而非 `GL_DEPTH_COMPONENT24`
- Near/Far 比值过大（如 0.001/1000），导致深度值分布极不均匀

**修复**：
```cpp
// 使用 24 位精度
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, ...);
// 缩小 far/near 比值
float near = 1.0f, far = scene_radius * 4.0f;
```

### 坑 2：bias 过大导致 Peter Panning

**症状**：阴影与物体脱离，物体看起来悬浮。

**修复**：使用自适应 bias，并配合正面剔除：
```glsl
float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);
```

### 坑 3：采样范围超出 Shadow Map

**症状**：场景边缘出现错误阴影或无阴影区域。

**原因**：片元变换后的 UV 坐标超出 `[0,1]`。

**修复**：
```cpp
// Shadow Map 使用 CLAMP_TO_BORDER，边界颜色为白色（无阴影）
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
```

在着色器中也可以早退：
```glsl
if (projCoords.z > 1.0) return 0.0;  // 超出远平面，视为无阴影
```

### 坑 4：PCSS blocker search 半径

**症状**：软阴影在某些角度看起来不自然，或近处物体阴影很硬。

**原因**：Blocker search 的搜索半径过小，找不到有效遮挡物。

**修复**：搜索半径需要根据光源虚拟大小和接收点深度动态调整：
```glsl
float searchRadius = uLightSize * (receiverDepth - 0.1) / receiverDepth;
```

### 坑 5：Shadow Map 分辨率选择

| 分辨率 | 适用场景 | 内存占用（24bit）|
|--------|----------|---------|
| 512×512 | 移动端 | 0.75 MB |
| 1024×1024 | 标准场景 | 3 MB |
| 2048×2048 | 高质量 | 12 MB |
| 4096×4096 | 超高质量/CSM | 48 MB |

### 调试技巧

可视化 Shadow Map 深度（将深度值作为颜色输出）：

```glsl
// 调试片元着色器
float depth = texture(shadowMap, vTexCoord).r;
FragColor = vec4(vec3(depth), 1.0);
```

---

## 10. 代码结构说明

```
module10_shadow/
├── CMakeLists.txt
├── include/
│   └── shadow_map.h          # ShadowMap 封装（FBO + 深度纹理）
├── src/
│   ├── main.cpp              # 主循环：场景渲染、键盘交互
│   └── shadow_map.cpp        # ShadowMap 实现
└── shaders/
    ├── shadow_depth.vert      # Pass1: 光源视角变换
    ├── shadow_depth.frag      # Pass1: 空片元着色器
    ├── shadow_lighting.vert   # Pass2: 世界/光源空间变换
    └── shadow_lighting.frag   # Pass2: Blinn-Phong + Hard/PCF/PCSS
```

### 键盘控制

| 键 | 功能 |
|----|------|
| `1` | Hard Shadow |
| `2` | PCF（16-tap Poisson 盘） |
| `3` | PCSS（动态半影） |
| `←/→` | 手动调整光源角度 |
| `R` | 切换自动旋转 |
| `W` | 切换线框模式 |

### ShadowMap 接口

```cpp
void create(int w, int h);          // 创建深度 FBO
void bind_for_write();              // 绑定用于深度渲染
void bind_depth_tex(int slot);      // 绑定深度纹理供采样
glm::mat4 light_space_matrix(...);  // 计算光源空间变换矩阵
void destroy();                     // 释放资源
```

---

## 11. 延伸阅读

- **LearnOpenGL - Shadow Mapping**: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
- **LearnOpenGL - Point Shadows**: https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
- **Lance Williams (1978)**: "Casting Curved Shadows on Curved Surfaces" — Shadow Map 原始论文
- **Randima Fernando (2005)**: "Percentage-Closer Soft Shadows" — PCSS 原始论文
- **GPU Gems 1, Chapter 11**: Nvidia 关于 Shadow Map 的工程实践
- **Cascaded Shadow Maps**: https://learnopengl.com/Guest-Articles/2021/CSM
- **MSDN - Common Techniques to Improve Shadow Maps**: 实用技巧汇总
- **Acerola**: "Why Do Video Games Struggle with Shadows?" — YouTube 科普视频

---

*本模块演示了从最基础的 Hard Shadow 到 PCF 和 PCSS 的完整演进过程，理解了每个阶段解决的问题才能在实际项目中做出正确的技术选型。*
