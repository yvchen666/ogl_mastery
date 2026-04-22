# Module 11: HDR Framebuffer & Post-Processing

## 目录

1. [FBO 完整体系](#1-fbo-完整体系)
2. [MRT（Multiple Render Targets）](#2-mrtmultiple-render-targets)
3. [HDR 帧缓冲](#3-hdr-帧缓冲)
4. [Tone Mapping 算法对比](#4-tone-mapping-算法对比)
5. [伽马矫正 Timing](#5-伽马矫正-timing)
6. [Bloom：双向高斯模糊](#6-bloom双向高斯模糊)
7. [边缘检测：Sobel 算子](#7-边缘检测sobel-算子)
8. [景深：Circle of Confusion](#8-景深circle-of-confusion)
9. [常见坑与调试方法](#9-常见坑与调试方法)
10. [代码结构说明](#10-代码结构说明)
11. [延伸阅读](#11-延伸阅读)

---

## 1. FBO 完整体系

### 什么是 Framebuffer Object（FBO）

FBO 是 OpenGL 提供的**离屏渲染目标**，允许将渲染结果写入纹理或 Renderbuffer，而不是直接显示到屏幕。

默认 Framebuffer（窗口系统提供）无法以纹理形式读取，因此所有后处理效果都需要先渲染到 FBO，再对结果进行处理。

### FBO 的组成

```
FBO
├── Color Attachment 0      → 颜色纹理 or RBO
├── Color Attachment 1      → （MRT 用，可选）
├── Color Attachment N      → （最多 GL_MAX_COLOR_ATTACHMENTS）
├── Depth Attachment        → 深度纹理 or 深度 RBO
└── Stencil Attachment      → 模板 RBO（可选）
```

### Renderbuffer（RBO）vs 纹理附件

| 比较维度 | 纹理附件 | RBO |
|---------|---------|-----|
| 可以被着色器采样 | ✅ | ❌ |
| 支持 MSAA | ❌（需要 Multisample 纹理） | ✅ |
| 创建开销 | 略高 | 低 |
| 推荐用途 | 需要后处理读取的附件 | 深度/模板（不需要采样） |

**选择原则**：
- 颜色附件需要被后处理读取 → 用**纹理**
- 深度/模板附件只用于深度测试，不需要读取 → 用 **RBO**（节省内存带宽）

### FBO 创建流程

```cpp
GLuint fbo;
glGenFramebuffers(1, &fbo);
glBindFramebuffer(GL_FRAMEBUFFER, fbo);

// 附加颜色纹理
GLuint colorTex;
glGenTextures(1, &colorTex);
glBindTexture(GL_TEXTURE_2D, colorTex);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
             GL_RGBA, GL_FLOAT, nullptr);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, colorTex, 0);

// 附加深度 RBO
GLuint depthRbo;
glGenRenderbuffers(1, &depthRbo);
glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_RENDERBUFFER, depthRbo);

// 完整性检查
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    // 错误处理

glBindFramebuffer(GL_FRAMEBUFFER, 0);  // 解绑
```

### FBO 完整性（Completeness）检查

`GL_FRAMEBUFFER_COMPLETE` 要求：
1. 至少有一个附件
2. 所有附件的宽高相同
3. 颜色附件使用颜色格式，深度附件使用深度格式
4. 颜色附件不是 `GL_NONE`（当 `glDrawBuffers` 指定时）

常见的不完整原因（`glCheckFramebufferStatus` 返回非 COMPLETE）：
- `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT`：某个附件格式不对或未初始化
- `GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT`：没有任何附件
- `GL_FRAMEBUFFER_UNSUPPORTED`：附件格式组合不被当前驱动支持

---

## 2. MRT（Multiple Render Targets）

MRT 允许一次渲染 Pass 同时输出到多个颜色纹理。延迟渲染的 G-Buffer 正是 MRT 的典型应用。

### 着色器端（输出声明）

```glsl
layout(location = 0) out vec4 gPosition;   // → GL_COLOR_ATTACHMENT0
layout(location = 1) out vec3 gNormal;     // → GL_COLOR_ATTACHMENT1
layout(location = 2) out vec4 gAlbedoSpec; // → GL_COLOR_ATTACHMENT2
```

`layout(location = N)` 指定输出变量映射到第 N 个颜色附件。

### CPU 端（指定绘制目标）

```cpp
GLenum attachments[] = {
    GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2
};
glDrawBuffers(3, attachments);
```

必须在 FBO 绑定后调用，且数量与着色器输出数量匹配。

### MRT 带宽分析

以 1920×1080 分辨率，3 个 GL_RGBA16F 附件为例：

```
每帧写入数据量 = 3个附件 × 4通道 × 2字节/通道 × 1920 × 1080
             = 3 × 4 × 2 × 2073600
             = ~49.9 MB/frame
```

以 60fps 计算：约 **3 GB/s** 的显存带宽用于 G-Buffer 写入。这就是为什么 G-Buffer 格式设计需要仔细权衡（见 module12）。

---

## 3. HDR 帧缓冲

### 为什么需要 HDR

传统 8 位颜色缓冲（`GL_RGBA8`）每通道只有 256 个值，范围 `[0, 1]`。

现实世界的光照强度范围极大：
- 室内阴暗处：约 10 lux
- 明亮的窗户：约 10,000 lux
- 晴天室外：约 100,000 lux

若直接截断到 `[0, 1]`，大量光照细节丢失，亮部出现"过曝"（白色一片）。

### GL_RGBA16F vs GL_RGBA8

| 格式 | 位深 | 精度 | 范围 | 内存（1080p） |
|------|------|------|------|------|
| `GL_RGBA8` | 8 bits/ch | 1/255 ≈ 0.004 | [0, 1] | 8 MB |
| `GL_RGBA16F` | 16 bits/ch（半精度浮点） | ~0.001（在1.0附近） | [-65504, 65504] | 16 MB |
| `GL_RGBA32F` | 32 bits/ch（全精度浮点） | ~0.00001 | [-3.4e38, 3.4e38] | 32 MB |

HDR 渲染选择 `GL_RGBA16F`：
- 范围足够（远超真实场景所需）
- 内存适中（比 32F 节省一半）
- 现代 GPU 对半精度浮点运算有硬件加速

### 创建 HDR FBO

```cpp
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
             GL_RGBA, GL_FLOAT, nullptr);  // type 必须是 GL_FLOAT！
```

**注意**：内部格式是 `GL_RGBA16F`，但 `type` 参数是 `GL_FLOAT`（OpenGL 会自动截断到半精度）。如果误用 `GL_UNSIGNED_BYTE`，会悄无声息地创建整数格式纹理，导致 HDR 值被截断。

---

## 4. Tone Mapping 算法对比

Tone Mapping 将 HDR 值压缩到 `[0, 1]` 的过程，需要在保留细节的同时避免颜色失真。

### Reinhard Tone Mapping

**公式**：

```
L_out = L_in / (1 + L_in)
```

**推导**：这是 Michaelis-Menten 方程，当 `L → 0` 时 `L_out ≈ L_in`（线性），当 `L → ∞` 时 `L_out → 1`（饱和）。

```glsl
vec3 reinhard(vec3 hdr) {
    return hdr / (hdr + vec3(1.0));
}
```

**优点**：简单，自动归一化。
**缺点**：高光区域变灰（无法还原白色高光），整体偏暗。

**扩展版（带白点）**：

```glsl
// Lwhite：最大白色亮度
vec3 reinhard_ext(vec3 hdr, float Lwhite) {
    return hdr * (1.0 + hdr / (Lwhite * Lwhite)) / (1.0 + hdr);
}
```

### Exposure Tone Mapping

**公式**：

```
L_out = 1 - exp(-L_in × exposure)
```

这是指数饱和曲线：
- `exposure = 1`：标准曝光
- `exposure > 1`：曝光补偿（提亮暗部）
- `exposure < 1`：欠曝（压暗高光）

```glsl
vec3 exposure_tm(vec3 hdr, float exposure) {
    return vec3(1.0) - exp(-hdr * exposure);
}
```

**优点**：可控参数，艺术家友好。
**缺点**：不符合电影行业标准。

### ACES Filmic Tone Mapping

ACES（Academy Color Encoding System）是电影工业标准色彩空间。Narkowicz 2015 提出了一个近似公式：

```
L_out = (L × (a×L + b)) / (L × (c×L + d) + e)

其中 a=2.51, b=0.03, c=2.43, d=0.59, e=0.14
```

```glsl
vec3 aces_filmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
```

**为什么是电影标准**：
- 在阴影区域保留更多细节（S 曲线）
- 高光自然地向暖色偏移（胶片特性）
- 与 ACES 色彩管线兼容（HDR 显示器）

**三种 Tone Mapping 视觉对比**（1.0 lux 白光照明）：

| 亮度值 | Reinhard | Exposure(1.0) | ACES |
|--------|----------|----------------|------|
| 0.5 | 0.333 | 0.393 | 0.407 |
| 1.0 | 0.500 | 0.632 | 0.598 |
| 2.0 | 0.667 | 0.865 | 0.824 |
| 5.0 | 0.833 | 0.993 | 0.972 |
| 10.0 | 0.909 | 1.000 | 0.999 |

---

## 5. 伽马矫正 Timing

### 为什么要伽马矫正

显示器的物理特性导致输入的电信号与输出亮度不成线性关系：

```
显示亮度 = 输入电压 ^ γ    （γ ≈ 2.2）
```

这意味着在线性颜色空间中计算的 0.5 灰度，显示出来会比期望的暗（约 0.218 的感知亮度）。

伽马矫正：在输出前应用 `pow(x, 1/2.2)` 进行预矫正，使最终显示亮度符合线性计算结果。

### 必须在 Tone Mapping 之后

**错误顺序**（先伽马再 Tone Map）：

```
HDR_linear → gamma_encode → tone_map → display
```

问题：Tone Map 函数是为线性值设计的，对非线性值进行 Tone Map 会产生色偏。

**正确顺序**（先 Tone Map 再伽马）：

```
HDR_linear → tone_map → gamma_encode → display
```

```glsl
vec3 ldr = aces_filmic(hdr);        // 1. Tone Mapping（在线性空间）
ldr = pow(ldr, vec3(1.0 / 2.2));    // 2. Gamma encoding（输出到 sRGB 显示器）
FragColor = vec4(ldr, 1.0);
```

### sRGB 帧缓冲（自动伽马）

另一种方式是使用 sRGB 格式的帧缓冲，让 GPU 自动完成伽马编码：

```cpp
glEnable(GL_FRAMEBUFFER_SRGB);
```

启用后，所有写入默认帧缓冲的颜色值会自动应用 sRGB 编码（约等于 pow(x, 1/2.2)）。

**注意**：不能对同一值进行两次伽马矫正（既手动 pow 又开启 GL_FRAMEBUFFER_SRGB），否则会过矫正（颜色偏亮）。

---

## 6. Bloom：双向高斯模糊

### Bloom 效果原理

Bloom 模拟了真实相机/眼睛中的**光散射**现象：强光源周围会有柔和的光晕。

实现步骤：
1. 提取亮度超过阈值的像素（bright pass）
2. 对提取结果进行高斯模糊
3. 将模糊结果叠加到原始图像

### 高斯模糊数学推导

一维高斯函数：

```
G(x) = (1 / (σ√(2π))) × exp(-x² / (2σ²))
```

对于二维图像，完整的 2D 高斯模糊需要 `(2r+1)²` 次采样（r 为核半径）。

**关键性质**：二维高斯是可分离的（separable）：

```
G(x, y) = G(x) × G(y)
```

因此可以分两步进行：
1. 水平方向做 1D 高斯模糊（(2r+1) 次采样）
2. 垂直方向做 1D 高斯模糊（(2r+1) 次采样）

总采样次数：`2 × (2r+1)` vs `(2r+1)²`

对于 9-tap 核（r=4）：
- 分离式：9 + 9 = **18 次采样**
- 完整 2D：9 × 9 = **81 次采样**

节省了约 **78%** 的采样次数。

### 高斯核权重计算

```python
import math

def gaussian_weights(n, sigma):
    weights = [math.exp(-x*x / (2*sigma*sigma)) for x in range(n)]
    total = weights[0] + 2 * sum(weights[1:])
    return [w / total for w in weights]

# 5-tap: n=5, sigma=2
# [0.227027, 0.194595, 0.121622, 0.054054, 0.016216]
```

```glsl
const float WEIGHTS[5] = float[](
    0.227027, 0.194595, 0.121622, 0.054054, 0.016216
);

// 水平 Pass
vec3 result = texture(uImage, vTexCoord).rgb * WEIGHTS[0];
for (int i = 1; i < 5; ++i) {
    result += texture(uImage, vTexCoord + vec2(texelSize.x*i, 0)).rgb * WEIGHTS[i];
    result += texture(uImage, vTexCoord - vec2(texelSize.x*i, 0)).rgb * WEIGHTS[i];
}
```

### Bloom Pipeline 实现

```
HDR Scene FBO
      │
      ▼
Bright Pass (bloom_bright.frag)
提取亮度 > 1.0 的像素
      │
      ▼
Ping-Pong Blur (10 iterations)
水平模糊 → 垂直模糊 → 水平模糊 → ...
      │
      ▼
Bloom Composite (bloom_composite.frag)
HDR + bloom * strength → Tone Mapping → Gamma
```

使用 **Ping-Pong FBO**：两个 FBO 交替作为输入/输出，避免同时读写同一纹理。

---

## 7. 边缘检测：Sobel 算子

### Sobel 算子推导

边缘是图像中灰度变化剧烈的区域，可以用梯度（一阶导数）来检测。

Sobel 算子用有限差分来近似梯度：

**水平梯度 Gx**（检测垂直边缘）：

```
Gx = [-1  0  1]   × I(x,y)
     [-2  0  2]
     [-1  0  1]
```

**垂直梯度 Gy**（检测水平边缘）：

```
Gy = [-1 -2 -1]   × I(x,y)
     [ 0  0  0]
     [ 1  2  1]
```

梯度幅度：

```
|G| = √(Gx² + Gy²)
```

中心像素的权重为 0，其权重分配给相邻像素（包括对角线，权重为 1）和轴向（权重为 2）。这种不对称设计使 Sobel 对水平/垂直边缘更敏感，对对角边缘也有响应。

### GLSL 实现

```glsl
// 以亮度作为输入
float lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// 3x3 邻域亮度
float p[9];
int k = 0;
for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
        p[k++] = lum(texture(tex, uv + vec2(dx, dy) * texel).rgb);

// 卷积
float Gx = -p[0] + p[2] - 2.0*p[3] + 2.0*p[5] - p[6] + p[8];
float Gy = -p[0] - 2.0*p[1] - p[2] + p[6] + 2.0*p[7] + p[8];
float G  = sqrt(Gx*Gx + Gy*Gy);
```

**为什么对亮度操作**：对 RGB 三通道分别做 Sobel 然后取最大值也可以，但亮度单通道计算更高效，且视觉效果相近。

---

## 8. 景深：Circle of Confusion

### CoC 公式

景深（Depth of Field）基于**弥散圆**（Circle of Confusion，CoC）原理。对于不在焦平面上的点，它在传感器上形成一个圆而不是点。

CoC 半径（像素）：

```
CoC = |f × (d - d_focus)| × A / (d × d_focus)
       ─────────────────────────────────────────
       f: 焦距
       d: 物体到镜头的距离
       d_focus: 焦点距离
       A: 光圈大小（f/N，N 为光圈数）
```

简化的屏幕空间近似（基于深度缓冲）：

```glsl
float depth = texture(depthBuffer, uv).r;  // [0,1] NDC 深度
float coc   = abs(depth - uFocusDepth) * uDofStrength;
int   radius = int(coc * 10.0);           // 模糊半径（像素）
```

### 均值模糊实现（简单 DoF）

```glsl
vec3 blur(sampler2D tex, vec2 uv, int radius) {
    vec2 texel = 1.0 / textureSize(tex, 0);
    vec3 sum = vec3(0.0);
    int  count = 0;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            sum += texture(tex, uv + vec2(dx, dy) * texel).rgb;
            ++count;
        }
    return sum / float(count);
}
```

简单均值模糊的缺点：边缘处焦内与焦外的过渡不自然（焦外物体"渗入"焦内区域）。

**更好的实现**（Bokeh DoF）需要对每个像素分别计算 CoC 大小，并使用分桶（bucket）方式处理焦内/焦外的混合，见 module14_advanced_gl。

---

## 9. 常见坑与调试方法

### 坑 1：FBO 不完整

**症状**：`glCheckFramebufferStatus` 返回非 COMPLETE，渲染到 FBO 无效果。

**常见原因与修复**：

```cpp
// 错误：颜色附件使用了深度格式
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, ...);  // ❌ 颜色附件不能用深度格式

// 正确：
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, ...);  // ✅
```

### 坑 2：HDR 纹理 type 必须是 GL_FLOAT

```cpp
// 错误：
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, nullptr);  // ❌ 类型不匹配

// 正确：
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,
             GL_RGBA, GL_FLOAT, nullptr);  // ✅
```

误用 `GL_UNSIGNED_BYTE` 时，OpenGL 实现可能拒绝创建纹理或悄无声息地创建整数格式。

### 坑 3：Bloom 带宽问题

高分辨率 Bloom 需要大量模糊迭代，带宽开销显著。

**优化策略**：
- 在半分辨率或 1/4 分辨率上做 Bloom（人眼对 Bloom 不敏感于分辨率）
- 使用 `glBlitFramebuffer` 做降采样而非全分辨率渲染

```cpp
// 在半分辨率 FBO 上执行 Bloom
Framebuffer half_fbo;
half_fbo.create(width/2, height/2, {GL_RGBA16F}, false);
```

### 坑 4：sRGB 二次矫正

如果同时开启了 `GL_FRAMEBUFFER_SRGB` 并手动执行 `pow(x, 1/2.2)`，会出现双重伽马矫正，导致图像过亮。

**解决**：二选一，不要两者都用。

### 坑 5：glDrawBuffers 未设置

写入 MRT 时，如果忘记调用 `glDrawBuffers`，所有输出都只写入 `GL_COLOR_ATTACHMENT0`。

```cpp
// 每次绑定 MRT FBO 后都要设置
GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
glDrawBuffers(2, bufs);
```

### 调试工具

**可视化各后处理阶段**（添加 debug 模式）：

```glsl
// 在 tonemap.frag 中添加 debug 输出
if (uDebug == 1) {
    // 显示 HDR 亮度分布（伪彩色）
    float lum = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(
        step(lum, 1.0) * vec3(0,1,0) +  // 绿=正常范围
        step(1.0, lum) * vec3(1,0,0),   // 红=HDR 超出
        1.0
    );
}
```

---

## 10. 代码结构说明

```
module11_framebuffer/
├── CMakeLists.txt
├── include/
│   └── framebuffer.h              # 通用 FBO 封装
├── src/
│   ├── main.cpp                   # 主循环 + 后处理调度
│   └── framebuffer.cpp            # Framebuffer 实现
└── shaders/
    ├── fullscreen.vert             # 全屏四边形（vertex-ID 技巧）
    ├── hdr_scene.vert/frag         # HDR 场景渲染
    ├── tonemap.frag                # Reinhard / Exposure / ACES
    ├── bloom_bright.frag           # 亮度提取
    ├── blur_horizontal.frag        # 水平高斯模糊
    ├── blur_vertical.frag          # 垂直高斯模糊
    ├── bloom_composite.frag        # Bloom 合成
    └── postprocess.frag            # Sobel + DoF
```

### 键盘控制

| 键 | 功能 |
|----|------|
| `1` | 无后处理（线性输出 + Gamma） |
| `2` | Reinhard Tone Mapping |
| `3` | ACES Filmic Tone Mapping |
| `4` | Bloom（提取 + 模糊 + 合成） |
| `5` | Sobel 边缘检测 |
| `6` | 景深（CoC 模糊） |
| `+/-` | 调整曝光（模式2/3有效） |
| `R` | 切换灯光旋转 |

### Framebuffer 接口

```cpp
bool create(int w, int h,
            std::vector<FboAttachment> attachments,
            bool with_depth_rbo = true);
void bind() const;
static void unbind();
GLuint color_tex(int idx = 0) const;
void destroy();
```

### 全屏四边形技巧（无 VBO）

```glsl
// fullscreen.vert — vertex-ID 技巧，无需 VAO/VBO
void main() {
    vec2 pos = vec2((gl_VertexID & 1) * 2.0 - 1.0,
                    (gl_VertexID >> 1) * 2.0 - 1.0);
    vTexCoord   = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
```

调用时只需 `glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)` + 空 VAO。
VertexID 0-3 生成覆盖整个 NDC 的四边形。

---

## 11. 延伸阅读

- **LearnOpenGL - Framebuffers**: https://learnopengl.com/Advanced-OpenGL/Framebuffers
- **LearnOpenGL - HDR**: https://learnopengl.com/Advanced-Lighting/HDR
- **LearnOpenGL - Bloom**: https://learnopengl.com/Advanced-Lighting/Bloom
- **Narkowicz 2015 - ACES Filmic Tone Mapping**: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
- **ACES 官方网站**: https://www.oscars.org/science-technology/sci-tech-projects/aces
- **Jimenez 2014 - Next Generation Post Processing in Call of Duty**: GDC 讲座，介绍工业级后处理流程
- **Real-Time Rendering (4th ed.), Chapter 12**: 后处理与色调映射综合参考
- **OpenGL 4.6 Core Specification, Section 9**: Framebuffer Objects 完整规范

---

*理解 FBO 是深入 OpenGL 渲染管线的关键一步。每一个高级效果（阴影、延迟渲染、后处理）都建立在离屏渲染的基础上。*
