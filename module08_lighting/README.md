# Module 08 — 光照模型：从物理到实时渲染

## 目录

1. [辐射度量学基础](#1-辐射度量学基础)
2. [渲染方程](#2-渲染方程)
3. [Lambert 漫反射 BRDF 推导](#3-lambert-漫反射-brdf-推导)
4. [Phong 镜面模型](#4-phong-镜面模型)
5. [Blinn-Phong 改进](#5-blinn-phong-改进)
6. [三种光源类型的实现](#6-三种光源类型的实现)
7. [衰减公式](#7-衰减公式)
8. [聚光灯软边](#8-聚光灯软边)
9. [法向量变换：NormalMatrix](#9-法向量变换normalmatrix)
10. [常见坑与调试技巧](#10-常见坑与调试技巧)
11. [本模块代码详解](#11-本模块代码详解)

---

## 1. 辐射度量学基础

### 1.1 辐射通量（Radiant Flux）Φ

单位时间内辐射的总能量，单位：瓦特（W）。

```
Φ = dQ/dt
```

对于一个点光源，Φ 描述其向所有方向辐射的总功率。

### 1.2 立体角（Solid Angle）ω

立体角是平面角在三维空间的推广，描述"方向锥"的大小。

```
dω = dA / r²      （dA 为球面上的面积微元，r 为球半径）
```

单位：球面度（steradian, sr）。完整球面对应 4π 球面度。

### 1.3 辐照度（Irradiance）E

单位面积接收到的辐射功率（从所有方向到达），单位：W/m²。

```
E = dΦ / dA
```

如果表面法线与光源方向夹角为 θ，则有效辐照度按 cosθ 缩减（Lambert 余弦定律）：

```
E = E₀ · cos(θ) = E₀ · (n · l)
```

### 1.4 辐射率（Radiance）L

沿某特定方向、单位投影面积、单位立体角的辐射功率，单位：W/(m²·sr)。

```
L = d²Φ / (dA · cosθ · dω)
```

辐射率是光照计算中最核心的量——着色器中计算的就是到达摄像机的辐射率 L。

### 1.5 各量之间的关系

```
Φ (总功率)
  ↓ 除以面积 dA
E (辐照度，所有方向之和)
  ↓ 乘以 BRDF，除以立体角 dω
L (辐射率，单个方向)
```

---

## 2. 渲染方程

### 2.1 完整形式

由 Kajiya（1986）提出：

```
Lo(p, ωo) = Le(p, ωo) + ∫Ω fr(p, ωi, ωo) · Li(p, ωi) · cosθi · dωi
```

其中：
- `Lo(p, ωo)`：从点 p 沿方向 ωo 出射的辐射率（摄像机方向）
- `Le(p, ωo)`：点 p 的自发光
- `fr(p, ωi, ωo)`：BRDF（双向反射分布函数），描述从 ωi 入射的光有多少比例被反射到 ωo
- `Li(p, ωi)`：从 ωi 方向入射到 p 点的辐射率
- `cosθi`：Lambert 余弦因子（`n · ωi`）
- 积分范围 Ω：上半球（所有入射方向）

### 2.2 实时渲染的简化

实时渲染中无法计算完整积分，通常：
1. 用有限数量的光源代替积分（直接光照）
2. 用环境光（ambient）粗略模拟间接光照
3. BRDF 用 Lambertian + Phong/GGX 等简化模型代替

```
Lo(p, ωo) ≈ ambient + Σ_lights [diffuse(light_i) + specular(light_i)]
```

---

## 3. Lambert 漫反射 BRDF 推导

### 3.1 Lambert BRDF

理想漫反射（Lambertian）表面将光均匀地向所有方向散射，辐射率与出射方向无关。

Lambert BRDF 为常数：

```
fr_diffuse = ρ / π
```

其中 ρ（rho）是漫反射率（albedo），取值 [0,1]，表示反射光与入射光的比例。

### 3.2 能量守恒验证

对 BRDF 在上半球积分，必须 ≤ 1（否则违反能量守恒）：

```
∫Ω fr · cosθ · dω ≤ 1

对于 Lambertian BRDF：
∫Ω (ρ/π) · cosθ · dω
= (ρ/π) · ∫₀²π ∫₀^(π/2) cosθ · sinθ · dθ · dφ
= (ρ/π) · 2π · [sinθ/2]₀^(π/2)...
= (ρ/π) · π
= ρ ≤ 1  ✓
```

分子中的 `1/π` 正是来自半球余弦积分等于 π，以保证能量守恒。

### 3.3 在 GLSL 中

```glsl
// 漫反射计算
float NdotL = max(dot(normal, lightDir), 0.0);
vec3 diffuse = (albedo / PI) * PI * NdotL * lightColor;
// 化简：diffuse = albedo * NdotL * lightColor
// （PI 在 Lambert BRDF 的 1/PI 与 cos 积分 PI 中约掉）
```

实际代码中通常直接写 `albedo * NdotL * lightColor`，省略 `1/π` 因子，
这在非物理的 Phong 模型中是常规做法（PBR 中才严格包含）。

---

## 4. Phong 镜面模型

### 4.1 镜面反射向量

给定法线 **n** 和光源方向 **l**（指向光源），反射向量 **r** 为：

```
r = 2(n·l)n - l
```

推导：**l** 可以分解为沿 **n** 的分量和垂直于 **n** 的分量：

```
l = (n·l)n + l_perp
反射后：垂直分量不变，法线分量取反
r = -(n·l)n + l_perp
  = -(n·l)n + (l - (n·l)n)
  = 2(n·l)n - l
```

GLSL 内置函数 `reflect(-l, n)` 实现了这个计算（注意参数是入射方向，即 `-l`）。

### 4.2 Phong 镜面项

```
L_spec = Ks · (max(dot(r, v), 0))^shininess · L_light
```

- `v`：视线方向（指向摄像机）
- `shininess`（光泽度）：越大越接近镜面，高光越小越亮
- `Ks`：镜面反射率

shininess 的物理含义：粗略对应表面粗糙度的倒数。
- shininess = 1：非常粗糙，高光很宽散
- shininess = 256：非常光滑，高光非常集中

---

## 5. Blinn-Phong 改进

### 5.1 半程向量（Halfway Vector）

Blinn（1977）提出用**半程向量** **h** 代替反射向量：

```
h = normalize(l + v)
```

Blinn-Phong 镜面项：

```
L_spec = Ks · (max(dot(n, h), 0))^shininess
```

### 5.2 与 Phong 的等价关系

当视线方向 **v** 和光源方向 **l** 都在表面上方时：

```
dot(n, h) = cos(α/2)   其中 α = angle(r, v)
dot(r, v) = cos(α)
```

因为 `cos(α) = 2cos²(α/2) - 1`，两者在行为上相似，但：

- **Blinn-Phong 更物理**：半程向量 **h** 对应于表面微平面法线方向，
  更符合微平面理论（GGX BRDF 的基础）
- **Blinn-Phong 的 shininess 需要约 4× 才能得到与 Phong 相似的高光大小**
- **掠射角处行为更好**：Phong 在掠射角 (v·n ≈ 0) 时高光会突然消失，Blinn-Phong 没有这个问题

### 5.3 为什么现代引擎偏好 Blinn-Phong

即使在非 PBR 渲染中，Blinn-Phong 也比原始 Phong 更接近物理现实，
且计算量相近（都需要一次 dot product + pow），是默认选择。

---

## 6. 三种光源类型的实现

### 6.1 方向光（Directional Light）

模拟无限远处的光源（如太阳），所有片段的光源方向相同，没有衰减。

```glsl
struct DirLight {
    vec3 direction;           // 世界空间方向（指向光源，已归一化）
    vec3 ambient, diffuse, specular;
};

vec3 calc_dir_light(DirLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction); // 转为指向光源
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), uMatShininess);
    return light.ambient  * uMatAmbient
         + light.diffuse  * diff * uMatDiffuse
         + light.specular * spec * uMatSpecular;
}
```

### 6.2 点光源（Point Light）

向所有方向均匀发光，有位置，有衰减。

```glsl
struct PointLight {
    vec3  position;
    float constant, linear, quadratic;
    vec3  ambient, diffuse, specular;
};

vec3 calc_point_light(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), uMatShininess);
    float d = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear*d + light.quadratic*d*d);
    return (light.ambient  * uMatAmbient
          + light.diffuse  * diff * uMatDiffuse
          + light.specular * spec * uMatSpecular) * attenuation;
}
```

### 6.3 聚光灯（Spot Light）

有位置、方向、锥角限制。见第 8 节的软边实现。

---

## 7. 衰减公式

### 7.1 物理衰减：平方反比

真实物理中，点光源的辐照度随距离平方反比衰减：

```
E ∝ 1/d²
```

这来自球面面积公式 A = 4πr²——相同功率分布到更大球面上，密度下降。

### 7.2 游戏中的实用公式

纯 1/d² 有两个问题：
1. d=0 时无穷大（需要一个最小距离偏移）
2. 无法直接控制"影响半径"（当 d 很大时衰减极慢）

因此游戏引擎常用三项式：

```
attenuation = 1.0 / (Kc + Kl·d + Kq·d²)
```

| 系数 | 含义 | 典型值 |
|------|------|--------|
| Kc（constant） | 常数项，防止除以零，控制近处亮度上限 | 1.0 |
| Kl（linear） | 线性衰减系数，中距离主导 | 0.09 |
| Kq（quadratic） | 二次衰减系数，远距离主导 | 0.032 |

当 Kc=1, Kl=Kq=0 时退化为"不衰减"（方向光效果）。
当 Kc=0, Kl=0, Kq=1 时退化为物理平方反比，但需要手动偏移防止奇点。

### 7.3 参考衰减值表（Ogre3D 经验值）

| 影响半径（米） | Kc | Kl | Kq |
|------------|----|----|-----|
| 7 | 1.0 | 0.7 | 1.8 |
| 13 | 1.0 | 0.35 | 0.44 |
| 20 | 1.0 | 0.22 | 0.20 |
| 50 | 1.0 | 0.09 | 0.032 |
| 100 | 1.0 | 0.045 | 0.0075 |
| 200 | 1.0 | 0.022 | 0.0019 |

---

## 8. 聚光灯软边

### 8.1 硬边聚光灯

最简单的聚光灯：当片段在圆锥内时正常照亮，否则完全黑暗。

```glsl
float theta = dot(lightDir, normalize(-light.direction));
if (theta > light.cutOff) {
    // 在圆锥内，正常计算
} else {
    // 在圆锥外，只有环境光
}
```

注意：这里用的是 **cos 值**而非角度，因为 `dot(a,b) = cos(angle)` 且 cos 是减函数：
cos(0°) = 1，cos(30°) = 0.866，cos(90°) = 0。所以夹角越小，cos 值越大，
"在圆锥内"的条件是 `theta > cos(cutOffAngle)`。

### 8.2 软边推导

定义内锥角 `φ_inner`（完全照亮）和外锥角 `φ_outer`（完全黑暗）：

```
cutOff      = cos(φ_inner)
outerCutOff = cos(φ_outer)   // outerCutOff < cutOff（因为 φ_outer > φ_inner，cos 更小）
```

在内锥和外锥之间线性过渡（smoothstep）：

```glsl
float theta   = dot(lightDir, normalize(-light.direction));
float epsilon = light.cutOff - light.outerCutOff;

// 线性映射：theta=cutOff → 1.0，theta=outerCutOff → 0.0
float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
```

**推导验证**：
- 当 `theta = cutOff` 时：`intensity = (cutOff - outerCutOff) / epsilon = 1.0` ✓
- 当 `theta = outerCutOff` 时：`intensity = 0 / epsilon = 0.0` ✓
- 当 `theta > cutOff`（更里面）时：`intensity > 1.0` → clamp 到 1.0 ✓
- 当 `theta < outerCutOff`（更外面）时：`intensity < 0.0` → clamp 到 0.0 ✓

更平滑的过渡可以使用 GLSL 内置的 `smoothstep`（三次 Hermite 插值）：

```glsl
float intensity = smoothstep(0.0, 1.0, (theta - light.outerCutOff) / epsilon);
```

`smoothstep(0,1,t)` = `t² · (3 - 2t)`，在端点导数为 0，过渡更自然。

---

## 9. 法向量变换：NormalMatrix

### 9.1 为什么不能直接用 Model 矩阵变换法线

法线是**方向**向量（有方向，无位置），并且有一个重要约束：
**法线必须始终垂直于其所在的表面**。

考虑一个非均匀缩放变换：x 方向拉伸 2 倍，y 方向不变。
如果表面是 45° 斜面，其法线是 (1,1,0)/√2。
直接用 model 矩阵变换法线：

```
new_normal = mat3(model) * normal = (2, 1, 0)/√5
```

但表面的切线方向也随 x 变化：

```
new_tangent = mat3(model) * tangent ≈ (2, -1, 0)
dot(new_normal, new_tangent) = 2·2 + 1·(-1) = 3 ≠ 0
```

法线不再垂直于变换后的表面！

### 9.2 正确的变换矩阵

数学证明：法线的正确变换矩阵是 **Model 矩阵 3×3 子矩阵的逆转置**：

```
NormalMatrix = transpose(inverse(mat3(Model)))
```

**直觉推导**：
设表面切线为 **t**，法线为 **n**，有 `n·t = 0`。
变换后切线为 `M·t`，变换后法线为 `N·n`。
要求 `(N·n)·(M·t) = 0`，即 `(N·n)^T (M·t) = 0`，即 `n^T N^T M t = 0`。
因为 `n·t = 0`（即 `n^T t = 0`），需要 `N^T M = I`，即 `N = (M^{-1})^T`。

### 9.3 GLSL 实现

```glsl
// 顶点着色器
uniform mat3 uNormalMatrix;  // 在 CPU 端计算
out vec3 vNormal;

void main() {
    vNormal = normalize(uNormalMatrix * aNormal);
}
```

```cpp
// C++ 端
glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
shader.set_mat3("uNormalMatrix", normalMatrix);
```

### 9.4 特殊情况

- **纯旋转/平移**：`transpose(inverse(R)) = R`，可以直接用 `mat3(model)`
- **均匀缩放**（各轴缩放相同）：法线方向不变，只需归一化，可以直接用 `mat3(model)`
- **非均匀缩放**：**必须**使用 NormalMatrix

在引擎中，通常总是预计算并传入 NormalMatrix，避免条件分支带来的复杂性。

---

## 10. 常见坑与调试技巧

### 坑 1：法线未归一化

**症状**：插值后的法线长度不为 1，导致 `dot(normal, lightDir)` 超出 [-1,1] 范围
**修复**：在片段着色器中始终 `normalize(vNormal)`，即使顶点法线已经是单位向量
（插值过程会改变长度）

### 坑 2：NormalMatrix 忘记更新

**症状**：物体旋转后，光照方向"随着物体走"（法线在世界空间错误）
**修复**：每帧更新 NormalMatrix：
```cpp
glm::mat3 nm = glm::mat3(glm::transpose(glm::inverse(model)));
shader.set_mat3("uNormalMatrix", nm);
```

### 坑 3：光照在视图空间 vs 世界空间不一致

**症状**：光照感觉不对，转动摄像机时光源位置似乎也在移动
**原因**：法线/位置在世界空间，但光源位置在视图空间（或反之）
**修复**：确保所有光照计算在**同一坐标系**（推荐世界空间）

### 坑 4：高光消失（掠射角）

**症状**：从侧面看物体时，高光突然消失
**原因**：使用原始 Phong 模型，`dot(r, v)` 在掠射角为负数
**修复**：改用 Blinn-Phong（半程向量）

### 坑 5：环境光过强导致看不到光照

**症状**：整个场景平坦，缺乏立体感
**修复**：将环境光系数降低到 0.05-0.1，使阴影区域足够暗，以突出漫反射的立体感

### 坑 6：点光源个数超出数组范围

**症状**：着色器编译错误或渲染出错
**修复**：在 GLSL 中定义足够大的数组，并用 uniform int 传入实际数量，着色器中只循环到实际数量

### 调试技巧：将法线可视化

```glsl
// 片段着色器调试：将法线值直接输出为颜色
FragColor = vec4(vNormal * 0.5 + 0.5, 1.0); // 将 [-1,1] 映射到 [0,1]
// 蓝色 = Z 朝上，红色 = X 方向，绿色 = Y 方向
```

---

## 11. 本模块代码详解

### 11.1 文件结构

```
module08_lighting/
├── CMakeLists.txt
├── include/
│   ├── shader.h      ← 从 module07 复用
│   └── mesh.h        ← 从 module07 复用
├── src/
│   └── main.cpp      ← 完整光照场景
└── shaders/
    ├── phong.vert    ← 传递世界空间法线和位置
    ├── phong.frag    ← Blinn-Phong，支持三种光源
    ├── light_cube.vert ← 光源标记（无光照）
    └── light_cube.frag ← 纯色输出
```

### 11.2 场景组成

| 对象 | 数量 | 材质 |
|------|------|------|
| 地面平面 | 1 | 灰蓝色，低光泽 |
| 旋转立方体 | 5 | 蓝色，中等光泽 |
| 球体 | 3 | 红色，高光泽 |
| 点光源标记（小立方体） | 4 | 彩色，无光照 |

### 11.3 操作说明

| 按键/操作 | 功能 |
|-----------|------|
| `WASD` | 移动摄像机 |
| 鼠标移动 | 旋转视角 |
| 滚轮 | 调整 FOV |
| `1` | 仅方向光 |
| `2` | 仅点光源（4个） |
| `3` | 仅聚光灯（跟随摄像机） |
| `4` | 全部光源组合 |
| `Space` | 切换聚光灯开关 |

### 11.4 光源参数参考

```cpp
// 方向光（模拟黄昏太阳）
DirLight: direction = (-0.5, -1.0, -0.3), diffuse = (0.6, 0.6, 0.7)

// 4个点光源（绕场景分布，缓慢公转）
PointLight: Kc=1.0, Kl=0.09, Kq=0.032 → 有效范围约 50 单位

// 聚光灯（手电筒，跟随摄像机）
SpotLight: innerCone=12.5°, outerCone=17.5°
```

---

*End of Module 08 README*
