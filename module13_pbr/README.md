# Module 13 — Physically Based Rendering (PBR)

> OpenGL 4.6 Core Profile · C++17
> 演示：5×5 材质球矩阵（roughness × metallic），IBL 天空盒，4 个点光源，ImGui 实时调节

---

## 目录

1. [辐射度量学基础](#1-辐射度量学基础)
2. [渲染方程](#2-渲染方程)
3. [Cook-Torrance BRDF](#3-cook-torrance-brdf)
4. [法线分布函数 D（GGX）](#4-法线分布函数-d-ggx)
5. [菲涅耳方程 F（Schlick）](#5-菲涅耳方程-f-schlick)
6. [几何遮蔽函数 G（Smith）](#6-几何遮蔽函数-g-smith)
7. [能量守恒与金属/非金属工作流](#7-能量守恒与金属非金属工作流)
8. [IBL — 图像照明](#8-ibl--图像照明)
9. [重要性采样 GGX](#9-重要性采样-ggx)
10. [BRDF 积分 LUT 生成](#10-brdf-积分-lut-生成)
11. [代码结构与编译](#11-代码结构与编译)

---

## 1. 辐射度量学基础

物理渲染建立在辐射度量学（Radiometry）之上。以下是核心量的定义与关系。

### 1.1 辐射通量 Φ（Radiant Flux）

$$\Phi \quad [\text{W = J/s}]$$

辐射通量是单位时间内穿过某一表面（或空间区域）的总辐射能量，即功率。对于点光源，其总辐射通量是向所有方向发射的功率之和。

### 1.2 辐照度 E（Irradiance）

$$E = \frac{d\Phi}{dA} \quad [\text{W/m}^2]$$

辐照度是单位面积上接收到的辐射通量。注意 $A$ 是垂直于光线方向的面积。当光线以角度 $\theta$ 照射到表面时，有效辐照度降低：

$$E_{\text{surface}} = E_{\perp} \cdot \cos\theta$$

这就是 Lambert 余弦定律的来源——渲染方程中的 $(\omega_i \cdot \mathbf{n})$ 因子。

### 1.3 辐射强度 I（Radiant Intensity）

$$I = \frac{d\Phi}{d\omega} \quad [\text{W/sr}]$$

辐射强度是单位立体角内的辐射通量。立体角 $\omega$ 的单位是球面度（steradian，sr）。

### 1.4 辐射率 L（Radiance）

$$L = \frac{d^2\Phi}{dA \cos\theta \, d\omega} \quad [\text{W/(m}^2\text{·sr)}]$$

辐射率是渲染中最核心的量。它描述了从某个方向 $\omega$ 穿过面元 $dA$ 的辐射功率密度（同时考虑了面积和立体角）。

**直觉**：想象一台相机，它的每个像素测量的就是从特定方向到达该像素的辐射率 $L$。

### 1.5 立体角与球面坐标

立体角 $d\omega$ 与球面坐标 $(\theta, \phi)$ 的关系：

$$d\omega = \sin\theta \, d\theta \, d\phi$$

其中 $\theta \in [0, \pi]$ 是极角（与法线的夹角），$\phi \in [0, 2\pi]$ 是方位角。

对整个单位球面积分：

$$\int_{\text{sphere}} d\omega = \int_0^{2\pi} \int_0^{\pi} \sin\theta \, d\theta \, d\phi = 4\pi$$

对上半球（$\theta \in [0, \pi/2]$）：

$$\int_{\Omega^+} d\omega = 2\pi$$

### 1.6 辐照度与辐射率的关系

$$E = \int_{\Omega^+} L(\omega_i) \cos\theta_i \, d\omega_i$$

这是渲染方程中 $(\omega_i \cdot \mathbf{n})$ 权重的物理来源：倾斜照射的光线在单位面积上"铺开"，有效功率降低。

---

## 2. 渲染方程

Kajiya（1986）提出的渲染方程统一了所有光传输现象：

$$L_o(\mathbf{p}, \omega_o) = L_e(\mathbf{p}, \omega_o) + \int_{\Omega^+} f_r(\mathbf{p}, \omega_i, \omega_o) \, L_i(\mathbf{p}, \omega_i) \, (\omega_i \cdot \mathbf{n}) \, d\omega_i$$

各项含义：

| 符号 | 含义 |
|------|------|
| $L_o(\mathbf{p}, \omega_o)$ | 从点 $\mathbf{p}$ 沿方向 $\omega_o$ 出射的辐射率 |
| $L_e(\mathbf{p}, \omega_o)$ | 自发光（Emissive），非发光材质为 0 |
| $f_r(\mathbf{p}, \omega_i, \omega_o)$ | BRDF：双向反射分布函数 |
| $L_i(\mathbf{p}, \omega_i)$ | 从方向 $\omega_i$ 入射的辐射率 |
| $(\omega_i \cdot \mathbf{n})$ | Lambert 余弦因子（$= \cos\theta_i$） |
| $\int_{\Omega^+}$ | 对上半球所有入射方向积分 |

### 2.1 BRDF 的物理约束

BRDF $f_r$ 必须满足两个物理约束：

**1. 非负性**：$f_r \geq 0$

**2. Helmholtz 互易律**（时间可逆性）：
$$f_r(\mathbf{p}, \omega_i, \omega_o) = f_r(\mathbf{p}, \omega_o, \omega_i)$$

**3. 能量守恒**（反射能量不超过入射能量）：
$$\int_{\Omega^+} f_r(\mathbf{p}, \omega_i, \omega_o) \cos\theta_i \, d\omega_i \leq 1 \quad \forall \omega_o$$

### 2.2 实时渲染的简化

实时渲染无法对整个半球进行真正的积分。常见简化策略：

- **直接光照**：将积分分解为有限个离散光源之和
- **IBL**：将环境光近似为辐射率场，预计算积分结果

$$L_o \approx \sum_k f_r(\omega_k, \omega_o) \, L_k \, \cos\theta_k + L_{\text{ambient}}$$

---

## 3. Cook-Torrance BRDF

Cook-Torrance BRDF 是实时 PBR 的工业标准，基于微表面（Microfacet）理论。

### 3.1 微表面理论

真实表面在微观尺度上并不光滑，而是由许多随机朝向的微小镜面（microfacet）组成。每个 microfacet 完全遵循镜面反射定律，但宏观上呈现出漫反射到光滑镜面的连续变化。

关键假设：只有法线方向等于**半角向量** $\mathbf{h}$ 的 microfacet 才能将光线 $\omega_i$ 反射到观察方向 $\omega_o$：

$$\mathbf{h} = \frac{\omega_i + \omega_o}{|\omega_i + \omega_o|}$$

### 3.2 完整公式

$$f_r = k_d \cdot \frac{c}{\pi} + k_s \cdot \frac{D \cdot F \cdot G}{4 \cdot (\mathbf{n} \cdot \omega_o) \cdot (\mathbf{n} \cdot \omega_i)}$$

| 项 | 名称 | 作用 |
|----|------|------|
| $D$ | 法线分布函数（NDF） | 控制 microfacet 的统计朝向分布 |
| $F$ | 菲涅耳方程 | 反射率随入射角变化 |
| $G$ | 几何遮蔽函数 | 统计 microfacet 相互遮蔽的比例 |
| $4 \cdot (n\cdot\omega_o) \cdot (n\cdot\omega_i)$ | 归一化因子 | 将微表面面积转换到宏观坐标系 |
| $k_d \cdot c/\pi$ | Lambert 漫反射 | 均匀散射进入次表面后重新出射 |

### 3.3 漫反射项的 $1/\pi$ 因子

Lambert BRDF 为 $f_{\text{Lambert}} = c/\pi$，其中 $c$ 是漫反射颜色（albedo）。
这个 $\pi$ 来自对上半球积分的归一化：

$$\int_{\Omega^+} \frac{c}{\pi} \cos\theta \, d\omega = c \cdot \frac{1}{\pi} \cdot \pi = c$$

即当入射辐照度为 1 时，反射出去的总能量恰好等于 $c$，满足能量守恒。

---

## 4. 法线分布函数 D（GGX）

GGX（Ground-Truth X，又称 Trowbridge-Reitz）NDF 是目前工业界最广泛使用的微表面分布。

### 4.1 公式

$$D_{\text{GGX}}(\mathbf{n}, \mathbf{h}, \alpha) = \frac{\alpha^2}{\pi \left[(\mathbf{n} \cdot \mathbf{h})^2 (\alpha^2 - 1) + 1\right]^2}$$

其中 $\alpha = \text{roughness}^2$（感知线性化，见下文）。

### 4.2 感知线性化：为什么 α = roughness²？

直接用 roughness 作为 $\alpha$ 会导致感知上的非线性：roughness 从 0.5 到 1.0 的变化远比 0.0 到 0.5 更显著。

Disney/Epic 的实验表明，令 $\alpha = \text{roughness}^2$ 可以使 roughness 参数在感知上近似线性，即 roughness = 0.5 视觉上处于完全光滑和完全粗糙的中间点。

### 4.3 GGX vs Blinn-Phong

| 特性 | GGX | Blinn-Phong |
|------|-----|-------------|
| 高光形状 | 长尾（heavy tail）| 指数衰减 |
| 物理性 | 来自微表面理论 | 经验公式 |
| 粗糙表面表现 | 自然、圆润 | 过于锐利 |
| 计算量 | 稍高 | 更低 |

GGX 的"长尾"特性更符合实际材料（如布料、皮肤）的高光形状。

### 4.4 GLSL 实现

```glsl
float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a     = roughness * roughness;  // α = roughness²
    float a2    = a * a;                  // α²
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}
```

---

## 5. 菲涅耳方程 F（Schlick）

菲涅耳方程描述了光线在两种介质界面处的反射率如何随入射角变化。

### 5.1 精确菲涅耳方程

对于无吸收介质，精确的 s 偏振和 p 偏振反射率为：

$$R_s = \left(\frac{n_1 \cos\theta_i - n_2 \cos\theta_t}{n_1 \cos\theta_i + n_2 \cos\theta_t}\right)^2$$

$$R_p = \left(\frac{n_1 \cos\theta_t - n_2 \cos\theta_i}{n_1 \cos\theta_t + n_2 \cos\theta_i}\right)^2$$

非偏振光的反射率：$R = (R_s + R_p) / 2$。

### 5.2 法向反射率 F₀ 推导

在法向入射时（$\theta_i = 0$），两式简化为：

$$F_0 = \left(\frac{n_1 - n_2}{n_1 + n_2}\right)^2$$

若从空气（$n_1 = 1$）入射到折射率为 $n$ 的材质：

$$F_0 = \left(\frac{n - 1}{n + 1}\right)^2$$

常见材质的 $F_0$：

| 材质 | $n$ | $F_0$ |
|------|-----|-------|
| 水 | 1.33 | 0.020 |
| 玻璃 | 1.5 | 0.040 |
| 钻石 | 2.4 | 0.172 |
| 铁（金属） | 复数折射率 | 约 0.56 |

非金属通用近似：$F_0 \approx 0.04$（玻璃）。

### 5.3 Schlick 近似

精确计算代价高，Schlick（1994）提出了精度足够的近似：

$$F(\omega_o, \mathbf{h}) = F_0 + (1 - F_0)(1 - \omega_o \cdot \mathbf{h})^5$$

物理直觉：当视线与半角向量接近平行（正面看）时，$F \to F_0$；当视线接近切线（掠射角）时，$F \to 1.0$（全反射）。

### 5.4 金属工作流中的 F₀

PBR 金属工作流统一了金属和非金属的处理方式：

$$F_0 = \text{lerp}(0.04, \text{albedo}, \text{metallic})$$

- **非金属（metallic = 0）**：$F_0 = 0.04$，albedo 用于漫反射颜色
- **金属（metallic = 1）**：$F_0 = \text{albedo}$（金属的折射率是复数，反射有色调），没有漫反射

### 5.5 带粗糙度的菲涅耳（IBL 环境光用）

标准 Schlick 假设表面完全光滑。对于 IBL，需要考虑粗糙度对掠射角菲涅耳的影响：

$$F_{\text{rough}}(\cos\theta, F_0, \text{roughness}) = F_0 + \left(\max(\mathbf{1} - \text{roughness}, F_0) - F_0\right)(1 - \cos\theta)^5$$

---

## 6. 几何遮蔽函数 G（Smith）

几何遮蔽函数统计了有多少 microfacet 因被相邻 microfacet 遮挡而不能被光源照到（阴影）或被观察者看到（遮蔽）。

### 6.1 Smith 分解

Smith（1967）将双向遮蔽分解为两个独立的单向遮蔽：

$$G(\mathbf{n}, \omega_o, \omega_i) = G_1(\mathbf{n}, \omega_o) \cdot G_1(\mathbf{n}, \omega_i)$$

其中 $G_1$ 是单方向（视线或光线方向）的遮蔽函数。

### 6.2 Schlick-GGX 近似

与 GGX NDF 配对使用的 Schlick 近似：

$$G_1(\mathbf{n}, \mathbf{v}, k) = \frac{\mathbf{n} \cdot \mathbf{v}}{(\mathbf{n} \cdot \mathbf{v})(1 - k) + k}$$

其中 $k$ 的取值根据使用场景不同：

$$k_{\text{direct}} = \frac{(\alpha + 1)^2}{8}, \quad k_{\text{IBL}} = \frac{\alpha^2}{2}$$

### 6.3 两个 k 值的来源

**$k_{\text{direct}}$** 来自 Schlick 的原始论文，通过匹配精确 Smith-GGX 遮蔽函数的积分行为推导得到。分子 $(\alpha+1)^2$ 中的 $+1$ 项是针对直接光照"热点"（hotspot）问题的修正——当 roughness = 0 时，$k=1/8$ 避免了 $G$ 在正入射时等于 1（太过光滑）的问题。

**$k_{\text{IBL}}$** 使用 $\alpha^2/2$ 是因为 IBL 已经对入射光方向做了积分，不存在直接光照的热点问题，更接近 Schlick 的原始推导 $k = \alpha/2$（在感知线性空间中，$\alpha = \text{roughness}^2$，所以最终是 $k = \text{roughness}^4 / 2$，近似为 $\alpha^2/2$）。

**实际影响**：在高 roughness 时，两种 $k$ 值会导致明显不同的亮度。使用错误的 $k$ 会让直接光照和环境光照之间出现能量不一致。

### 6.4 完整 G 函数

$$G(\mathbf{n}, \omega_o, \omega_i, \alpha) = G_1(\mathbf{n}, \omega_o, k) \cdot G_1(\mathbf{n}, \omega_i, k)$$

### 6.5 GLSL 实现

```glsl
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;  // k_direct
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(NdotV, roughness) *
           geometry_schlick_ggx(NdotL, roughness);
}
```

---

## 7. 能量守恒与金属/非金属工作流

### 7.1 kS 与 kD 的关系

Cook-Torrance BRDF 的能量守恒要求镜面反射和漫反射的贡献之和不超过 1。

菲涅耳项 $F$ 给出了镜面反射的比例 $k_s = F$，因此漫反射比例为：

$$k_d = 1 - k_s = 1 - F$$

### 7.2 金属的特殊处理

金属（导体）与非金属（绝缘体）的根本区别：

- **非金属**：光线部分反射（受菲涅耳约束），部分透射进入材质内部散射后以漫反射形式出射
- **金属**：自由电子会立即吸收并重新辐射光线，不存在次表面散射；所有漫反射能量都被吸收转化为热能

因此，对于金属（metallic = 1），漫反射贡献为零：

$$k_d^{\text{metal}} = 0$$

在代码中：

```glsl
vec3 kD = (1.0 - F) * (1.0 - uMetallic);  // 金属时 kD = 0
```

### 7.3 albedo 的双重角色

- **非金属**：albedo 是漫反射颜色（$F_0$ 固定为 0.04）
- **金属**：albedo 是 $F_0$（镜面反射颜色，金属有色泽的高光）

---

## 8. IBL — 图像照明

IBL（Image-Based Lighting）用真实场景的高动态范围图像（HDRI）作为全局光照来源，解决了纯点光源无法表现环境光和天空盒反射的问题。

### 8.1 辐照度积分

在点 $\mathbf{p}$ 处，法线方向为 $\mathbf{n}$，环境光贡献（漫反射 IBL）是：

$$E(\mathbf{n}) = \int_{\Omega^+} L_{\text{env}}(\omega_i) \, (\omega_i \cdot \mathbf{n}) \, d\omega_i$$

这个积分只依赖于 $\mathbf{n}$，可以**预先对每个方向计算并存储到 Cubemap**，称为辐照度图（Irradiance Map）。

渲染时只需一次 cubemap 采样：
```glsl
vec3 irradiance = texture(uIrradianceMap, N).rgb;
vec3 diffuse_ibl = irradiance * albedo * kD;
```

辐照度图通过对环境 cubemap 做半球积分卷积得到：
- 采样间隔 $\Delta\phi = \Delta\theta = 0.025$ 弧度
- 约 $251 \times 63 = 15813$ 个采样点
- 低分辨率（32×32）即可，因为积分本质上是低频信号

### 8.2 镜面 IBL — 分裂求和近似（Split-Sum）

镜面 IBL 积分比漫反射复杂，因为它依赖于观察方向 $\omega_o$ 和 roughness：

$$L_{\text{spec}}(\omega_o) = \int_{\Omega^+} f_r(\omega_i, \omega_o) \, L_{\text{env}}(\omega_i) \, \cos\theta_i \, d\omega_i$$

这个积分无法预计算成简单 cubemap（4D 函数）。Epic Games（Karis, 2013）提出了分裂求和近似：

$$\int_{\Omega^+} f_r \, L_i \cos\theta \, d\omega \approx \underbrace{\left(\int_{\Omega^+} L_i(\omega_i) d\omega_i\right)}_{\text{Part 1: Prefiltered Env}} \cdot \underbrace{\left(\int_{\Omega^+} f_r \cos\theta \, d\omega\right)}_{\text{Part 2: BRDF LUT}}$$

#### Part 1：预过滤环境贴图

$$\int_{\Omega^+} L_i d\omega_i \approx \frac{\sum_{k=1}^N L(\mathbf{l}_k) \, (\mathbf{n} \cdot \mathbf{l}_k)}{\sum_{k=1}^N (\mathbf{n} \cdot \mathbf{l}_k)}$$

重要性采样 GGX 分布，不同 roughness 值对应 cubemap 的不同 mip 级别（共 5 级）：

```
mip 0: roughness = 0.0 (镜面反射)
mip 1: roughness = 0.25
mip 2: roughness = 0.5
mip 3: roughness = 0.75
mip 4: roughness = 1.0 (完全漫射)
```

#### Part 2：BRDF 积分 LUT

BRDF 积分只依赖两个参数（NdotV 和 roughness），存储为 2D 纹理：

$$\int_{\Omega^+} \frac{f_r \cos\theta}{F} \, d\omega = F_0 \cdot \underbrace{\int ... (1 - F_c) G_{\text{vis}} \, d\omega}_{\text{scale（红通道）}} + \underbrace{\int ... F_c \, G_{\text{vis}} \, d\omega}_{\text{bias（绿通道）}}$$

渲染时组合：
```glsl
vec2 brdf = texture(uBrdfLut, vec2(NdotV, roughness)).rg;
vec3 specular_ibl = prefiltered * (F * brdf.r + brdf.g);
```

### 8.3 HDR 格式与 equirectangular 转换

HDRI 通常存储为等距柱状投影（equirectangular）格式的 `.hdr` 文件。

球面坐标到 UV 的映射：
$$u = \frac{\arctan(z, x)}{2\pi} + 0.5, \quad v = \frac{\arcsin(y)}{\pi} + 0.5$$

转换步骤：
1. 加载 `.hdr` 文件（stb_image，float 格式）
2. 绑定到 2D 纹理
3. 创建 FBO，对 cubemap 的 6 个面各渲染一次
4. 每次渲染时用 90° FOV 摄像机朝 6 个方向投影，片段着色器将方向向量转换为 UV 采样 HDR 贴图

---

## 9. 重要性采样 GGX

蒙特卡洛积分的效率取决于采样分布与被积函数形状的匹配程度。对于镜面 BRDF，GGX NDF 是主导项，因此使用 GGX 作为采样分布。

### 9.1 蒙特卡洛积分基础

$$\int f(x) dx \approx \frac{1}{N} \sum_{i=1}^N \frac{f(x_i)}{p(x_i)}$$

其中 $p(x)$ 是采样概率密度，$x_i$ 是按 $p$ 分布的随机样本。

### 9.2 GGX NDF 的逆 CDF

GGX NDF 在极角方向的 PDF 为（对方位角均匀积分后）：

$$p(\theta) = \frac{\alpha^2 \cos\theta \sin\theta}{\pi ((\alpha^2 - 1)\cos^2\theta + 1)^2}$$

对此 PDF 求逆 CDF（令 $\xi = \int_0^\theta p(\theta') d\theta'$）：

$$\cos\theta = \sqrt{\frac{1 - \xi}{(\alpha^2 - 1)\xi + 1}}$$

方位角均匀采样：$\phi = 2\pi \xi_2$

这就是 `importance_sample_ggx` 函数中的公式来源。

### 9.3 低差异序列（Hammersley）

蒙特卡洛积分用普通随机数会有高方差。低差异序列（QMC）通过保证样本均匀分布来降低方差。

Hammersley 序列：

$$\mathbf{x}_i = \left(\frac{i}{N}, \Phi_2(i)\right)$$

其中 $\Phi_2(i)$ 是 $i$ 的二进制位反转（Van der Corput 序列）：

```
i = 1  → binary: 001 → reversed: 100 → Φ₂(1) = 0.5
i = 2  → binary: 010 → reversed: 010 → Φ₂(2) = 0.25
i = 3  → binary: 011 → reversed: 110 → Φ₂(3) = 0.75
```

### 9.4 采样偏差补偿（Mip Bias）

直接用最高分辨率环境贴图采样重要性样本时，会出现亮斑。原因是：低概率样本（偏离 NDF 峰值）对应的辐射率方向上纹素数量少，采样到明亮纹素的概率被高估。

解决方法：根据 PDF 和纹素立体角计算合适的 mip 级别：

$$\text{mip} = \frac{1}{2} \log_2\left(\frac{\Omega_s}{\Omega_t}\right), \quad \Omega_s = \frac{1}{N \cdot p(\mathbf{l}_k)}, \quad \Omega_t = \frac{4\pi}{6 \cdot \text{res}^2}$$

---

## 10. BRDF 积分 LUT 生成

### 10.1 输入输出

- **输入**：NdotV（$u$ 轴），roughness（$v$ 轴）
- **输出**：RG16F 纹理，R = scale，G = bias

### 10.2 积分公式

将 Schlick 菲涅耳 $F = F_0 + (1-F_0)(1-v\cdot h)^5$ 代入镜面 BRDF 积分，令 $F_c = (1-v\cdot h)^5$：

$$\int_{\Omega^+} \frac{f_r}{F} \cos\theta \, d\omega = F_0 \underbrace{\int ... (1 - F_c) G_{\text{vis}} \, d\omega}_{A} + \underbrace{\int ... F_c \, G_{\text{vis}} \, d\omega}_{B}$$

其中 $G_{\text{vis}} = G \cdot \frac{v \cdot h}{(n \cdot h)(n \cdot v)}$ 是几何可见性项。

### 10.3 蒙特卡洛计算

使用 GGX 重要性采样，N = 1024 样本：

```glsl
for (uint i = 0u; i < 1024u; ++i) {
    vec2 Xi = hammersley(i, 1024u);
    vec3 H  = importance_sample_ggx(Xi, N, roughness);
    vec3 L  = normalize(2.0 * dot(V, H) * H - V);
    float NdotL = max(L.z, 0.0);
    if (NdotL > 0.0) {
        float G_vis = geometry_smith_ibl(...) * VdotH / (NdotH * NdotV);
        float Fc    = pow(1.0 - VdotH, 5.0);
        A += (1.0 - Fc) * G_vis;
        B += Fc * G_vis;
    }
}
return vec2(A, B) / 1024.0;
```

### 10.4 为什么 IBL 几何函数用 k = α²/2

BRDF LUT 的生成使用 $k_{\text{IBL}} = \alpha^2/2$ 而不是直接光照的 $k_{\text{direct}} = (\alpha+1)^2/8$。

原因在于 LUT 存储的是对**所有入射方向**积分的结果，没有特定的直接光源，不需要 hotspot 修正。若使用 $k_{\text{direct}}$，当 roughness 较低时积分结果偏暗，与预过滤环境贴图的亮度不匹配。

---

## 11. 代码结构与编译

### 11.1 文件结构

```
module13_pbr/
├── CMakeLists.txt
├── include/
│   ├── cubemap.h          # Cubemap 封装
│   └── brdf_lut.h         # BRDF LUT 生成函数
├── src/
│   ├── main.cpp           # 主程序，材质球矩阵演示
│   ├── cubemap.cpp        # IBL 预计算实现
│   └── brdf_lut.cpp       # BRDF 积分 LUT 生成
└── shaders/
    ├── pbr.vert            # PBR 顶点着色器
    ├── pbr.frag            # Cook-Torrance BRDF + IBL
    ├── equirect_to_cubemap.vert  # 公用立方体投影顶点
    ├── equirect_to_cubemap.frag  # HDR → Cubemap 转换
    ├── irradiance_conv.frag      # 辐照度卷积
    ├── prefilter_env.frag        # GGX 重要性采样预过滤
    ├── brdf_lut.frag             # BRDF 积分 LUT
    ├── skybox.vert               # 天空盒
    └── skybox.frag               # 天空盒
```

### 11.2 IBL 预计算流程

```
HDRI .hdr 文件
    │ stbi_loadf()
    ▼
equirect_tex (GL_TEXTURE_2D, GL_RGB16F)
    │ Cubemap::from_equirect()  [512×512, 6 faces]
    ▼
env_cubemap (GL_TEXTURE_CUBE_MAP)
    ├─► Cubemap::convolve_irradiance()  [32×32]
    │       └► irradiance_map
    └─► Cubemap::prefilter_env()        [128×128, 5 mips]
            └► prefilter_map

generate_brdf_lut()  [512×512, GL_RG16F]
    └► brdf_lut
```

### 11.3 编译

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target module13_pbr
./build/module13_pbr/module13_pbr
```

### 11.4 运行控制

| 操作 | 功能 |
|------|------|
| 右键拖拽 | 旋转摄像机 |
| WASD | 移动摄像机 |
| ImGui "Albedo" | 修改球体基础色 |
| ImGui "AO" | 修改环境光遮蔽 |
| ESC | 退出 |

### 11.5 HDR 环境贴图

程序在 `assets/hdr/newport_loft.hdr` 查找 HDR 文件。
可从 [Poly Haven](https://polyhaven.com/hdris) 下载免费 HDRI 放置到该路径。
若文件不存在，程序使用 2×2 的简单梯度作为 fallback，IBL 效果退化为均匀环境光。

### 11.6 性能说明

IBL 预计算（512px 环境 cubemap + 32px 辐照度 + 128px 预过滤 + 512px LUT）在启动时执行一次，约需 1-3 秒（取决于 GPU）。预计算完成后帧率应在 60fps 以上（5×5 = 25 个球体，25 个 draw call）。

如需加速，可将预计算结果缓存到文件（`.dds` 或原始浮点数组），下次启动直接加载。

### 11.7 关键数学量对照表

| 符号 | GLSL 变量 | 含义 |
|------|-----------|------|
| $\mathbf{n}$ | `N` | 表面法线（世界空间，归一化） |
| $\omega_o$ | `V` | 观察方向（$= -\text{fragDir}$） |
| $\omega_i$ | `L` | 光线方向 |
| $\mathbf{h}$ | `H` | 半角向量 |
| $\mathbf{r}$ | `R` | 反射方向 |
| $\alpha$ | `roughness*roughness` | GGX 粗糙度参数 |
| $F_0$ | `F0` | 法向入射菲涅耳反射率 |
| $k_s$ | `F` | 镜面反射系数（= 菲涅耳） |
| $k_d$ | `kD` | 漫反射系数（= 1-F，金属=0） |

---

*Module 13 实现了工业级 PBR 管线的核心部分。完整的生产管线还包括：高度图/AO 纹理、Clearcoat 层（车漆）、各向异性 BRDF（拉丝金属）、次表面散射（皮肤、蜡），以及更精确的多散射模型（Kulla-Conty）。*
