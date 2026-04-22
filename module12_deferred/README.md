# Module 12: Deferred Rendering + SSAO

## 目录

1. [Forward vs Deferred 复杂度分析](#1-forward-vs-deferred-复杂度分析)
2. [G-Buffer 设计权衡](#2-g-buffer-设计权衡)
3. [G-Buffer 可视化的调试价值](#3-g-buffer-可视化的调试价值)
4. [SSAO 完整算法推导](#4-ssao-完整算法推导)
5. [延迟渲染局限性](#5-延迟渲染局限性)
6. [Light Volume 优化](#6-light-volume-优化)
7. [常见坑与调试方法](#7-常见坑与调试方法)
8. [代码结构说明](#8-代码结构说明)
9. [渲染管线总览](#9-渲染管线总览)
10. [性能数据参考](#10-性能数据参考)
11. [延伸阅读](#11-延伸阅读)

---

## 1. Forward vs Deferred 复杂度分析

### Forward Rendering（正向渲染）

经典渲染管线：对每个几何片元，立即计算所有光源的光照。

```
对每个几何片元 f：
    对每个光源 l：
        计算 f 对 l 的光照贡献
```

复杂度：`O(f × l)`

其中：
- `f`：片元数量（= 分辨率 × overdraw 率）
- `l`：光源数量

### Deferred Rendering（延迟渲染）

将几何信息和光照计算分离为两个独立 Pass：

```
Pass 1（几何 Pass）：
    对每个几何片元 f：
        写入 G-Buffer（位置/法线/颜色）   → O(f)

Pass 2（光照 Pass）：
    对每个屏幕像素 p：
        对每个光源 l：
            从 G-Buffer 读取几何信息
            计算光照                        → O(p × l)
```

复杂度：`O(f + p × l)`

### 32 个光源的具体对比

假设场景：1280×720 分辨率，10 个不透明物体，平均 1.5× overdraw（过度绘制）：

```
f ≈ 1280 × 720 × 1.5 = 1,382,400 片元
p = 1280 × 720 = 921,600 像素
l = 32 个光源

Forward 着色调用次数：
    f × l = 1,382,400 × 32 ≈ 44,236,800 次

Deferred 着色调用次数：
    f + p × l = 1,382,400 + 921,600 × 32 ≈ 30,873,600 次
```

节省了约 **30%**。但当 overdraw 率更高时（如密集植被场景）：

```
overdraw = 5× → f = 4,608,000

Forward: 4,608,000 × 32 = 147,456,000 次
Deferred: 4,608,000 + 921,600 × 32 ≈ 34,099,200 次

节省约 77%！
```

**结论**：Deferred 渲染在**大量光源 + 高 overdraw**场景中优势显著。

### G-Buffer 写入成本

Deferred 渲染的代价是 G-Buffer 写入/读取的显存带宽：

```
G-Buffer 写入（每帧）：
3个RT × 1280×720像素 = 3 × 8 bytes × 921,600 ≈ 22 MB/frame（RGB16F）

以 60fps 计算：~1.3 GB/s 带宽用于 G-Buffer
```

因此 G-Buffer 格式设计要尽量紧凑（见下节）。

---

## 2. G-Buffer 设计权衡

### 本模块的 G-Buffer 布局

```
附件0: GL_RGB16F  → 世界空间位置 (x, y, z)
附件1: GL_RGB16F  → 世界空间法线 (nx, ny, nz)
附件2: GL_RGBA8   → 漫反射颜色(r,g,b) + 高光强度(a)
附件3: GL_DEPTH_COMPONENT24 → 深度缓冲（RBO）
```

### 方案1：直接存储位置（简单但带宽大）

```glsl
gPosition = fragPos;  // 直接存储 3D 坐标
```

存储成本：RGB16F = 3 × 2 bytes = **6 bytes/像素**

### 方案2：从深度重建位置（节省带宽，计算略复杂）

只需要存储深度值（1 float），在光照 Pass 中从深度重建世界坐标：

```glsl
// 光照 Pass 中重建世界坐标
float depth   = texture(depthTex, uv).r;
vec4  clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
vec4  worldPos = invProjView * clipPos;
worldPos /= worldPos.w;
```

存储成本：1 float = **4 bytes/像素**（节省 33%）

**权衡**：
- 方案1：实现简单，适合学习和原型开发
- 方案2：节省带宽（每帧节省约 1.8 MB），但需要传入逆矩阵 `inv(Proj × View)`

### 法线压缩：Oct-encoding

标准法线存储（RGB16F）：`3 × 2 = 6 bytes`

**Oct-encoding**（Cigolle 2014）：将单位向量映射到 `[0,1]²` 的两个 float，只需 `2 × 2 = 4 bytes`（节省 33%）：

```glsl
// 编码（几何 Pass）
vec2 oct_encode(vec3 n) {
    vec2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
    return (n.z <= 0.0)
        ? (1.0 - abs(p.yx)) * sign(p)
        : p;
    // 映射到 [0,1]
    return p * 0.5 + 0.5;
}

// 解码（光照 Pass）
vec3 oct_decode(vec2 e) {
    e = e * 2.0 - 1.0;
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy += (n.xy >= 0.0) ? -t : t;
    return normalize(n);
}
```

### G-Buffer 带宽完整分析（1920×1080, 60fps）

| 格式（位置方案1） | 大小/像素 | 总大小/帧 | 带宽/秒 |
|---------|---------|---------|---------|
| RGB16F × 3 + RGBA8 | 6+6+4 = 16 B | 33 MB | 2 GB/s |

| 格式（位置方案2 + Oct法线） | 大小/像素 | 节省比 |
|---------|---------|---------|
| Depth重建位置 + RG16法线 + RGBA8 | 4+4 = 8 B（颜色附件） | ~50% |

---

## 3. G-Buffer 可视化的调试价值

### 为什么要可视化 G-Buffer

延迟渲染的 Bug 往往难以定位，因为最终渲染结果经过了多个 Pass 的处理。
通过单独可视化每个 G-Buffer 通道，可以快速缩小问题范围：

- **位置缓冲异常** → 几何变换矩阵有误
- **法线缓冲异常** → 法线矩阵计算错误（如忘记 transpose(inverse(model))）
- **漫反射缓冲异常** → 材质参数传递有误
- **最终着色异常但各缓冲正常** → 光照 Pass 逻辑有问题

### 可视化技巧

```glsl
// deferred_lighting.frag 中的可视化模式
if      (uVisMode == 1) {
    // 位置缓冲：乘以系数缩放到可见范围
    FragColor = vec4(fragPos * 0.1 + 0.5, 1.0);
} else if (uVisMode == 2) {
    // 法线缓冲：映射 [-1,1] → [0,1]
    FragColor = vec4(normal * 0.5 + 0.5, 1.0);
} else if (uVisMode == 3) {
    // 漫反射：直接显示
    FragColor = vec4(albedo, 1.0);
} else if (uVisMode == 4) {
    // SSAO：显示遮蔽程度（白=无遮蔽，黑=完全遮蔽）
    FragColor = vec4(vec3(ao), 1.0);
}
```

### 法线可视化的常见异常

| 现象 | 可能原因 |
|------|---------|
| 法线全部偏向一个方向 | 光照 Pass 没有转换到正确坐标系 |
| 法线在相机移动时抖动 | 误用了 view 空间法线而非 world 空间 |
| 某些面法线颜色错误 | 顶点法线数据错误或模型翻转 |

---

## 4. SSAO 完整算法推导

SSAO（Screen Space Ambient Occlusion，屏幕空间环境遮蔽）由 Crytek 于 2007 年在《孤岛危机》中首次应用。

### 理论基础：环境遮蔽与渲染方程

在渲染方程中，环境遮蔽是半球积分中可见性函数 `V` 的近似：

```
AO(p) = (1/π) ∫_Ω V(p, ω) × cos(θ) dω
```

其中：
- `Ω`：以法线 `n` 为轴的上半球
- `V(p, ω)`：方向 `ω` 的可见性（1=可见，0=遮挡）
- `θ`：方向 `ω` 与法线 `n` 的夹角

SSAO 的近似：用深度缓冲代替真实几何，在半球内采样一组点，通过深度比较近似可见性。

### 算法步骤

#### 步骤 1：生成半球采样核

在切线空间（以法线为 Z 轴）的上半球均匀分布 N 个采样点：

```cpp
for (int i = 0; i < N; ++i) {
    glm::vec3 sample(
        uniform(-1, 1),  // x
        uniform(-1, 1),  // y
        uniform(0, 1)    // z（正值 = 上半球）
    );
    sample = glm::normalize(sample);
    sample *= uniform(0, 1);  // 随机长度

    // 偏向法线方向：使样本集中在近处
    // lerp(0.1, 1.0, (i/N)^2)
    float scale = (float)i / N;
    scale = 0.1f + 0.9f * scale * scale;
    kernel[i] = sample * scale;
}
```

**偏向策略的意义**：
- 线性间距：采样点均匀分布在 [0, R] 范围
- 二次加速：采样点集中在 [0, 0.3R] 内（更靠近表面）
- 物理意义：近处的遮挡对 AO 值贡献更大，远处遮挡意义较小

采样核分布示意：

```
法线方向 ↑
         ┌─────┐
    样本  │ ⋆⋆⋆│⋆ ⋆
    密度  │⋆⋆⋆⋆│⋆   （二次加速：近处密，远处疏）
         └─────┘
    0   0.3R   R
```

#### 步骤 2：生成随机旋转噪声纹理

4×4 的随机旋转向量纹理（在 XY 平面内旋转，Z=0）：

```cpp
glm::vec3 noise[16];
for (int i = 0; i < 16; ++i)
    noise[i] = glm::vec3(uniform(-1,1), uniform(-1,1), 0.0f);

glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0,
             GL_RGB, GL_FLOAT, glm::value_ptr(noise[0]));
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  // 4×4 平铺
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
```

噪声纹理用途：对每个片元，用不同的随机旋转来扰动采样核的方向，消除固定采样模式产生的条带。

#### 步骤 3：构建 TBN 矩阵

在着色器中，将采样核从切线空间变换到世界空间：

```glsl
vec3 fragPos = texture(gPosition, uv).xyz;
vec3 normal  = normalize(texture(gNormal, uv).xyz);
vec3 randVec = normalize(texture(noiseTex, uv * noiseScale).xyz);

// Gram-Schmidt 正交化：构建切线坐标系
vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
vec3 bitangent = cross(normal, tangent);
mat3 TBN       = mat3(tangent, bitangent, normal);
```

`TBN` 矩阵将切线空间向量变换到世界空间：
- 列向量分别是切线、副切线、法线
- 采样核 × TBN = 世界空间中以法线为轴的半球方向

#### 步骤 4：采样与深度比较

```glsl
float occlusion = 0.0;
for (int i = 0; i < 64; ++i) {
    // 切线空间 → 世界空间
    vec3 samplePos = fragPos + TBN * uSamples[i] * RADIUS;

    // 世界空间 → 屏幕空间（读 G-Buffer）
    vec4 offset    = projection * view * vec4(samplePos, 1.0);
    offset.xyz    /= offset.w;          // 透视除法
    offset.xyz     = offset.xyz * 0.5 + 0.5;  // NDC → UV

    // 从 G-Buffer 读取该屏幕位置的实际深度
    float sampleDepth = texture(gPosition, offset.xy).z;

    // Range check：防止误遮蔽（避免远处几何影响到当前片元）
    float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(fragPos.z - sampleDepth));

    // 深度比较：如果采样点"在"场景几何后面，认为被遮挡
    occlusion += (sampleDepth >= samplePos.z + BIAS ? 1.0 : 0.0) * rangeCheck;
}

FragOcclusion = 1.0 - (occlusion / 64.0);
```

**Range Check 的必要性**：

```
     表面A       表面B（远处，互不影响）
     ┌────┐      ┌────┐
     │    │      │    │
─────┴────┴──────┴────┴─────

如果不做 range check：
  采样点 S 落在表面A的深度范围内，但对应屏幕坐标恰好映射到表面B
  → sampleDepth = 表面B深度 >> samplePos.z
  → 误判：认为未遮挡（但实际上表面A可能被遮挡）
```

`smoothstep` 距离衰减确保只考虑半径 `R` 以内的几何对遮蔽的贡献。

#### 步骤 5：模糊 SSAO 降噪

4×4 均值模糊消除采样噪声：

```glsl
float result = 0.0;
for (int x = -2; x <= 1; ++x)
    for (int y = -2; y <= 1; ++y)
        result += texture(ssaoInput, uv + vec2(x,y) * texelSize).r;
FragOcclusion = result / 16.0;
```

模糊半径：4×4 = 16 采样（权衡：更大的模糊 → 更平滑但细节更少）。

### SSAO 与渲染方程的关系

SSAO 近似了半球积分中的可见性项：

```
AO ≈ 1/N × Σᵢ V(p, ωᵢ)
```

这忽略了 `cos(θ)` 权重，因此并不完全物理正确，但视觉效果已经足够好。

更物理正确的版本（HBAO, Horizon-Based AO）考虑了角度权重，见 module14。

---

## 5. 延迟渲染局限性

### 局限性 1：透明物体

G-Buffer 每个像素只能存储一个表面信息。透明物体（玻璃、水面、粒子）需要混合多层表面，无法用延迟渲染处理。

**解决方案**：透明物体使用 **Forward Pass** 单独渲染，叠加到 Deferred 结果上。

```
Pass 1: Geometry Pass → G-Buffer（只渲染不透明物体）
Pass 2: Lighting Pass → 最终颜色（不透明部分）
Pass 3: Forward Pass  → 透明物体叠加（alpha blending）
```

### 局限性 2：MSAA 兼容性

传统 MSAA（Multi-Sample Anti-Aliasing）在 Deferred 渲染中无法直接使用：
- G-Buffer 需要每个采样点都有完整的几何信息
- 以 4xMSAA 为例，G-Buffer 存储量增加 4 倍

**解决方案**：
- 使用后处理 FXAA 或 TAA 代替 MSAA（不依赖 G-Buffer 多采样）
- 使用 Deferred + MSAA 的专用扩展（`glTexImage2DMultisample`）

### 局限性 3：带宽开销

见第 1 节分析。在低端设备（手机 GPU）上，G-Buffer 带宽可能成为性能瓶颈。

### 局限性 4：光照模型限制

延迟着色（Deferred Shading）将光照参数存入 G-Buffer，因此所有物体必须使用**相同的光照模型**（或将光照模型选择也存入 G-Buffer，增加带宽）。

**变体：延迟光照（Deferred Lighting / Light Pre-Pass）**：
- 只将法线/高光存入 G-Buffer
- 光照 Pass 输出光照缓冲（漫反射/高光分别存储）
- 第三个 Pass 重新读取几何信息 + 光照缓冲计算最终颜色
- 优点：支持不同光照模型，G-Buffer 更紧凑

---

## 6. Light Volume 优化

### 问题

即使使用 Deferred 渲染，每个光源仍然需要访问所有屏幕像素。32 个光源 × 921,600 像素 = 约 3000 万次光照计算，其中许多像素与某些光源根本没有交互（距离超出衰减范围）。

### Light Volume：几何限制光照范围

对每个点光源，计算其有效影响半径 `R`（能量降至 threshold 以下的距离），然后：
1. 渲染一个球体（或多面体近似），半径为 `R`
2. 只对球体覆盖的像素执行该光源的光照计算

```cpp
// 光照 Pass 中，用球体几何代替全屏四边形
glEnable(GL_BLEND);
glBlendFunc(GL_ONE, GL_ONE);   // 加法混合（光照累加）

for (auto& light : lights) {
    float radius = compute_light_radius(light);  // 根据衰减计算半径
    glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1), light.pos),
        glm::vec3(radius)
    );
    glUniform...(model, light);
    sphere_mesh.draw();
}
```

**优化效果**（32个分布于场景各处的点光源）：

| 方法 | 像素×光源次数 | 节省比 |
|------|-------|------|
| 全屏 Pass | 32 × 921,600 = 29.5M | 基准 |
| Light Volume | ~3 × 921,600 = 2.8M（假设每像素平均3灯影响） | ~90% |

### Stencil 剔除

为了进一步避免光照 Volume 对远处不透明表面的错误计算，可以使用 Stencil Buffer 标记：

```
背面绘制光照球体 → 遮挡测试失败的像素置模板1
正面绘制光照球体 → 只对模板=1的像素执行光照计算
```

---

## 7. 常见坑与调试方法

### 坑 1：G-Buffer 格式选择

**症状**：SSAO 计算结果全黑或全白，或位置缓冲显示异常。

**原因**：位置缓冲使用了 `GL_RGB8`（值被截断到 `[0,1]`），无法存储负坐标值。

**修复**：
```cpp
// 位置/法线必须使用浮点格式
glTexImage2D(..., GL_RGB16F, ..., GL_FLOAT, ...);   // ✅
glTexImage2D(..., GL_RGB8,   ..., GL_UNSIGNED_BYTE, ...); // ❌ 值被截断
```

### 坑 2：深度缓冲共享

Deferred 渲染结束后，若要继续渲染透明物体（Forward Pass），需要**共享深度缓冲**：

```cpp
// 将 G-Buffer 的深度 RBO 复制到默认 FBO
glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer_fbo);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
glBlitFramebuffer(
    0, 0, width, height,
    0, 0, width, height,
    GL_DEPTH_BUFFER_BIT, GL_NEAREST
);
```

复制深度后，默认 FBO 上的 Forward 渲染可以正确地与延迟渲染的几何体进行深度测试。

### 坑 3：透明物体特殊处理

不能将透明物体写入 G-Buffer（会覆盖后面的不透明几何信息）。

**流程**：
1. 几何 Pass：只渲染不透明物体
2. 光照 Pass：Deferred 计算
3. 深度复制（blitFramebuffer）
4. Forward Pass：透明物体用正向渲染叠加

### 坑 4：SSAO 强度参数调整

SSAO 效果依赖多个参数：

| 参数 | 含义 | 典型值 | 过大的影响 |
|------|------|--------|----------|
| `RADIUS` | 采样半球半径（世界单位） | 0.3~1.0 | 遮蔽范围过大，错误遮蔽远处几何 |
| `BIAS` | 自遮蔽偏移 | 0.01~0.05 | 过小=表面自遮蔽；过大=遮蔽不足 |
| `kernel N` | 采样点数量 | 32~64 | 越多越好，但性能开销线性增加 |
| `strength` | 遮蔽强度系数 | 1.0~3.0 | 过大=过度遮蔽，画面发黑 |

调试时先把 `strength = 1.0`，`radius` 从小到大调，让效果自然出现在凹槽/缝隙处。

### 坑 5：MRT 输出顺序

G-Buffer 片元着色器的 `layout(location=N)` 必须与 `glDrawBuffers` 中的顺序一致：

```glsl
// gbuffer.frag
layout(location = 0) out vec3 gPosition;   // → GL_COLOR_ATTACHMENT0
layout(location = 1) out vec3 gNormal;     // → GL_COLOR_ATTACHMENT1
layout(location = 2) out vec4 gAlbedoSpec; // → GL_COLOR_ATTACHMENT2
```

```cpp
// gbuffer.cpp
GLenum attachments[] = {
    GL_COLOR_ATTACHMENT0,  // 对应 location=0
    GL_COLOR_ATTACHMENT1,  // 对应 location=1
    GL_COLOR_ATTACHMENT2   // 对应 location=2
};
glDrawBuffers(3, attachments);
```

如果顺序不匹配，数据会写到错误的纹理，导致光照 Pass 读取到错误的几何数据。

---

## 8. 代码结构说明

```
module12_deferred/
├── CMakeLists.txt
├── include/
│   ├── gbuffer.h         # G-Buffer 封装（MRT FBO）
│   └── ssao.h            # SSAO 数据（核、噪声纹理、FBO）
├── src/
│   ├── main.cpp          # 4-Pass 渲染循环 + 可视化
│   ├── gbuffer.cpp       # G-Buffer MRT 实现
│   └── ssao.cpp          # SSAO 核生成、噪声纹理、FBO
└── shaders/
    ├── fullscreen.vert    # 全屏四边形（vertex-ID）
    ├── gbuffer.vert/frag  # 几何 Pass（MRT 输出）
    ├── ssao.frag          # SSAO 计算（64采样）
    ├── ssao_blur.frag     # SSAO 4×4 均值模糊
    └── deferred_lighting.frag  # 光照 Pass（32 点光源 + SSAO）
```

### 键盘控制

| 键 | 功能 |
|----|------|
| `1` | 完整光照 + SSAO |
| `2` | 可视化位置缓冲 |
| `3` | 可视化法线缓冲 |
| `4` | 可视化漫反射缓冲 |
| `5` | 可视化 SSAO 遮蔽 |
| `R` | 切换灯光旋转 |

### 关键接口

```cpp
// GBuffer
bool create(int w, int h);      // 创建 MRT FBO（3个颜色附件 + 深度RBO）
void bind_geometry_pass();      // 绑定 FBO 用于几何 Pass
void bind_textures();           // 绑定到纹理单元 0/1/2

// SSAO
void init(int w, int h);               // 初始化 FBO 和纹理
void generate_kernel(int n = 64);      // 生成半球采样核
void generate_noise_tex();             // 生成 4×4 旋转噪声纹理
```

---

## 9. 渲染管线总览

```
┌─────────────────────────────────────────────────────────┐
│                    每帧渲染流程                           │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Pass 1: 几何 Pass                               │   │
│  │  输入: 场景几何 + 模型矩阵 + 材质                 │   │
│  │  输出: G-Buffer                                  │   │
│  │    • 位置纹理 (RGB16F)                           │   │
│  │    • 法线纹理 (RGB16F)                           │   │
│  │    • 漫反射+高光 (RGBA8)                         │   │
│  └────────────────────┬─────────────────────────────┘   │
│                       │                                  │
│  ┌────────────────────▼─────────────────────────────┐   │
│  │  Pass 2: SSAO                                    │   │
│  │  输入: G-Buffer(位置+法线) + 噪声纹理 + 采样核   │   │
│  │  输出: 遮蔽纹理 (R16F, 0=遮挡 1=未遮挡)         │   │
│  └────────────────────┬─────────────────────────────┘   │
│                       │                                  │
│  ┌────────────────────▼─────────────────────────────┐   │
│  │  Pass 3: SSAO 模糊                               │   │
│  │  输入: 原始 SSAO 纹理                            │   │
│  │  输出: 模糊后的遮蔽纹理                          │   │
│  └────────────────────┬─────────────────────────────┘   │
│                       │                                  │
│  ┌────────────────────▼─────────────────────────────┐   │
│  │  Pass 4: 光照 Pass                               │   │
│  │  输入: G-Buffer(全部) + SSAO(模糊) + 32个光源   │   │
│  │  输出: 最终颜色（写入默认 FBO）                  │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

---

## 10. 性能数据参考

以下数据为典型 GTX 1080 Ti 上的参考值（1080p, 32 点光源）：

| Pass | 耗时（估算） | 主要瓶颈 |
|------|------------|---------|
| 几何 Pass | ~0.5ms | 顶点处理 + G-Buffer 写入带宽 |
| SSAO Pass | ~1.5ms | 64 次纹理读取 × 100万像素 |
| SSAO 模糊 | ~0.2ms | 16 次采样 |
| 光照 Pass（无 Light Volume）| ~2.0ms | 32 光源 × 全屏采样 |
| 光照 Pass（有 Light Volume）| ~0.4ms | 每像素平均3灯 |
| **总计** | **~4.6ms** | 约 217fps 上限 |

---

## 11. 延伸阅读

- **LearnOpenGL - Deferred Shading**: https://learnopengl.com/Advanced-Lighting/Deferred-Shading
- **LearnOpenGL - SSAO**: https://learnopengl.com/Advanced-Lighting/SSAO
- **John Chapman (2013)**: "SSAO Tutorial" — SSAO 算法经典原文
  http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
- **Crytek (2007)**: "Real-Time Ambient Occlusion" — SSAO 首次公开在《孤岛危机》中的技术演讲
- **Cigolle et al. (2014)**: "A Survey of Efficient Representations for Independent Unit Vectors" — Oct-encoding 法线压缩
- **Kajalin (2009)**: "Screen-Space Ambient Occlusion" — GPU Gems 中的 SSAO 变体
- **Real-Time Rendering (4th ed.), Chapter 20**: 可见性与遮蔽
- **GDC 2011 - Lighting of Battlefield 3**: 延迟渲染在大型商业项目中的实践（Light Volume 等优化）
- **HBAO (Horizon-Based Ambient Occlusion)**: Nvidia 2008, 更物理正确的 AO 算法
  https://developer.nvidia.com/object/ssao-white-paper.html

---

*延迟渲染是现代游戏引擎的核心技术之一（UE4/5, Unity HDRP, CryEngine 均使用延迟渲染）。理解其原理和局限性，是进阶图形开发的必要基础。*
