# Module 09 — 法线贴图与视差贴图

## 目录

1. [切线空间的必要性](#1-切线空间的必要性)
2. [TBN 矩阵推导](#2-tbn-矩阵推导)
3. [Gram-Schmidt 重正交化](#3-gram-schmidt-重正交化)
4. [在切线空间 vs 世界空间做光照](#4-在切线空间-vs-世界空间做光照)
5. [基础视差映射](#5-基础视差映射)
6. [陡峭视差映射（Steep Parallax）](#6-陡峭视差映射steep-parallax)
7. [视差遮蔽映射（POM）](#7-视差遮蔽映射pom)
8. [常见坑](#8-常见坑)
9. [本模块代码详解](#9-本模块代码详解)
10. [进阶扩展](#10-进阶扩展)
11. [参考资料](#11-参考资料)

---

## 1. 切线空间的必要性

### 1.1 法线贴图存储什么？

法线贴图存储的是**表面法线的扰动**——描述表面在微观尺度下的凹凸方向。
典型的法线贴图以蓝紫色为主，其中：
- R 通道（红）→ 切线方向（T）的法线分量
- G 通道（绿）→ 副切线方向（B）的法线分量
- B 通道（蓝）→ 法线方向（N）的法线分量，通常接近 1.0

解码：`normal = normalize(sample * 2.0 - 1.0)` 将 [0,1] 映射到 [-1,1]

### 1.2 为什么不直接存世界空间法线？

如果法线贴图存储的是**世界空间**方向，那么：

- 同一块砖墙纹理用于不同角度的墙面（竖直墙/斜面/天花板），每种角度都需要不同的法线贴图
- 网格变形（骨骼动画）后，所有法线都要重新烘焙
- 纹理无法复用，内存爆炸

如果存储的是**切线空间**方向，法线是相对于表面自身坐标系的，
只要纹理 UV 不变，同一张法线贴图可以用于任意朝向的表面、动画中的任意帧。

### 1.3 切线空间坐标系

切线空间（Tangent Space，也叫 TBN 空间）是一个以顶点为原点、
以表面为基准的局部坐标系：

```
T（Tangent，切线）   ── 沿 UV 的 U 轴方向（世界空间）
B（Bitangent，副切线）── 沿 UV 的 V 轴方向（世界空间）
N（Normal，法线）    ── 表面法线方向（T × B）
```

TBN 矩阵将切线空间中的向量变换到世界（或视图）空间：

```
v_world = TBN_matrix * v_tangent
```

其中 `TBN_matrix = [T | B | N]`（列向量排列的 3×3 矩阵）。

---

## 2. TBN 矩阵推导

### 2.1 从 UV 梯度计算切线

对于一个三角形，设两条边：

```
ΔP₁ = P₁ - P₀    （位置差，世界空间）
ΔP₂ = P₂ - P₀

ΔUV₁ = UV₁ - UV₀  （UV 差）
ΔUV₂ = UV₂ - UV₀
```

切线空间定义要求：沿 U 方向移动一个单位，在世界空间中移动 **T**；
沿 V 方向移动一个单位，在世界空间中移动 **B**。因此：

```
ΔP₁ = ΔU₁·T + ΔV₁·B
ΔP₂ = ΔU₂·T + ΔV₂·B
```

写成矩阵形式：

```
[ΔP₁]   [ΔU₁  ΔV₁] [T]
[ΔP₂] = [ΔU₂  ΔV₂] [B]
```

### 2.2 逆矩阵求解

设 M = [[ΔU₁, ΔV₁], [ΔU₂, ΔV₂]]，则：

```
[T]   M⁻¹ [ΔP₁]
[B] =      [ΔP₂]
```

2×2 矩阵的逆：

```
M⁻¹ = (1 / det(M)) * [ΔV₂  -ΔV₁]
                       [-ΔU₂  ΔU₁]

其中 det(M) = ΔU₁·ΔV₂ - ΔU₂·ΔV₁
```

展开：

```
T = (1/det) * ( ΔV₂·ΔP₁ - ΔV₁·ΔP₂)
B = (1/det) * (-ΔU₂·ΔP₁ + ΔU₁·ΔP₂)
```

### 2.3 代码实现（tangent_calculator.cpp）

```cpp
glm::vec3 dP1  = v1.position - v0.position;
glm::vec3 dP2  = v2.position - v0.position;
glm::vec2 dUV1 = v1.uv - v0.uv;
glm::vec2 dUV2 = v2.uv - v0.uv;

float det = dUV1.x * dUV2.y - dUV1.y * dUV2.x;
float inv = 1.0f / det;

glm::vec3 T = inv * ( dUV2.y * dP1 - dUV1.y * dP2);
glm::vec3 B = inv * (-dUV2.x * dP1 + dUV1.x * dP2);
```

### 2.4 每顶点平均

一个顶点被多个三角形共享，需要对所有共享三角形的切线取平均，然后归一化：

```cpp
for (每个三角形) {
    计算 T_tri, B_tri;
    tangents[i0] += T_tri;  tangents[i1] += T_tri;  tangents[i2] += T_tri;
    bitangents[i0] += B_tri; // 类似
}
for (每个顶点) {
    tangents[i] = normalize(tangents[i]);  // 归一化
    // 再做 Gram-Schmidt 正交化（见第3节）
}
```

---

## 3. Gram-Schmidt 重正交化

### 3.1 为什么需要重正交化？

由于：
1. 顶点法线在插值后可能不严格垂直于切线（特别是低面数模型）
2. 多个三角形平均后的切线不一定与法线严格正交
3. 浮点数精度损失

需要对 T、B、N 进行正交化，确保 TBN 矩阵是正交矩阵（否则求逆会出错）。

### 3.2 Gram-Schmidt 过程

给定法线 N（已知准确），重正交化 T 和 B：

```
// 步骤1：从 T 中减去沿 N 的分量（使 T ⊥ N）
T' = normalize(T - dot(T, N) · N)

// 步骤2：通过叉积重建 B（确保 B ⊥ T 且 B ⊥ N）
B' = cross(N, T') · handedness
```

其中 handedness（手性）用于修正坐标系方向：

```cpp
float handedness = (dot(cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;
```

如果叉积 `cross(N, T)` 与原始 B 方向相反，说明坐标系是左手系，需要翻转 B。

### 3.3 GLSL 中的重正交化

在顶点着色器中同样需要：

```glsl
vec3 T = normalize(uNormalMatrix * aTangent);
vec3 N = normalize(uNormalMatrix * aNormal);
// Gram-Schmidt
T = normalize(T - dot(T, N) * N);
vec3 B = cross(N, T);
```

注意：这里不需要显式传入 Bitangent，可以从 T 和 N 计算，节省顶点属性带宽。

---

## 4. 在切线空间 vs 世界空间做光照

### 4.1 两种方案

**方案 A：切线空间（本模块采用）**
- 在顶点着色器中将光源位置、视角位置变换到切线空间
- 片段着色器从法线贴图直接采样，在切线空间计算光照

```glsl
// 顶点着色器
mat3 TBN_inv = transpose(mat3(T, B, N)); // TBN 的逆 = TBN 的转置（正交矩阵）
vs_out.TangentLightPos = TBN_inv * uLightPos;
vs_out.TangentViewPos  = TBN_inv * uViewPos;
vs_out.TangentFragPos  = TBN_inv * fragPos;
```

**方案 B：世界空间**
- 在片段着色器中构建 TBN 矩阵
- 将法线贴图中的法线从切线空间变换到世界空间
- 在世界空间做光照

```glsl
// 片段着色器
vec3 normal = texture(uNormalMap, uv).rgb * 2.0 - 1.0;
normal = normalize(TBN * normal); // 切线→世界
// 在世界空间计算光照
```

### 4.2 对比

| | 切线空间（方案 A） | 世界空间（方案 B） |
|--|--|--|
| 计算位置 | 顶点着色器（变换向量） | 片段着色器（构建 TBN） |
| 每帧计算量 | 低（顶点数 << 片段数） | 高（每片段都要构建 TBN） |
| 精度 | 切线空间插值可能引入误差 | 法线贴图直接变换，精度更高 |
| 视差贴图 | 更方便（viewDir 已在切线空间） | 需要额外变换 |
| 推荐场景 | 性能敏感、移动端 | 复杂动态场景、PBR 渲染 |

---

## 5. 基础视差映射

### 5.1 原理

视差映射（Parallax Mapping）通过偏移纹理坐标来模拟表面的深度感，
让平面几何体看起来像真实的凹凸表面。

**基础视差**（也叫 Parallax Offset Mapping）：
使用高度图（height map）值来偏移 UV：

```glsl
vec2 parallax_basic(vec2 uv, vec3 viewDir) {
    float height = texture(uDepthMap, uv).r;
    // 沿视线方向在切线空间偏移 UV
    // viewDir.xy/viewDir.z 是视线方向的 xy 分量与 z 分量的比值
    vec2 offset = viewDir.xy / viewDir.z * height * uHeightScale;
    return uv - offset; // 负号：高处向视线方向"靠近"，UV 向相反方向偏移
}
```

### 5.2 局限性

基础视差只做了一次采样，在大 heightScale 或掠射角下会产生明显伪影（非线性扭曲）。

---

## 6. 陡峭视差映射（Steep Parallax）

### 6.1 算法思路

把深度范围 [0,1] 分成 N 层，沿视线方向从表面外层（depth=0）向内步进，
找到第一个光线深度超过深度图值的层。

```glsl
vec2 steep_parallax(vec2 uv, vec3 viewDir) {
    int numLayers = 16;  // 层数越多精度越高，但越慢
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    // 每层 UV 偏移量
    vec2 deltaUV = -viewDir.xy * uHeightScale / (viewDir.z * numLayers);

    vec2  currentUV    = uv;
    float currentDepth = texture(uDepthMap, currentUV).r;

    while (currentLayerDepth < currentDepth) {
        currentUV         += deltaUV;
        currentDepth       = texture(uDepthMap, currentUV).r;
        currentLayerDepth += layerDepth;
    }
    return currentUV;
}
```

### 6.2 自适应层数

掠射角下（viewDir.z 小）需要更多层才能保持精度：

```glsl
int numLayers = int(mix(float(MAX_LAYERS), float(MIN_LAYERS), abs(viewDir.z)));
// 视线正对表面（viewDir.z ≈ 1）→ 少量层即可
// 视线掠射（viewDir.z ≈ 0）    → 需要更多层
```

### 6.3 局限性

陡峭视差在层边界处会有锯齿/台阶状伪影。视差遮蔽映射（POM）通过插值解决这个问题。

---

## 7. 视差遮蔽映射（POM）

### 7.1 相邻层插值

POM 在陡峭视差找到交叉层之后，在前后两层之间做线性插值，精确定位光线与深度图的交点：

```glsl
vec2 parallax_mapping(vec2 uv, vec3 viewDir) {
    // [陡峭视差：步进找到交叉层] ...
    // (见本模块 shaders/normal_map.frag 完整实现)

    // 找到交叉后
    vec2 prevUV      = currentUV - deltaUV;  // 上一层的 UV
    float afterDepth  = currentDepth - currentLayerDepth;
    float beforeDepth = texture(uDepthMap, prevUV).r - (currentLayerDepth - layerDepth);

    // 线性插值权重
    // afterDepth < 0（光线在表面内部），beforeDepth > 0（光线在表面外部）
    // weight = afterDepth / (afterDepth - beforeDepth) ∈ [0,1]
    float weight = afterDepth / (afterDepth - beforeDepth);

    // 加权平均两层的 UV
    return mix(currentUV, prevUV, weight);
}
```

### 7.2 POM 效果对比

| 技术 | 性能 | 质量 | 适用场景 |
|------|------|------|----------|
| 基础视差 | 最快 | 低（大角度失真） | 顶视角，小 heightScale |
| 陡峭视差 | 中 | 中（层间锯齿） | 一般凹凸表面 |
| 视差遮蔽（POM） | 较慢 | 高（平滑） | 砖墙、石块等高质量场景 |
| 细节法线 + POM | 慢 | 最高 | 次世代角色皮肤、建筑 |

### 7.3 深度图约定

高度图（Height Map）中，**白色（1.0）= 高处，黑色（0.0）= 低处（凹陷）**。

在代码中通常用反转的"深度图"表示：`depth = 1.0 - height`，
这样深度图中白色=表面（无偏移），黑色=最深处（最大偏移）。

本模块使用高度图（直接用），视差偏移公式中用 `height * uHeightScale`，
`uHeightScale` 越大，视差效果越强烈（但也越容易出现自遮挡伪影）。

---

## 8. 常见坑

### 坑 1：TBN 不正交

**症状**：法线贴图光照有条纹、闪烁或错误高光
**原因**：Gram-Schmidt 未执行，T、B、N 不正交，`transpose(TBN)` ≠ `inverse(TBN)`
**修复**：在顶点着色器或 C++ 计算切线时执行 Gram-Schmidt 重正交化

### 坑 2：法线贴图 Y 轴约定（OpenGL vs DirectX）

**症状**：光照在垂直方向看起来反了（光从左边来，凹凸阴影出现在右边）
**原因**：OpenGL 使用 UV 原点在左下，绿色通道（G）= +Y = 向上；
DirectX 使用 UV 原点在左上，绿色通道（G）= -Y（向下）。
**修复**：从 DirectX 工具（Substance Painter，部分模式）导出的法线贴图需要翻转 G 通道：

```glsl
vec3 normal = texture(uNormalMap, uv).rgb * 2.0 - 1.0;
normal.y = -normal.y; // DirectX → OpenGL 转换
```

或在导出时选择 OpenGL 模式。

### 坑 3：切线空间坐标系不一致

**症状**：某些面的法线贴图效果正确，某些面效果反了
**原因**：模型 UV 展开时某些多边形 UV 是镜像的（手性不同），导致叉积方向反了
**修复**：正确计算并传入 handedness（手性值），在着色器中 `B = cross(N,T) * handedness`

### 坑 4：视差自阴影

**症状**：视差贴图在光源斜射时边缘出现暗条纹
**原因**：表面的高处遮挡了低处（自阴影），但普通 POM 没有处理这种情况
**修复**：实现视差自阴影（Parallax Self-Shadowing）：在光源方向再做一次步进查询

### 坑 5：视差 UV 越界导致边缘伪影

**症状**：平面边缘出现奇怪的拉伸纹理
**原因**：偏移后的 UV 超出 [0,1] 范围，GL_REPEAT 导致从另一侧采样
**修复**：在片段着色器中丢弃超出范围的片段：

```glsl
if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    discard;
```

### 坑 6：法线贴图用了 sRGB 格式

**症状**：法线贴图采样后值域偏移，光照出现奇怪的偏色
**原因**：法线贴图存储的是向量数据，不是颜色，不应用 sRGB 格式加载
**修复**：加载法线贴图时使用 `GL_RGB8`（线性），不使用 `GL_SRGB8`

---

## 9. 本模块代码详解

### 9.1 文件结构

```
module09_normal_maps/
├── CMakeLists.txt
├── include/
│   ├── shader.h             ← 从 module07 复用
│   ├── mesh.h               ← 从 module07 复用（普通 Vertex）
│   ├── mesh_tbn.h           ← 扩展 Mesh（VertexTBN，含 tangent/bitangent）
│   ├── tangent_calculator.h ← 接口声明
│   └── texture.h            ← Texture2D（与 module07 相同）
├── src/
│   ├── main.cpp             ← 演示场景
│   ├── tangent_calculator.cpp ← TBN 计算实现
│   └── texture.cpp          ← Texture2D 实现
└── shaders/
    ├── normal_map.vert      ← 构建 TBN，变换到切线空间
    └── normal_map.frag      ← 法线贴图采样，POM，Blinn-Phong
```

### 9.2 VertexTBN 结构

```cpp
struct VertexTBN {
    glm::vec3 position;    // location=0
    glm::vec3 normal;      // location=1
    glm::vec2 uv;          // location=2
    glm::vec3 tangent;     // location=3
    glm::vec3 bitangent;   // location=4
};
```

比普通 Vertex 多了 tangent 和 bitangent，每个顶点额外占用 6×4 = 24 字节。

### 9.3 切线计算流程

```
输入：顶点列表（position, normal, uv）+ 索引列表

1. 遍历所有三角形
   ├── 计算 ΔP₁, ΔP₂, ΔUV₁, ΔUV₂
   ├── 求 det = ΔU₁·ΔV₂ - ΔU₂·ΔV₁
   ├── 计算 T, B
   └── 累加到三个顶点上

2. 遍历所有顶点
   ├── Gram-Schmidt：T' = normalize(T - dot(T,N)·N)
   ├── 计算 handedness：sign(dot(cross(N,T), B))
   └── B' = cross(N, T') × handedness

输出：每顶点的 tangents[], bitangents[]
```

### 9.4 顶点着色器：切线空间变换

```glsl
// 构建 TBN 矩阵（列向量形式）
vec3 T = normalize(uNormalMatrix * aTangent);
vec3 N = normalize(uNormalMatrix * aNormal);
T = normalize(T - dot(T, N) * N);  // Gram-Schmidt
vec3 B = cross(N, T);

// TBN 的逆 = TBN 的转置（正交矩阵的性质）
mat3 TBN_inv = transpose(mat3(T, B, N));

// 将世界空间向量变换到切线空间
vs_out.TangentLightPos = TBN_inv * uLightPos;
vs_out.TangentViewPos  = TBN_inv * uViewPos;
vs_out.TangentFragPos  = TBN_inv * worldFragPos;
```

### 9.5 片段着色器：POM + Blinn-Phong

```glsl
// 1. 计算切线空间视线方向
vec3 viewDir = normalize(TangentViewPos - TangentFragPos);

// 2. 视差偏移 UV（POM）
if (uUseParallax)
    uv = parallax_mapping(uv, viewDir);

// 3. 从法线贴图采样
vec3 normal = texture(uNormalMap, uv).rgb * 2.0 - 1.0;  // [0,1]→[-1,1]
normal = normalize(normal);

// 4. Blinn-Phong（所有向量已在切线空间）
vec3 lightDir = normalize(TangentLightPos - TangentFragPos);
vec3 halfDir  = normalize(lightDir + viewDir);
float diff = max(dot(normal, lightDir), 0.0);
float spec = pow(max(dot(normal, halfDir), 0.0), 64.0);
```

### 9.6 按键说明

| 按键 | 效果 |
|------|------|
| `1` | 禁用法线贴图（平坦着色） |
| `2` | 启用法线贴图（无视差） |
| `3` | 启用法线贴图 + 视差遮蔽映射（POM） |
| `WASD` | 移动摄像机 |
| 鼠标 | 旋转视角 |
| 滚轮 | 调整 FOV |
| `ESC` | 退出 |

---

## 10. 进阶扩展

### 10.1 Mikktspace 标准切线空间

工业标准（Substance Painter、Marmoset 等工具使用）的切线空间计算算法，
由 Morten S. Mikkelsen 提出。与本模块使用的简单平均法相比，
Mikktspace 对共享顶点边界的处理更一致。

库：https://github.com/mmikk/MikkTSpace

### 10.2 世界空间法线贴图

对于需要全局光照（GI）或反射的场景，将法线在着色时变换到世界空间更合适：

```glsl
vec3 normal = texture(uNormalMap, uv).rgb * 2.0 - 1.0;
normal = normalize(mat3(T, B, N) * normal); // 切线→世界空间
// 之后在世界空间计算 GI、反射等
```

### 10.3 凹凸偏移贴图（Bump Map to Normal）

如果只有灰度凹凸贴图，可以用 Sobel 算子实时生成法线：

```glsl
float h_l = textureOffset(uBumpMap, uv, ivec2(-1, 0)).r;
float h_r = textureOffset(uBumpMap, uv, ivec2( 1, 0)).r;
float h_d = textureOffset(uBumpMap, uv, ivec2( 0,-1)).r;
float h_u = textureOffset(uBumpMap, uv, ivec2( 0, 1)).r;
vec3 normal = normalize(vec3(h_l - h_r, h_d - h_u, 0.1));
```

### 10.4 细节法线叠加

```glsl
// 混合两张法线贴图（一张大尺度，一张小细节）
vec3 n1 = texture(uNormalMap,       uv * 1.0).rgb * 2.0 - 1.0;
vec3 n2 = texture(uDetailNormalMap, uv * 8.0).rgb * 2.0 - 1.0;
// UDN 混合（Unreal 方法）
vec3 combined = normalize(vec3(n1.xy + n2.xy, n1.z));
```

### 10.5 视差遮蔽映射自阴影

在确定视差偏移后的 UV 后，沿**光源方向**再做陡峭视差搜索，
检测当前点是否在其他高出部分的阴影中：

```glsl
// 伪代码
vec2 final_uv = parallax_mapping(uv, viewDir);
float shadow = 0.0;
if (isInShadow(final_uv, lightDir_tangent)) {
    shadow = 1.0;
}
color *= (1.0 - shadow * 0.5);
```

---

## 11. 参考资料

1. Lengyel, Eric. "Mathematics for 3D Game Programming and Computer Graphics", 3rd ed.
   — TBN 矩阵推导（Chapter 7）

2. Blinn, James F. "Simulation of wrinkled surfaces." SIGGRAPH 1978.
   — 法线扰动的原始论文

3. Welsh, Terry. "Parallax Mapping with Offset Limiting." 2004.
   — 视差映射的基础参考

4. Tatarchuk, Natalya. "Practical parallax occlusion mapping." ShaderX5, 2006.
   — 视差遮蔽映射（POM）

5. Mikkelsen, Morten S. "Simulation of Wrinkled Surfaces Revisited." 2008.
   — Mikktspace 切线空间标准（http://mikktspace.com/）

6. de Vries, Joey. "Learn OpenGL - Normal Mapping."
   — https://learnopengl.com/Advanced-Lighting/Normal-Mapping

---

*End of Module 09 README*
