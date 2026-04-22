# Module 16 — YUV420P OpenGL 渲染器

> **cpp_meet 直接关联**：本模块是 `cpp_meet/module10_qt_client` 中 `YuvRenderer` 类的独立 OpenGL 实现版本。GLSL 着色器代码与 Qt 项目完全一致；区别仅在 OpenGL 上下文管理（本模块用 GLFW，Qt 项目用 `QOpenGLWidget::makeCurrent()`）。集成方法见第 10 节。

---

## 目录

1. [概述](#1-概述)
2. [YUV420P 格式详解](#2-yuv420p-格式详解)
3. [为什么解码器输出 YUV 而不是 RGB](#3-为什么解码器输出-yuv-而不是-rgb)
4. [三纹理方案 vs CPU 转换方案](#4-三纹理方案-vs-cpu-转换方案)
5. [GL_R8 纹理格式](#5-gl_r8-纹理格式)
6. [BT.601 vs BT.709 颜色标准](#6-bt601-vs-bt709-颜色标准)
7. [Studio Swing vs Full Range](#7-studio-swing-vs-full-range)
8. [完整颜色转换矩阵推导](#8-完整颜色转换矩阵推导)
9. [PBO 异步上传原理](#9-pbo-异步上传原理)
10. [与 cpp_meet module10_qt_client 的集成](#10-与-cpp_meet-module10_qt_client-的集成)
11. [常见坑与调试技巧](#11-常见坑与调试技巧)

---

## 1. 概述

本模块实现一个独立的 **YUV420P → RGB OpenGL 渲染器**，功能：

- 读取/生成 YUV420P 测试帧（彩色条纹，动态亮度波动）
- **三纹理方案**：Y/U/V 三个平面各对应一张 `GL_R8` 单通道纹理
- 在 Fragment Shader 中完成 YCbCr → RGB 转换（全 GPU，零 CPU 转换开销）
- 支持 **BT.601 / BT.709** 色彩空间切换
- 支持 **Studio Swing (16-235) / Full Range (0-255)** 切换
- **双 PBO 乒乓**异步上传，演示有无 PBO 的 CPU 占用对比
- 窗口标题显示帧率和上传延迟（微秒）

### 控制键

| 键 | 功能 |
|---|---|
| P | 切换 PBO / 同步上传 |
| C | BT.601 ↔ BT.709 |
| R | Studio ↔ Full Range |
| ESC | 退出 |

---

## 2. YUV420P 格式详解

### 2.1 三平面内存布局

YUV420P（也称 I420）是最常见的视频解码器输出格式：

```
┌──────────────────────────────────────────────────────────────────┐
│                  Y 平面（亮度）                                    │
│                  大小：width × height 字节                         │
│  每个像素对应一个 Y 值，全分辨率                                    │
└──────────────────────────────────────────────────────────────────┘
┌────────────────────────────────────┐
│            U/Cb 平面（蓝色差）      │
│            大小：(width/2) × (height/2) 字节                       │
│  每 2×2 像素块共享一个 U 值（二次采样）                              │
└────────────────────────────────────┘
┌────────────────────────────────────┐
│            V/Cr 平面（红色差）      │
│            大小：(width/2) × (height/2) 字节                       │
└────────────────────────────────────┘
```

### 2.2 各平面大小计算

以 1920×1080 为例：

```
Y  平面：1920 × 1080       = 2,073,600 字节 ≈ 2.0 MB
U  平面：960  × 540        =   518,400 字节 ≈ 0.5 MB
V  平面：960  × 540        =   518,400 字节 ≈ 0.5 MB
─────────────────────────────────────────────────────
总计：                     = 3,110,400 字节 ≈ 3.0 MB / 帧
```

这是 RGB24 帧（6 MB）大小的 50%。

### 2.3 4:2:0 色度二次采样示意

```
像素 (0,0)  像素 (1,0)  像素 (0,1)  像素 (1,1)
  Y[0,0]     Y[1,0]      Y[0,1]      Y[1,1]
    └──────────┴──────────────┴──── 共享 U[0,0] 和 V[0,0]
```

人眼对亮度的分辨率远高于色度，因此 4:2:0 采样在主观质量上与 4:4:4 差别极小，但数据量减少 50%。

### 2.4 内存地址计算

```cpp
// 给定帧指针 frame_data：
const uint8_t* y = frame_data;
const uint8_t* u = frame_data + width * height;
const uint8_t* v = frame_data + width * height + (width/2) * (height/2);

// 特定像素的 Y 值：
uint8_t y_val = y[row * width + col];

// 对应的 UV 值（注意下采样）：
uint8_t u_val = u[(row/2) * (width/2) + (col/2)];
uint8_t v_val = v[(row/2) * (width/2) + (col/2)];
```

---

## 3. 为什么解码器输出 YUV 而不是 RGB

### 3.1 YUV 是视频信号的原生格式

历史原因：模拟电视时代，黑白电视只用 Y 信号（亮度），彩色信号在 Y 基础上加 U/V 色差。YUV 格式天然兼容黑白设备。

### 3.2 YUV 在视频压缩中的效率优势

H.264/H.265/VP9 等视频编解码器在 **YCbCr 空间**中进行 DCT 变换和量化：

1. **人眼对亮度更敏感**：Y 通道用更高的量化精度，UV 通道可以更激进地量化/丢弃高频信息
2. **去相关性**：YUV 三通道相关性远低于 RGB 三通道，DCT 变换效率更高
3. **色度下采样（4:2:0）**：UV 分辨率减半后主观质量几乎无损，直接节省 33% 数据量

### 3.3 CPU 转换的代价

1080p@60fps 的 CPU YUV→RGB 转换：
- 数据量：3 MB × 60 = 180 MB/s 读取
- 计算量：每像素约 6 次浮点乘加
- 单线程：约 15-20ms/帧（占用一整个核心）

GPU 在 Fragment Shader 中转换：每帧约 0.1ms，几乎可忽略。

---

## 4. 三纹理方案 vs CPU 转换方案

### 4.1 CPU 转换方案

```
CPU:  YUV → RGB 转换  →  glTexImage2D(RGB)  →  GPU 渲染
      ↑ 每帧 ~15ms     ↑ 传输 6MB/帧
```

缺点：
- CPU 占用高（15-20ms/帧）
- PCIe 带宽：6MB/帧 × 60fps = 360 MB/s

### 4.2 三纹理方案（本模块）

```
CPU:  glTexSubImage2D(Y), glTexSubImage2D(U), glTexSubImage2D(V)
      传输 3MB/帧（节省 50%）
           ↓
GPU:  Fragment Shader 执行 YCbCr → RGB（每像素 6 次浮点运算）
      GPU 浮点性能极高，几乎不增加帧时间
```

优点：
- CPU 完全不做颜色转换
- PCIe 带宽减半（3MB vs 6MB）
- GPU 并行处理所有像素

### 4.3 带宽分析

| 方案 | PCIe 带宽/帧 | PCIe 带宽@60fps | CPU 占用/帧 |
|---|---|---|---|
| CPU 转换 | 6 MB (RGB) | 360 MB/s | ~15ms |
| 三纹理（同步）| 3 MB (YUV) | 180 MB/s | <1ms |
| 三纹理 + PBO | 3 MB (YUV, 异步) | 180 MB/s | <0.1ms（主线程）|

---

## 5. GL_R8 纹理格式

### 5.1 为什么用 GL_R8 而不是 GL_LUMINANCE

`GL_LUMINANCE` 是 OpenGL 2.x 的旧格式，在 OpenGL 3.0+ Core Profile 中**已废弃**：
- `GL_LUMINANCE` 会将单通道值复制到 RGB 三个分量（`sample(tex).rgb = (v, v, v)`）
- 在 Core Profile 中使用会产生驱动警告或错误

`GL_R8` 是现代做法：
```cpp
// 正确（OpenGL 3.0+ Core Profile）
glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
// 采样结果：texture(tex, uv).r = 值，.g = 0, .b = 0, .a = 1

// 废弃（不要用）
glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
```

### 5.2 Swizzle 参数

如果需要 `texture(tex, uv).rgb` 都等于 Y 值（便于调试），可以设置 swizzle：
```cpp
GLint swizzle[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
```

本模块直接在 GLSL 中使用 `.r`，不需要 swizzle。

---

## 6. BT.601 vs BT.709 颜色标准

### 6.1 标准来源

| 标准 | 用途 | 主色坐标（CIE xy）|
|---|---|---|
| ITU-R BT.601 | 标清（SD）：480i/576i、DVD | R(0.640,0.330) G(0.290,0.600) B(0.150,0.060) |
| ITU-R BT.709 | 高清（HD）：720p/1080p | R(0.640,0.330) G(0.300,0.600) B(0.150,0.060) |

BT.601 和 BT.709 的 R/B 主色相同，G 略有差异，导致转换矩阵系数不同。

### 6.2 为什么混用会出现颜色偏差

如果视频是 BT.709 编码，却用 BT.601 矩阵解码：
- 绿色通道偏差约 5%
- 皮肤色调偏黄

混用是视频播放器最常见的颜色错误原因之一，需要从容器的元数据（如 H.264 的 VUI parameters）获取正确的色彩空间标识。

### 6.3 BT.2020（HDR 视频）

4K HDR 内容使用 BT.2020 标准，主色坐标更宽，覆盖更大色域。本模块未实现，但扩展方法：在 GLSL 中增加第三个矩阵分支。

---

## 7. Studio Swing vs Full Range

### 7.1 历史原因

广播电视标准（SMPTE）为了保留信号过冲/欠冲的余量，将 8 位 Y 值限制在 16-235（而不是 0-255），UV 值限制在 16-240（中心 128）。这被称为 **Studio Swing**（或 Limited Range）。

### 7.2 两种范围的字节值对应

| 含义 | Studio Swing（Limited）| Full Range |
|---|---|---|
| Y 最暗（黑色）| 16 | 0 |
| Y 最亮（白色）| 235 | 255 |
| UV 中性（无色差）| 128 | 128 |
| UV 最大偏移 | ±112（到 16/240）| ±127.5（到 0/255）|

### 7.3 GLSL 归一化

```glsl
// Studio Swing
y = (y - 16.0/255.0)  / (219.0/255.0);   // 219 = 235 - 16
u = (u - 128.0/255.0) / (112.0/255.0);   // 112 = 240 - 128
v = (v - 128.0/255.0) / (112.0/255.0);

// Full Range
// y 已在 [0,1]，不需要 Y 偏移
u -= 0.5;  // 中心移到 0
v -= 0.5;
```

### 7.4 如何判断视频使用哪种范围

H.264：查看 VUI parameters 中的 `video_full_range_flag`（0=Studio, 1=Full）。
实际上：大多数 MP4/MKV 视频是 Studio Swing；屏幕录像/手机视频常是 Full Range。

---

## 8. 完整颜色转换矩阵推导

### 8.1 YCbCr 定义（BT.601）

ITU-R BT.601 定义的 RGB → YCbCr 转换：

```
Wr = 0.299,  Wg = 0.587,  Wb = 0.114   （亮度权重，和为 1）

Y  =  Wr * R + Wg * G + Wb * B
Cb = (B - Y) / (2 * (1 - Wb))   =  -0.168736 R - 0.331264 G + 0.5 B
Cr = (R - Y) / (2 * (1 - Wr))   =   0.5 R - 0.418688 G - 0.081312 B
```

### 8.2 逆变换（YCbCr → RGB，BT.601）

从定义出发推导逆变换：

```
R = Y + 2*(1-Wr) * Cr               = Y + 1.402   * Cr
B = Y + 2*(1-Wb) * Cb               = Y + 1.772   * Cb
G = (Y - Wr*R - Wb*B) / Wg          = Y - 0.344136 * Cb - 0.714136 * Cr
```

矩阵形式（Full Range，Cb/Cr 已减去 0.5）：

```
┌R┐   ┌1    0        1.402   ┐ ┌Y ┐
│G│ = │1  -0.344136 -0.714136│ │Cb│
└B┘   └1    1.772    0       ┘ └Cr┘
```

### 8.3 BT.709 的系数

```
Wr = 0.2126, Wg = 0.7152, Wb = 0.0722

R = Y + 1.5748  * Cr
G = Y - 0.1873  * Cb - 0.4681 * Cr
B = Y + 1.8556  * Cb
```

### 8.4 GLSL 实现

```glsl
// BT.601
rgb.r = y                      + 1.402    * v;
rgb.g = y - 0.344136 * u - 0.714136 * v;
rgb.b = y + 1.772    * u;

// BT.709
rgb.r = y                     + 1.5748  * v;
rgb.g = y - 0.1873  * u - 0.4681  * v;
rgb.b = y + 1.8556  * u;
```

---

## 9. PBO 异步上传原理

### 9.1 同步上传的问题

```cpp
// 同步上传：CPU 必须等待 GPU 完成 DMA 传输
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, cpu_data);
// 这一行会阻塞，直到 GPU 完成数据拷贝
```

### 9.2 PBO（Pixel Buffer Object）工作原理

PBO 是 GPU 内存中的缓冲区（但可以 map 到 CPU 地址空间）：

```
step 1：CPU 写 PBO（glMapBufferRange + memcpy）
─────────────────────────────────────────────────
   CPU 侧操作，写入 PBO 在系统内存（AGP/pinned）的映射区域
   这一步不触碰 GPU 渲染管线

step 2：glTexSubImage2D 从 PBO 传输（异步 DMA）
─────────────────────────────────────────────────
   GPU DMA 引擎从 PBO 拷贝到纹理（texel store）
   CPU 立即返回，不等待 DMA 完成

step 3：Fragment Shader 使用纹理时，DMA 已经完成
```

### 9.3 双 PBO 乒乓

```
帧 N：
  CPU: memcpy → PBO[0]  （写当前帧到 PBO[0]）
  GPU: PBO[1] → Texture  （从上帧 PBO[1] 传输到纹理，异步）

帧 N+1：
  CPU: memcpy → PBO[1]  （写当前帧到 PBO[1]）
  GPU: PBO[0] → Texture  （从上帧 PBO[0] 传输到纹理，异步）
```

时序图：
```
帧 N  :  [CPU: copy→PBO0] [GPU: PBO1→Tex] [GPU: Render]
帧 N+1:                   [CPU: copy→PBO1] [GPU: PBO0→Tex] [GPU: Render]
                                             ↑ 重叠！CPU 写 PBO1 时 GPU 同时做 DMA
```

### 9.4 实现细节

```cpp
// glMapBufferRange 的标志
GL_MAP_WRITE_BIT              // 写入模式
GL_MAP_INVALIDATE_BUFFER_BIT  // 声明丢弃之前内容（允许驱动复用内存，避免同步）
```

`GL_MAP_INVALIDATE_BUFFER_BIT` 是关键——它告诉驱动可以分配一块新内存给 PBO，而不必等待之前的 DMA 完成后再覆盖。

### 9.5 性能数据（1080p YUV）

| 方式 | CPU 阻塞时间/帧 | 主线程 stall |
|---|---|---|
| 同步 glTexSubImage2D | ~2ms（等待 DMA）| 明显 |
| 双 PBO | <0.1ms（只有 memcpy）| 极小 |

---

## 10. 与 cpp_meet module10_qt_client 的集成

> **重要关联**：本模块的核心 GLSL 代码（`yuv.frag`）与 `cpp_meet/module10_qt_client` 中 Qt YuvRenderer 的 fragment shader **完全一致**。

### 10.1 cpp_meet/module10_qt_client 的 YuvRenderer 概述

Qt 项目中的 YUV 渲染器继承自 `QOpenGLWidget`：
```cpp
// cpp_meet/module10_qt_client/src/yuv_renderer.h
class YuvRenderer : public QOpenGLWidget {
    Q_OBJECT
public:
    void setFrame(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                  int width, int height);
protected:
    void initializeGL()   override;
    void resizeGL(int w, int h) override;
    void paintGL()        override;
private:
    // 与本模块相同的成员
    GLuint tex_y_, tex_u_, tex_v_;
    GLuint prog_;
    // ...
};
```

### 10.2 GLSL 代码迁移

本模块的 `yuv.frag` 可直接复制到 Qt 项目，唯一修改是 OpenGL 版本声明（Qt 可能用 4.1 或 ES 3.0）：

```glsl
// Qt OpenGL Widget 通常限制在 4.1（或 ES 3.0）
// 将 #version 460 改为 #version 410 或 #version 300 es
#version 410 core
// 其余代码完全相同
```

### 10.3 PBO 在 Qt 中的使用

Qt 的 `QOpenGLWidget` 在 `paintGL()` 中拥有当前上下文，PBO 操作与本模块完全相同：
```cpp
void YuvRenderer::paintGL() {
    // 此时已有 makeCurrent，可以直接调用 GL 函数
    upload_frame_pbo(y_, u_, v_);
    render();
}
```

关键注意事项：`glMapBufferRange` 和 `glUnmapBuffer` 必须在 `makeCurrent()` 之后调用。Qt 应用程序在非 `paintGL`/`resizeGL`/`initializeGL` 回调中调用 GL 函数时，必须手动调用 `makeCurrent()`。

### 10.4 Qt 集成完整示例

```cpp
// 在 setFrame 中存储数据，paintGL 中上传
void YuvRenderer::setFrame(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                            int width, int height) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    // 拷贝帧数据（解码线程可能与渲染线程不同）
    y_buf_.assign(y, y + width * height);
    u_buf_.assign(u, u + (width/2) * (height/2));
    v_buf_.assign(v, v + (width/2) * (height/2));
    frame_ready_ = true;
    update();  // 触发 Qt 重绘（线程安全）
}

void YuvRenderer::paintGL() {
    if (!frame_ready_) return;
    std::lock_guard<std::mutex> lock(frame_mutex_);
    upload_frame_pbo(y_buf_.data(), u_buf_.data(), v_buf_.data());
    render();
    frame_ready_ = false;
}
```

---

## 11. 常见坑与调试技巧

### 坑 1：使用 GL_LUMINANCE 导致 Core Profile 错误

```
Error: GL_INVALID_ENUM (GL_LUMINANCE not supported in Core Profile)
```

解决：改用 `GL_R8` + `GL_RED`，在 GLSL 中用 `.r` 分量。

### 坑 2：UV 平面尺寸写错

常见错误：
```cpp
// 错误！U/V 平面是 w/2 × h/2，不是 w × h
glTexImage2D(..., GL_R8, width, height, ...)  // 错误
glTexImage2D(..., GL_R8, width/2, height/2, ...)  // 正确
```

### 坑 3：PBO 忘记 unmap

```cpp
// glMapBufferRange 后必须 unmap，否则后续 GL 调用会崩溃
void* ptr = glMapBufferRange(...);
if (ptr) {
    memcpy(ptr, data, size);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);  // 必须！
}
```

### 坑 4：颜色偏差（最常见）

症状：图像偏绿、偏红，或整体偏暗。

诊断：
1. 检查 Studio/Full Range 是否匹配（最常见：视频是 Studio Swing，但按 Full Range 处理）
2. 检查 BT.601/BT.709 是否匹配
3. 使用纯白（Y=235 Studio / Y=255 Full）测试帧，RGB 应为 (1,1,1)

### 坑 5：GL_UNPACK_ALIGNMENT 导致行错位

YUV 平面的宽度不一定是 4 的倍数（如 1280 的 UV 平面宽度 640 正好整除，但其他宽度如 1366 的 UV=683 不整除）：

```cpp
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // 关键！禁用行对齐
glTexSubImage2D(...);
```

### 坑 6：PBO 在双缓冲中的竞争

如果 upload 和 render 分属不同线程（如 Qt 的渲染线程和解码线程），需要用 fence 或互斥锁保护：
```cpp
// 解码线程写 CPU 缓冲，渲染线程从 CPU 缓冲上传 PBO
// 不能让两个线程同时操作同一 PBO
```

### 调试技巧：纯色测试

生成已知值的测试帧来验证颜色正确性：
```cpp
// BT.601 Studio Swing 纯红色：Y=81, U=90, V=240
// BT.601 Studio Swing 纯绿色：Y=145, U=54, V=34
// BT.601 Studio Swing 纯蓝色：Y=41, U=240, V=110
```

---

*Module 16 — YUV420P Renderer | ogl_mastery 课程第五阶段 | 关联：cpp_meet/module10_qt_client*
