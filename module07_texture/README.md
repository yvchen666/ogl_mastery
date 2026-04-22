# Module 07 — OpenGL 纹理系统完全指南

## 目录

1. [纹理对象的完整生命周期](#1-纹理对象的完整生命周期)
2. [内部格式 vs 数据格式](#2-内部格式-vs-数据格式)
3. [sRGB 色彩空间与 Gamma 矫正](#3-srgb-色彩空间与-gamma-矫正)
4. [过滤模式](#4-过滤模式)
5. [Mipmap 原理与 LOD 计算](#5-mipmap-原理与-lod-计算)
6. [各向异性过滤](#6-各向异性过滤)
7. [纹理坐标系（UV 空间）](#7-纹理坐标系uv-空间)
8. [纹理单元（Texture Units）](#8-纹理单元texture-units)
9. [纹理压缩格式](#9-纹理压缩格式)
10. [常见坑与排查清单](#10-常见坑与排查清单)
11. [本模块代码详解](#11-本模块代码详解)

---

## 1. 纹理对象的完整生命周期

### 1.1 创建

```cpp
GLuint tex_id;
glGenTextures(1, &tex_id);          // 生成名称（仅是一个整数）
glBindTexture(GL_TEXTURE_2D, tex_id); // 绑定到目标，此时才在 GPU 分配对象
```

OpenGL 4.5+ 提供了 DSA（Direct State Access）接口，可以不必先绑定：

```cpp
glCreateTextures(GL_TEXTURE_2D, 1, &tex_id); // 立即分配对象
glTextureStorage2D(tex_id, levels, GL_RGB8, width, height);
glTextureSubImage2D(tex_id, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
```

DSA 接口更安全（无隐式状态），但本模块为了与旧代码兼容使用传统绑定方式。

### 1.2 上传数据

```cpp
glTexImage2D(
    GL_TEXTURE_2D,   // 目标
    0,               // mip level（0 = 基础层）
    GL_RGB8,         // 内部格式（GPU 上的存储格式）
    width, height,   // 尺寸
    0,               // border（必须为 0，历史遗留）
    GL_RGB,          // 像素数据的格式（CPU 传入的格式）
    GL_UNSIGNED_BYTE,// 像素数据的类型
    data             // 像素数据指针（nullptr = 只分配不上传）
);
```

- `glTexImage2D` 每次调用都重新分配 GPU 存储。
- `glTexSubImage2D` 只更新部分区域，不重新分配，速度更快（用于动态纹理更新）。
- OpenGL 4.2 引入的 `glTexStorage2D` 一次性分配不可变存储（immutable storage），之后只能用 `glTexSubImage2D` 更新，更高效。

### 1.3 采样参数设置

```cpp
// 环绕模式
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);       // 水平
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);       // 垂直
// 其他：GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT

// 过滤模式
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // 缩小
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);               // 放大
```

### 1.4 生成 Mipmap

```cpp
glGenerateMipmap(GL_TEXTURE_2D); // 自动从 level 0 生成所有更小的 mip 层
```

注意：必须在上传基础层数据之后、使用之前调用。如果使用了 Mipmap 过滤（MIN_FILTER 包含 MIPMAP），但没有生成 mipmap，纹理会显示为**黑色**。

### 1.5 使用

```cpp
glActiveTexture(GL_TEXTURE0 + slot); // 激活纹理单元（0..GL_MAX_TEXTURE_IMAGE_UNITS-1）
glBindTexture(GL_TEXTURE_2D, tex_id);
// 在着色器中：uniform sampler2D uTex; 并 set_int("uTex", slot);
```

### 1.6 销毁

```cpp
glDeleteTextures(1, &tex_id); // 释放 GPU 资源
tex_id = 0;                    // 防止悬空句柄
```

---

## 2. 内部格式 vs 数据格式

这是 OpenGL 中最容易混淆的地方之一。

| 参数 | 含义 | 示例 |
|------|------|------|
| `internalformat` | GPU 上实际存储的格式 | `GL_RGB8`（每通道 8 位，3 通道） |
| `format` | CPU 传入像素数据的逻辑格式 | `GL_RGB`（3 通道，无位数信息） |
| `type` | CPU 传入像素数据每分量的类型 | `GL_UNSIGNED_BYTE`（0-255） |

### 常用内部格式对照

| internalformat | 含义 | 内存占用/像素 |
|----------------|------|--------------|
| `GL_R8` | 1 通道，8 位归一化 | 1 字节 |
| `GL_RG8` | 2 通道，各 8 位 | 2 字节 |
| `GL_RGB8` | 3 通道，各 8 位 | 3 字节（驱动可能填充到 4） |
| `GL_RGBA8` | 4 通道，各 8 位 | 4 字节 |
| `GL_RGB16F` | 3 通道，各 16 位浮点 | 6 字节 |
| `GL_RGBA32F` | 4 通道，各 32 位浮点 | 16 字节 |
| `GL_SRGB8` | sRGB 3 通道，采样时自动线性化 | 3 字节 |
| `GL_SRGB8_ALPHA8` | sRGB 4 通道 | 4 字节 |
| `GL_DEPTH24_STENCIL8` | 深度 24 位 + 模板 8 位 | 4 字节 |

### 为什么不直接用 `GL_RGB`？

在旧版 OpenGL 中，`GL_RGB` 可以作为 internalformat，驱动会自行选择位深度。
在现代 OpenGL Core Profile 中，推荐始终使用**精确格式**（sized format）如 `GL_RGB8`，
以确保跨平台/驱动一致性。

### 格式不匹配会怎样？

```cpp
// CPU 数据是 RGBA（4通道），但 internalformat 指定为 RGB8（3通道）
// OpenGL 会自动丢弃 alpha 通道 —— 不会报错，但可能不是你想要的
glTexImage2D(..., GL_RGB8, ..., GL_RGBA, GL_UNSIGNED_BYTE, data);
```

---

## 3. sRGB 色彩空间与 Gamma 矫正

### 3.1 为什么需要 sRGB？

人眼对亮度的感知是非线性的（Weber–Fechner 定律）。
为了在有限的 8 位精度中最大化感知质量，PNG/JPEG 等图片文件使用 sRGB 编码，
其中暗部被拉伸（约 `value^(1/2.2)`）。

如果直接将 sRGB 图片当作线性数据用于光照计算，会出现颜色偏暗/偏亮的问题。

### 3.2 正确工作流

```
1. 纹理文件（PNG/JPG）: sRGB 编码
   ↓  GPU 采样时自动线性化（^2.2）
2. 着色器中光照计算: 线性空间
   ↓  写入 sRGB 帧缓冲时自动编码（^(1/2.2)）
3. 显示器: sRGB 解码 → 正确亮度
```

### 3.3 OpenGL 实现

```cpp
// 加载 sRGB 纹理（美术素材，如漫反射贴图）
glTexImage2D(..., GL_SRGB8_ALPHA8, ..., GL_RGBA, GL_UNSIGNED_BYTE, data);
// GPU 采样时会自动将 sRGB 值转换为线性值

// 法线贴图、高度图、粗糙度图等数据纹理 —— 不用 sRGB
glTexImage2D(..., GL_RGB8, ..., GL_RGB, GL_UNSIGNED_BYTE, data);

// 开启 sRGB 帧缓冲（默认窗口通常已经是 sRGB 目标）
glEnable(GL_FRAMEBUFFER_SRGB);
// 所有写入默认帧缓冲的颜色会被自动从线性空间转换到 sRGB
```

### 3.4 哪些纹理应该用 sRGB？

| 纹理类型 | 是否 sRGB | 原因 |
|----------|-----------|------|
| 漫反射/Albedo | 是 | 美术资产，sRGB 编码 |
| 法线贴图 | **否** | 存储向量，不是颜色 |
| 粗糙度/金属度 | **否** | 线性数据 |
| 自发光 | 是 | 颜色纹理 |
| 高度/遮蔽 | **否** | 线性数据 |

---

## 4. 过滤模式

### 4.1 放大过滤（Magnification）

当纹素（texel）比屏幕像素大时（纹理被放大显示），需要在纹素之间插值。

**GL_NEAREST（最近邻）**
- 直接取最近的纹素颜色
- 像素化、锯齿感，适合像素艺术风格
- 最快

**GL_LINEAR（双线性）**
- 取最近 2×2 纹素的加权平均
- 平滑，但可能模糊
- 放大时最常用

### 4.2 缩小过滤（Minification）

当纹素比屏幕像素小时（纹理被缩小显示），一个像素可能覆盖多个纹素。

| 过滤模式 | 说明 |
|---------|------|
| `GL_NEAREST` | 最近邻（可能摩尔纹/闪烁） |
| `GL_LINEAR` | 双线性（比 NEAREST 好但仍有摩尔纹） |
| `GL_NEAREST_MIPMAP_NEAREST` | 选最近的 mip 层，在其中取最近邻 |
| `GL_LINEAR_MIPMAP_NEAREST` | 选最近的 mip 层，双线性插值 |
| `GL_NEAREST_MIPMAP_LINEAR` | 在两个 mip 层间线性插值，各取最近邻 |
| `GL_LINEAR_MIPMAP_LINEAR` | **三线性过滤**：两个 mip 层间线性 + 各层双线性，质量最好 |

> 注意：`GL_NEAREST_MIPMAP_NEAREST` 等模式只能用于 MIN_FILTER，不能用于 MAG_FILTER。

### 4.3 三线性过滤（Trilinear）

```
Trilinear = Bilinear(mip_level_n) ×(1-t) + Bilinear(mip_level_n+1) × t
```

其中 t = fract(LOD)，即 LOD 的小数部分。
消除了 mip 层之间的跳变（banding），但比双线性慢约 2× 纹理采样。

---

## 5. Mipmap 原理与 LOD 计算

### 5.1 为什么 Mipmap 能减少摩尔纹

根据**奈奎斯特采样定理**：要正确重建信号，采样频率必须至少是信号最高频率的 2 倍。

当纹理被缩小时，相邻像素跨越多个纹素，相当于对高频纹理进行欠采样，产生摩尔纹（混叠）。

Mipmap 预先对纹理进行下采样（低通滤波），在每个 mip 层中，纹素的密度与屏幕像素匹配，
从根本上避免了欠采样问题。

### 5.2 Mipmap 链

```
Level 0: 256×256  (基础纹理)
Level 1: 128×128
Level 2:  64×64
Level 3:  32×32
...
Level n:   1×1    (最小 mip，整个纹理的平均颜色)
```

总内存占用约为基础纹理的 **4/3 倍**（等比数列求和：1 + 1/4 + 1/16 + ... = 4/3）。

### 5.3 LOD（Level of Detail）计算

GPU 在光栅化阶段会计算每个像素对应的 LOD 值：

```
LOD = log2(max(|dUV/dx| · texture_width, |dUV/dy| · texture_height))
```

其中 `dUV/dx`、`dUV/dy` 是 UV 坐标相对于屏幕空间 x、y 的偏导数（`dFdx`/`dFdy`）。

- LOD = 0：纹理与屏幕像素 1:1
- LOD = 1：每个像素覆盖 2×2 纹素 → 使用 mip level 1
- LOD = n：使用 mip level n

可以手动控制 LOD 偏移：
```cpp
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.5f); // 使用更高分辨率的 mip
```

或在着色器中手动采样：
```glsl
float lod = textureQueryLod(uTex, vUV).y; // 查询 LOD
vec4 color = textureLod(uTex, vUV, lod);  // 手动指定 LOD
```

---

## 6. 各向异性过滤

### 6.1 为什么需要各向异性过滤

当表面与视线成斜角（如地面在远处），UV 坐标在屏幕 x、y 方向的变化率差异很大：
- x 方向：UV 变化大（纹理被压缩）
- y 方向：UV 变化小

标准 Mipmap 是**各向同性**的（正方形采样区域），会选择满足较大梯度的 mip 层，
导致纹理在另一方向过度模糊。

各向异性过滤（AF, Anisotropic Filtering）使用**椭圆形**（各向异性）采样区域，
沿着较大梯度方向采集多个纹素，有效消除斜面模糊。

### 6.2 实现

```cpp
// 查询支持的最大各向异性度（通常 16x）
GLfloat max_anisotropy;
glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);

// 设置各向异性过滤（1.0 = 关闭，16.0 = 最高质量）
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
```

OpenGL 4.6 起 `GL_TEXTURE_MAX_ANISOTROPY` 是核心规范的一部分（之前是扩展 EXT_texture_filter_anisotropic）。

### 6.3 性能开销

AF 的开销与各向异性度成正比：
- 1x：标准双线性/三线性，最快
- 4x：需要沿梯度方向采集 4 个样本
- 16x：最高质量，比 1x 慢约 2-4×（取决于 GPU 架构）

现代 GPU 对 AF 做了大量优化，在大多数场景下 16x AF 的帧率影响不到 1%。

---

## 7. 纹理坐标系（UV 空间）

### 7.1 OpenGL 约定

OpenGL 纹理坐标原点在**左下角**（0,0），UV 值从 (0,0) 到 (1,1)。

```
(0,1) ──────── (1,1)
  │              │
  │  纹理图像    │
  │              │
(0,0) ──────── (1,0)
```

### 7.2 stb_image 的陷阱

stb_image 读取 PNG/JPEG 时，第一行数据对应图片的**顶部**（左上角起）。
OpenGL 期望第一行数据对应纹理的**底部**（左下角）。

因此在加载纹理时需要翻转：

```cpp
stbi_set_flip_vertically_on_load(1); // 翻转 Y 轴
unsigned char* data = stbi_load(path, &w, &h, &channels, 0);
```

如果忘记翻转，纹理会上下颠倒。

### 7.3 不同系统的 UV 原点约定

| 系统/API | UV 原点 |
|----------|---------|
| OpenGL | 左下角 (0,0) |
| DirectX / Metal / Vulkan | 左上角 (0,0) |
| 大多数图片格式 | 左上角 |
| Blender（导出时） | 左下角（已正确）|
| Unity（内部） | 左下角 |

从 DirectX 移植资产时，可能需要在着色器中翻转 V：`vUV.y = 1.0 - vUV.y`。

### 7.4 环绕模式详解

```cpp
// GL_REPEAT（默认）: UV > 1 时重复
// UV = 1.5 → 取纹素 0.5 处
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

// GL_MIRRORED_REPEAT: 镜像重复
// UV = 1.2 → 取纹素 0.8 处（从右侧镜像）
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);

// GL_CLAMP_TO_EDGE: 超出范围夹紧到边缘纹素
// UV = 1.5 → 取最右边纹素
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

// GL_CLAMP_TO_BORDER: 超出范围使用指定边框颜色
GLfloat border_color[] = {1.0f, 0.0f, 0.0f, 1.0f}; // 红色
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
```

---

## 8. 纹理单元（Texture Units）

### 8.1 概念

GPU 着色器通过**纹理单元**（Texture Unit）访问纹理，而不是直接访问纹理对象。
纹理单元是 GPU 中的一个绑定槽位，采样器 uniform 变量存储的是纹理单元编号（0-15+）。

```
着色器中:  uniform sampler2D uTex;  // 存储纹理单元编号，不是纹理 ID
            texture(uTex, uv)        // 从对应纹理单元采样

C++ 中:    glUniform1i(loc, 0);     // 告诉 uTex 使用纹理单元 0
            glActiveTexture(GL_TEXTURE0);  // 激活纹理单元 0
            glBindTexture(GL_TEXTURE_2D, tex_id); // 将纹理绑定到单元 0
```

### 8.2 最大纹理单元数

```cpp
GLint max_units;
glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_units); // 片段着色器可用单元数（最少 16）
glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_units); // 总计（通常 192+）
```

OpenGL 规范保证片段着色器中至少 16 个纹理单元，现代 GPU 通常支持 32 或更多。

### 8.3 多纹理示例

```cpp
// 绑定两张纹理
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, diffuse_tex);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, normal_tex);

// 告诉着色器各 sampler 对应哪个单元
shader.set_int("uDiffuse", 0);
shader.set_int("uNormal",  1);
```

---

## 9. 纹理压缩格式

纹理压缩可以减少 GPU 显存占用和带宽消耗，同时保持实时解压缩能力。

### 9.1 BC（Block Compression）系列

所有 BC 格式都以 **4×4 像素块**为单位压缩，GPU 可以硬件加速解压。

| 格式 | OpenGL 名称 | 压缩比 | 用途 |
|------|-------------|--------|------|
| BC1 (DXT1) | `GL_COMPRESSED_RGB_S3TC_DXT1_EXT` | 6:1 | 漫反射（无 alpha） |
| BC2 (DXT3) | `GL_COMPRESSED_RGBA_S3TC_DXT3_EXT` | 4:1 | 带明显 alpha（如植被） |
| BC3 (DXT5) | `GL_COMPRESSED_RGBA_S3TC_DXT5_EXT` | 4:1 | 带平滑 alpha，法线贴图 |
| BC4 | `GL_COMPRESSED_RED_RGTC1` | 2:1 | 单通道（粗糙度/遮蔽） |
| BC5 | `GL_COMPRESSED_RG_RGTC2` | 2:1 | 双通道法线贴图（XY） |
| BC6H | `GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT` | ~4:1 | HDR 纹理 |
| BC7 | `GL_COMPRESSED_RGBA_BPTC_UNORM` | ~4:1 | 高质量漫反射/特殊纹理 |

### 9.2 移动平台格式

| 格式 | 平台 | 说明 |
|------|------|------|
| ETC2 | Android/iOS（OpenGL ES 3.0+） | 取代 ETC1，支持 alpha |
| ASTC | ARM Mali, Apple（OpenGL ES 3.2+） | 可变块大小，最灵活 |
| PVRTC | PowerVR（旧 iOS） | 逐渐被 ASTC 取代 |

### 9.3 在 OpenGL 中使用压缩纹理

```cpp
// 方法1：上传未压缩数据，让驱动在线压缩（质量差，速度慢）
glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, w, h, 0,
             GL_RGB, GL_UNSIGNED_BYTE, raw_data);

// 方法2：直接上传已压缩的数据（推荐，使用离线工具如 crunch/nvcompress 压缩）
glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                       w, h, 0, compressed_data_size, compressed_data);
```

---

## 10. 常见坑与排查清单

### 坑 1：忘记 flip_y

**症状**：纹理上下颠倒
**原因**：PNG/JPEG 图片数据从顶行开始，OpenGL 期望从底行开始
**修复**：`stbi_set_flip_vertically_on_load(1)` 或在着色器中 `vUV.y = 1.0 - vUV.y`

### 坑 2：NPOT（非 2 的幂）纹理

**症状**：纹理显示为黑色（旧硬件/驱动）或环绕模式无效
**原因**：OpenGL ES 2.0 和旧版 OpenGL 对非 2 的幂纹理有限制
**修复**：在 OpenGL 4.6 Core Profile 中，NPOT 纹理完全支持；
但如果需要使用 GL_REPEAT，某些平台需要确保纹理是 2 的幂大小

### 坑 3：sRGB 双重矫正

**症状**：纹理颜色过亮或过暗（通常是偏亮）
**原因**：
- 将 sRGB 纹理用 `GL_RGB8` 加载（跳过线性化），但手动在着色器中做了矫正
- 或：用 `GL_SRGB8` 加载（自动线性化），但着色器中又做了矫正
**修复**：选择一条路：要么全部手动（统一 GL_RGB8 + 手动 pow(x, 2.2)），要么全自动（GL_SRGB8 + glEnable(GL_FRAMEBUFFER_SRGB)）

### 坑 4：未生成 mipmap 导致纹理黑色

**症状**：纹理显示为黑色
**原因**：设置了 `GL_LINEAR_MIPMAP_LINEAR` 等含 MIPMAP 的过滤模式，但没有调用 `glGenerateMipmap`
**修复**：在上传纹理数据后立即调用 `glGenerateMipmap(GL_TEXTURE_2D)`；
或将 MIN_FILTER 改为不需要 mipmap 的 `GL_LINEAR`

### 坑 5：纹理坐标超出范围导致边缘撕裂

**症状**：纹理边缘出现奇怪颜色条纹
**原因**：在 GL_CLAMP_TO_EDGE 下，UV 值非常接近 0 或 1 时会采样到相邻纹素
**修复**：对于图集（texture atlas）或精灵表，在 UV 范围内缩小一个半纹素的边距

### 坑 6：采样器类型不匹配

**症状**：GLSL 编译警告或采样错误
**原因**：将 `sampler2D` 绑定到 3D 纹理等
**修复**：确保 GLSL 中的采样器类型与 C++ 中绑定的纹理目标一致

### 坑 7：多个绑定到同一纹理单元

**症状**：渲染结果不正确，某些 pass 使用错误纹理
**原因**：忘记更新 `glActiveTexture` 就绑定了新纹理
**修复**：养成习惯：每次绑定纹理前都调用 `glActiveTexture`

---

## 11. 本模块代码详解

### 11.1 文件结构

```
module07_texture/
├── CMakeLists.txt
├── include/
│   ├── shader.h          ← Shader 工具类
│   ├── mesh.h            ← Mesh 工具类（含 make_quad/cube/sphere）
│   └── texture.h         ← Texture2D 声明
├── src/
│   ├── main.cpp          ← 演示主程序
│   ├── texture.cpp       ← Texture2D 实现
│   └── gen_test_textures.cpp ← 生成测试纹理工具
├── shaders/
│   ├── texture.vert      ← 顶点着色器
│   └── texture.frag      ← 片段着色器（含多纹理混合）
└── assets/               ← 测试纹理（由 gen_test_textures 生成）
```

### 11.2 Texture2D 封装设计

`Texture2D::from_file` 的关键流程：

```cpp
// 1. 翻转 Y 轴（OpenGL 左下原点）
stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

// 2. 加载像素数据（自动处理 JPEG/PNG/BMP/HDR）
unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

// 3. 根据通道数选择格式
GLenum internal_fmt = srgb ? GL_SRGB8 : GL_RGB8;

// 4. 上传到 GPU
glGenTextures(1, &id);
glBindTexture(GL_TEXTURE_2D, id);
glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, data);

// 5. 生成 mipmap（避免黑色纹理问题）
glGenerateMipmap(GL_TEXTURE_2D);

// 6. 设置过滤和各向异性
glTexParameteri(..., GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
GLfloat max_aniso;
glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);
glTexParameterf(..., GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

// 7. 释放 CPU 内存
stbi_image_free(data);
```

### 11.3 演示模式说明

| 按键 | 模式 | 说明 |
|------|------|------|
| `1` | 单纹理 | 棋盘格纹理贴到全屏矩形 |
| `2` | 多纹理混合 | `↑/↓` 调整混合比例（0=全棋盘格，1=全渐变） |
| `3` | 过滤对比 | 四个子矩形展示 NEAREST/LINEAR/CLAMP/REPEAT |
| `4` | Mipmap 演示 | 同一纹理以不同缩放比例渲染，GPU 自动选择 mip 层 |

### 11.4 多纹理 GLSL

```glsl
// texture.frag 核心
uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform float     uMixRatio;

void main() {
    vec4 c0 = texture(uTex0, vUV);
    vec4 c1 = texture(uTex1, vUV);
    FragColor = mix(c0, c1, uMixRatio); // 线性插值
}
```

`mix(a, b, t)` 等价于 `a*(1-t) + b*t`，t=0 时返回 a，t=1 时返回 b。

### 11.5 STB_IMAGE_IMPLEMENTATION 宏

stb_image 是 header-only 库，但实现代码需要恰好在**一个**翻译单元中包含：

```cpp
// 在 texture.cpp 中（或任意一个 .cpp 文件）
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

CMakeLists.txt 通过 `target_compile_definitions` 注入这个宏：

```cmake
target_compile_definitions(module07_texture PRIVATE STB_IMAGE_IMPLEMENTATION)
```

这样 `texture.cpp` 在编译时会包含完整实现，其他 `.cpp` 文件只需 `#include <stb_image.h>` 即可使用声明，不会重复定义。

---

*End of Module 07 README*
