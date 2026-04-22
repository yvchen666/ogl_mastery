# Module05 — Transforms（MVP 坐标变换）

## 1. 目的

本模块演示三维图形渲染中最核心的数学流水线：**Model → View → Projection → NDC → Screen**。

程序渲染 5 个带不同变换的立方体，WASD 控制相机移动，每 2 秒在终端打印当前 MVP 矩阵，帮助学习者建立对各坐标空间变换的具体感知。

主要学习目标：
- 理解齐次坐标及各变换矩阵的构造原理
- 掌握法向量变换（为什么是 $(M^{-1})^T$）
- 理解透视投影的非线性深度压缩成因
- 观察 TRS 变换顺序对结果的影响

---

## 2. 架构图

```
main.cpp
  ├── Shader("shaders/mvp.vert", "shaders/mvp.frag")
  ├── Mesh::create_cube()           ← 24顶点，6面，36索引
  │
  ├── 5个 CubeInstance              ← 各有 pos/scale/rot_axis/rot_speed/color
  │
  └── 主循环
        ├── process_input() → g_cam_pos 更新
        ├── view  = lookAt(pos, pos+front, up)
        ├── proj  = perspective(45°, aspect, 0.1, 100)
        │
        └── for each cube:
              ├── model = T * R * S
              ├── normalMat = transpose(inverse(mat3(model)))
              ├── shader.set_mat4("uModel", model)
              ├── shader.set_mat3("uNormalMatrix", normalMat)
              └── cube.draw()       ← glDrawElements

Shader (shader.h / shader.cpp)
  └── 从文件编译 GLSL，提供 set_mat4/set_vec3 等 setter

Mesh (mesh.h / mesh.cpp)
  ├── create_cube()   — 24顶点：每面4个，带法线和UV
  ├── create_quad()   — 4顶点全屏四边形
  └── create_sphere() — UV 球体

着色器
  shaders/mvp.vert  ← MVP变换 + 法向量矩阵
  shaders/mvp.frag  ← Lambert 漫反射
```

---

## 3. 关键 API

| 类/函数 | 说明 |
|---|---|
| `Shader(vert_path, frag_path)` | 从文件编译链接着色器程序 |
| `Shader::use()` | `glUseProgram(id)` |
| `Shader::set_mat4(name, m)` | 设置 4×4 矩阵 uniform |
| `Shader::set_mat3(name, m)` | 设置 3×3 法向量矩阵 |
| `Shader::set_vec3(name, v)` | 设置 vec3 uniform |
| `Mesh::create_cube()` | 生成单位立方体（24顶点，法线，UV） |
| `Mesh::create_sphere(stacks, slices)` | 生成 UV 球体 |
| `Mesh::draw()` | `glDrawElements(GL_TRIANGLES, ...)` |

---

## 4. 核心算法与完整数学推导

### 4.1 为什么需要齐次坐标

3D 仿射变换（平移+旋转+缩放）中，**平移不是线性变换**：

$$\mathbf{v}' = \mathbf{v} + \mathbf{t}$$

用 3×3 矩阵无法表达：若 $A\mathbf{0} = \mathbf{0}$（线性变换保原点），则平移后零向量变为 $\mathbf{t} \neq \mathbf{0}$，矛盾。

**引入 w 分量（齐次坐标）：**

将 3D 点 $(x,y,z)$ 扩展为 4D：

$$\tilde{\mathbf{p}} = (x, y, z, 1)^T \quad \text{（点，w=1）}$$
$$\tilde{\mathbf{v}} = (x, y, z, 0)^T \quad \text{（方向向量，w=0）}$$

**w=1 vs w=0 的几何意义：**
- **点（w=1）**：受平移影响，存在于欧氏空间中的具体位置
- **向量（w=0）**：不受平移影响，只表示方向（无起点）

验证：平移矩阵作用于向量：

$$\begin{pmatrix}1&0&0&t_x\\0&1&0&t_y\\0&0&1&t_z\\0&0&0&1\end{pmatrix} \begin{pmatrix}x\\y\\z\\0\end{pmatrix} = \begin{pmatrix}x\\y\\z\\0\end{pmatrix}$$

向量不被平移 ✓（物理意义：方向不受平移影响）

---

### 4.2 平移矩阵推导

从点 $(x,y,z,1)^T$ 平移 $(t_x, t_y, t_z)$ 得 $(x+t_x, y+t_y, z+t_z, 1)^T$。

直接写出满足条件的 4×4 矩阵：

$$T(t_x,t_y,t_z) = \begin{pmatrix}1&0&0&t_x\\0&1&0&t_y\\0&0&1&t_z\\0&0&0&1\end{pmatrix}$$

验证：

$$T \cdot (x,y,z,1)^T = (x+t_x,\; y+t_y,\; z+t_z,\; 1)^T \checkmark$$

**逆矩阵：** $T^{-1}(t_x,t_y,t_z) = T(-t_x,-t_y,-t_z)$（平移反向）。

---

### 4.3 缩放矩阵推导

沿各轴缩放因子 $s_x, s_y, s_z$：

$$S(s_x,s_y,s_z) = \begin{pmatrix}s_x&0&0&0\\0&s_y&0&0\\0&0&s_z&0\\0&0&0&1\end{pmatrix}$$

**均匀缩放：** $s_x = s_y = s_z = s$，保持形状比例。

**非均匀缩放：** $s_x \neq s_y$，例如压扁效果。注意：非均匀缩放使法向量变形，需用法向量矩阵纠正。

**逆矩阵：** $S^{-1} = S(1/s_x, 1/s_y, 1/s_z)$。

---

### 4.4 旋转矩阵推导（绕坐标轴）

**绕 X 轴旋转 θ：**

X 轴方向不变，YZ 平面内旋转。基向量变化：

$$\mathbf{e}_y \to (\cos\theta)\mathbf{e}_y + (\sin\theta)\mathbf{e}_z, \quad \mathbf{e}_z \to (-\sin\theta)\mathbf{e}_y + (\cos\theta)\mathbf{e}_z$$

写成矩阵（列为变换后的基向量）：

$$R_x(\theta) = \begin{pmatrix}1&0&0&0\\0&\cos\theta&-\sin\theta&0\\0&\sin\theta&\cos\theta&0\\0&0&0&1\end{pmatrix}$$

**绕 Y 轴旋转 θ：**

Y 轴不变，ZX 平面内旋转（注意右手系，Z 绕 Y 向 X 方向旋转为负方向）：

$$R_y(\theta) = \begin{pmatrix}\cos\theta&0&\sin\theta&0\\0&1&0&0\\-\sin\theta&0&\cos\theta&0\\0&0&0&1\end{pmatrix}$$

**绕 Z 轴旋转 θ：**

Z 轴不变，XY 平面内旋转：

$$R_z(\theta) = \begin{pmatrix}\cos\theta&-\sin\theta&0&0\\\sin\theta&\cos\theta&0&0\\0&0&1&0\\0&0&0&1\end{pmatrix}$$

---

### 4.5 Rodrigues 旋转公式（绕任意轴）完整推导

**目标：** 将向量 $\mathbf{v}$ 绕单位轴 $\hat{k}$ 旋转角度 $\theta$，求旋转后向量 $\mathbf{v}'$。

**步骤一：** 分解 $\mathbf{v}$ 为平行于 $\hat{k}$ 和垂直于 $\hat{k}$ 的分量：

$$\mathbf{v}_\parallel = (\mathbf{v}\cdot\hat{k})\hat{k}$$
$$\mathbf{v}_\perp = \mathbf{v} - \mathbf{v}_\parallel$$

**步骤二：** 在垂直平面内，$\mathbf{v}_\perp$ 旋转 $\theta$ 角。

构造与 $\mathbf{v}_\perp$ 垂直且在同一平面内的向量：

$$\mathbf{w} = \hat{k} \times \mathbf{v}_\perp = \hat{k} \times \mathbf{v}$$

（最后一步：$\hat{k}\times\mathbf{v}_\parallel = \mathbf{0}$，故 $\hat{k}\times\mathbf{v} = \hat{k}\times\mathbf{v}_\perp$）

注意 $|\mathbf{w}| = |\mathbf{v}_\perp|$（因 $\hat{k} \perp \mathbf{v}_\perp$，叉积长度 = 乘积）。

**步骤三：** 垂直分量旋转：

$$\mathbf{v}_\perp' = \mathbf{v}_\perp \cos\theta + \mathbf{w}\sin\theta$$

**步骤四：** 平行分量不变，合并：

$$\mathbf{v}' = \mathbf{v}_\parallel + \mathbf{v}_\perp' = \mathbf{v}_\parallel + \mathbf{v}_\perp\cos\theta + \mathbf{w}\sin\theta$$

代入 $\mathbf{v}_\perp = \mathbf{v} - \mathbf{v}_\parallel$ 和 $\mathbf{w} = \hat{k}\times\mathbf{v}$：

$$\mathbf{v}' = (\mathbf{v}\cdot\hat{k})\hat{k} + (\mathbf{v} - (\mathbf{v}\cdot\hat{k})\hat{k})\cos\theta + (\hat{k}\times\mathbf{v})\sin\theta$$

$$\boxed{\mathbf{v}' = \mathbf{v}\cos\theta + (\hat{k}\times\mathbf{v})\sin\theta + \hat{k}(\hat{k}\cdot\mathbf{v})(1-\cos\theta)}$$

**写成 4×4 矩阵形式（令 $c=\cos\theta$，$s=\sin\theta$，$\hat{k}=(k_x,k_y,k_z)$）：**

$$R(\hat{k},\theta) = \begin{pmatrix}
k_x^2(1-c)+c & k_xk_y(1-c)-k_zs & k_xk_z(1-c)+k_ys & 0 \\
k_xk_y(1-c)+k_zs & k_y^2(1-c)+c & k_yk_z(1-c)-k_xs & 0 \\
k_xk_z(1-c)-k_ys & k_yk_z(1-c)+k_xs & k_z^2(1-c)+c & 0 \\
0 & 0 & 0 & 1
\end{pmatrix}$$

此公式正是 `glm::rotate(mat4, angle, axis)` 的内部实现。

---

### 4.6 变换顺序：TRS 原则

在代码中，变换以矩阵乘法从右到左的顺序作用于顶点：

$$\mathbf{v}' = T \cdot R \cdot S \cdot \mathbf{v}$$

**为什么是 TRS 顺序（先缩放→再旋转→再平移）？**

- 若先平移再缩放：平移量也会被缩放，物体会移动到错误位置
- 若先旋转再缩放（且非均匀缩放）：缩放会沿旋转后的轴进行，结果不符合直觉

**示例：** 将一个球体放到坐标 (3,0,0)，半径 0.5：

```cpp
model = glm::translate(mat4{1}, vec3{3,0,0});  // T
model = glm::rotate(model, angle, vec3{0,1,0}); // R
model = glm::scale (model, vec3{0.5f});          // S
// 等效于矩阵乘法：T * R * S
```

等效于：先将单位球缩小为半径 0.5（S），再旋转（R），再移动到 (3,0,0)（T）。

---

### 4.7 法向量变换的完整推导

设顶点变换矩阵为 $M$（Model 矩阵的 3×3 部分）。

**为什么不能直接用 M 变换法向量？**

考虑非均匀缩放 $S = \text{diag}(2, 1, 1)$，一个沿 YZ 面的法向量 $\mathbf{n} = (1,0,0)^T$。

变换后的表面顶点：$\mathbf{v}' = S\mathbf{v}$，切向量 $\mathbf{t}' = S\mathbf{t}$。

若也用 S 变换法向量：$\mathbf{n}' = S\mathbf{n} = (2,0,0)^T = (1,0,0)^T$（方向不变）。

但变换后的切向量（例如 $\mathbf{t}=(0,1,0)^T \to \mathbf{t}'=(0,1,0)^T$）与 $(1,0,0)^T$ 确实垂直。

**非均匀缩放的反例：**

沿斜面的法向量 $\mathbf{n} = (1,1,0)^T/\sqrt{2}$，对应切向量 $\mathbf{t} = (-1,1,0)^T/\sqrt{2}$（垂直验证：$\mathbf{n}\cdot\mathbf{t}=0$ ✓）。

用 $S = \text{diag}(2,1,1)$ 变换后：
- $\mathbf{n}' = S\mathbf{n} = (2,1,0)^T/\sqrt{2}$（直接变换）
- $\mathbf{t}' = S\mathbf{t} = (-2,1,0)^T/\sqrt{2}$

验证：$\mathbf{n}'\cdot\mathbf{t}' = (-4+1)/2 = -3/2 \neq 0$ ✗ —— 不再垂直！

**推导正确变换矩阵：**

设法向量变换矩阵为 $G$，要求：$\mathbf{t}'^T \mathbf{n}' = 0$

$$(M\mathbf{t})^T (G\mathbf{n}) = \mathbf{t}^T M^T G \mathbf{n} = 0$$

由 $\mathbf{t}^T \mathbf{n} = 0$，对所有满足条件的 $\mathbf{t}$ 成立，故：

$$M^T G = \lambda I \Rightarrow G = (M^T)^{-1} = (M^{-1})^T$$

（$\lambda$ 为任意非零标量，法向量只需方向正确，归一化处理尺度）

---

### 4.8 坐标空间变换流程

**完整流水线：**

```
局部空间 (Local/Object Space)
    ↓  × Model 矩阵
世界空间 (World Space)
    ↓  × View 矩阵 (LookAt)
视图空间 (View/Camera/Eye Space)
    ↓  × Projection 矩阵
裁剪空间 (Clip Space)
    ↓  ÷ w（透视除法）
NDC (Normalized Device Coordinates)  [-1,1]³
    ↓  视口变换 (Viewport Transform)
屏幕空间 (Screen Space)  [0,width]×[0,height]
```

**各空间特点：**
- **局部空间**：模型设计时以原点为中心，无关绝对位置
- **世界空间**：所有物体共同的参照系，单位为场景单位（米）
- **视图空间**：以相机为原点，相机朝向 -Z，上方为 +Y（OpenGL 惯例）
- **裁剪空间**：透视矩阵已应用，但未做透视除法；GPU 在此空间做裁剪
- **NDC**：透视除法后，可见范围压缩到 [-1,1]³；OpenGL 中深度 [-1,1]，DirectX 中 [0,1]
- **屏幕空间**：最终像素坐标，由 `glViewport` 决定映射关系

---

### 4.9 View 矩阵（LookAt）完整推导

**输入：** 相机位置 $\mathbf{eye}$，目标点 $\mathbf{center}$，世界上向量 $\mathbf{up}$

**步骤一：** 构建相机正交基（右手坐标系，相机朝向 -Z）：

$$\mathbf{f} = \text{normalize}(\mathbf{center} - \mathbf{eye}) \quad \text{（front，朝向目标）}$$
$$\mathbf{r} = \text{normalize}(\mathbf{f} \times \mathbf{up}) \quad \text{（right，右方向）}$$
$$\mathbf{u} = \mathbf{r} \times \mathbf{f} \quad \text{（up，相机真实上方向，重正交化）}$$

注意：因为 $|\mathbf{f}|=|\mathbf{r}|=1$ 且 $\mathbf{f}\perp\mathbf{r}$，所以 $|\mathbf{u}|=1$ 自然满足。

**步骤二：** LookAt 矩阵 = 旋转矩阵 × 平移矩阵

旋转矩阵（把世界轴对齐到相机基）：

$$R = \begin{pmatrix} r_x & r_y & r_z & 0 \\ u_x & u_y & u_z & 0 \\ -f_x & -f_y & -f_z & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

（注意相机朝向 -Z，所以第三行是 $-\mathbf{f}$）

平移矩阵（移动世界使相机到原点）：

$$T = \begin{pmatrix}1&0&0&-e_x\\0&1&0&-e_y\\0&0&1&-e_z\\0&0&0&1\end{pmatrix}$$

合并：

$$\text{LookAt} = R \cdot T = \begin{pmatrix}
r_x & r_y & r_z & -\mathbf{r}\cdot\mathbf{eye} \\
u_x & u_y & u_z & -\mathbf{u}\cdot\mathbf{eye} \\
-f_x & -f_y & -f_z & \mathbf{f}\cdot\mathbf{eye} \\
0 & 0 & 0 & 1
\end{pmatrix}$$

这正是 `glm::lookAt` 的实现。

---

### 4.10 透视投影矩阵完整推导

**输入：** 垂直视角 $\text{fovy}$，宽高比 $\text{aspect}$，近平面 $n$，远平面 $f$

**步骤一：** 几何推导 XY 映射

在视图空间中，相机在原点朝 -Z 看，近平面 z = -n。

一个点 $(x, y, z)$（z < 0）投影到近平面上的坐标（相似三角形）：

$$x_p = \frac{n \cdot x}{-z}, \quad y_p = \frac{n \cdot y}{-z}$$

近平面范围：$y \in [-t, t]$，其中 $t = n\tan(\text{fovy}/2)$，$r = t\cdot\text{aspect}$。

归一化到 NDC $[-1,1]$：

$$x_{\text{ndc}} = \frac{x_p}{r} = \frac{n}{r}\cdot\frac{x}{-z}, \quad y_{\text{ndc}} = \frac{y_p}{t} = \frac{n}{t}\cdot\frac{y}{-z}$$

**步骤二：** 矩阵形式

为了用矩阵表达除以 $-z$，利用齐次坐标的透视除法（令 $w = -z$）：

$$\begin{pmatrix}n/r & 0 & 0 & 0 \\ 0 & n/t & 0 & 0 \\ 0 & 0 & A & B \\ 0 & 0 & -1 & 0\end{pmatrix}\begin{pmatrix}x\\y\\z\\1\end{pmatrix} = \begin{pmatrix}nx/r\\ny/t\\Az+B\\-z\end{pmatrix}$$

透视除法后：$x_{\text{ndc}} = \frac{nx/r}{-z}$，$y_{\text{ndc}} = \frac{ny/t}{-z}$ ✓

**步骤三：** 推导深度映射系数 A、B

要求：$z = -n$ 时深度 NDC = -1，$z = -f$ 时深度 NDC = 1。

$$z_{\text{ndc}} = \frac{Az+B}{-z}$$

联立：

$$\frac{A(-n)+B}{n} = -1 \Rightarrow -An+B = -n \quad \text{...(1)}$$
$$\frac{A(-f)+B}{f} = 1 \Rightarrow -Af+B = f \quad \text{...(2)}$$

(2)-(1)：$A(n-f) = f+n \Rightarrow A = -\frac{f+n}{f-n}$

代入(1)：$B = -n - An = -n + n\frac{f+n}{f-n} = \frac{-n(f-n)+n(f+n)}{f-n} = \frac{2nf}{f-n} \cdot (-1) $

更仔细地计算：

$$B = -n \cdot \frac{f-n}{f-n} + n\cdot\frac{f+n}{f-n} = n\cdot\frac{-f+n+f+n}{f-n} = \frac{2n^2}{f-n}$$

等等，再来一遍：从 (1)：$B = -n + An = -n + \left(-\frac{f+n}{f-n}\right)n = -n - \frac{n(f+n)}{f-n}$

$$= n\left(-1 - \frac{f+n}{f-n}\right) = n\cdot\frac{-(f-n)-(f+n)}{f-n} = n\cdot\frac{-2f}{f-n} = \frac{-2nf}{f-n}$$

所以：

$$\boxed{A = -\frac{f+n}{f-n}, \quad B = -\frac{2nf}{f-n}}$$

**完整透视投影矩阵：**

$$P = \begin{pmatrix}
\frac{n}{r} & 0 & 0 & 0 \\
0 & \frac{n}{t} & 0 & 0 \\
0 & 0 & -\frac{f+n}{f-n} & -\frac{2nf}{f-n} \\
0 & 0 & -1 & 0
\end{pmatrix}$$

其中 $t = n\tan(\text{fovy}/2)$，$r = t\cdot\text{aspect}$，即 $n/t = 1/\tan(\text{fovy}/2)$。

---

### 4.11 深度非线性与 Z-fighting

深度 NDC 值关于视图空间 z 是双曲线关系：

$$z_{\text{ndc}} = \frac{Az+B}{-z} = A - \frac{B}{z}$$

代入 A、B：

$$z_{\text{ndc}} = -\frac{f+n}{f-n} + \frac{2nf}{(f-n)z}$$

当 $z = -n$（近平面）时梯度最大，当 $z = -f$（远平面）时梯度趋近于 0。

**Z-fighting 成因：** 远处两个几乎重叠的面，其深度值被压缩到相同的 24-bit 浮点格表示，深度测试无法区分，产生闪烁。

**Reverse-Z 解决方案：** 将深度映射到 $[1, 0]$（近→1，远→0），利用 float32 在 0 附近精度更高的特性，大幅改善远处精度。实现：使用 `glDepthRange(1, 0)` 并修改投影矩阵中的符号。

---

## 5. 时序图

```
程序启动
  │
  ├─ glfwInit / glfwCreateWindow / gladLoadGLLoader
  ├─ glEnable(GL_DEPTH_TEST)
  ├─ Shader("shaders/mvp.vert", "shaders/mvp.frag")
  │     └─ 从文件读取 → compile_shader → glLinkProgram
  ├─ Mesh::create_cube()
  │     └─ 构造24顶点6面 → setup() → glGenVAO/VBO/EBO → glBufferData
  │
  └─ 主循环
        ├─ glfwPollEvents()
        ├─ process_input(dt) → g_cam_pos ± velocity
        │
        ├─ glClear(COLOR | DEPTH)
        │
        ├─ view = lookAt(cam_pos, cam_pos+front, up)
        ├─ proj = perspective(45°, aspect, 0.1, 100)
        │
        ├─ shader.use()
        ├─ set_vec3("uLightPos", ...)
        ├─ set_mat4("uView", view)
        ├─ set_mat4("uProjection", proj)
        │
        ├─ for cube in 5 cubes:
        │     ├─ model = T * R * S
        │     ├─ normalMat = transpose(inverse(mat3(model)))
        │     ├─ set_mat4("uModel", model)
        │     ├─ set_mat3("uNormalMatrix", normalMat)
        │     ├─ set_vec3("uObjectColor", color)
        │     └─ cube.draw() → glDrawElements(GL_TRIANGLES, 36, ...)
        │
        ├─ [每2秒] 打印 MVP 矩阵到终端
        │
        └─ glfwSwapBuffers()
```

---

## 6. 关键代码

### create_cube 单面构建

```cpp
auto face = [](vector<Vertex>& V, vector<uint32_t>& I,
               vec3 p0, vec3 p1, vec3 p2, vec3 p3, vec3 n) {
    uint32_t base = V.size();
    V.push_back({p0, n, {0,0}});
    V.push_back({p1, n, {1,0}});
    V.push_back({p2, n, {1,1}});
    V.push_back({p3, n, {0,1}});
    // 每面两个三角形（CCW 绕序）
    I.insert(I.end(), {base, base+1, base+2,
                       base, base+2, base+3});
};
```

### 法向量矩阵计算

```cpp
glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
shader.set_mat3("uNormalMatrix", normalMat);
```

着色器中：

```glsl
vNormal = normalize(uNormalMatrix * aNormal);
```

### TRS 变换顺序

```cpp
glm::mat4 model{1.0f};
model = glm::translate(model, ci.pos);              // T
model = glm::rotate(model, now * ci.rot_speed, ci.rot_axis); // R
model = glm::scale (model, ci.scale);               // S
```

GLM 矩阵乘法从右到左：`model = T * R * S`。

---

## 7. 设计决策

1. **24 顶点而非 8 顶点的立方体**：共享顶点的立方体（8顶点）无法为每面指定不同法线，会导致光照平滑错误。每面 4 个独立顶点（共 24）允许每面有独立的、正确朝向的法线。

2. **`mat3(model)` 而非 `model` 求法向量矩阵**：平移部分（第 4 列）不影响法向量（w=0 的方向向量不受平移影响），只需对旋转+缩放部分（3×3）求逆转置。

3. **帧率解耦（`dt` 参数）**：相机移动速度为 `move_speed * dt`，不依赖帧率，在高低帧率机器上表现一致。

4. **终端打印 MVP**：不依赖 ImGui，降低依赖，方便在无 GUI 环境（CI、SSH）下运行和验证。

5. **`create_sphere` 使用 UV 展开**：纬度/经度参数化（$\phi$/$\theta$）产生均匀 UV 坐标，适合后续纹理映射章节复用。

---

## 8. 常见坑

### 坑 1：TRS 顺序错误
**问题：** `model = S * R * T`（先平移再旋转），物体绕世界原点旋转而非自身中心。

**解决：** 正确顺序是 `model = T * R * S`（GLM 代码中从左到右写，但数学上右乘顶点）。

### 坑 2：法向量矩阵用 `mat4` 传入
**问题：** `uNormalMatrix` 如果定义为 `mat4` 但传入 `mat3` 数据，内存布局不匹配（GLSL `mat3` 每列 3 个 float，`mat4` 每列 4 个 float）。

**解决：** 着色器中定义为 `uniform mat3 uNormalMatrix`，C++ 中用 `glUniformMatrix3fv`。

### 坑 3：深度测试未启用
**问题：** 未调用 `glEnable(GL_DEPTH_TEST)`，远处物体会覆盖近处物体（渲染顺序覆盖）。

**解决：** 初始化时调用 `glEnable(GL_DEPTH_TEST)`，每帧 `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`。

### 坑 4：near 平面太小
**问题：** `near = 0.001` 时深度精度集中在极近范围，远处物体 Z-fighting 严重。

**解决：** 合理设置 near/far 比值，通常 far/near < 10000。本模块使用 near=0.1，far=100。

### 坑 5：GLM 列主序与 GLSL 不一致
**问题：** GLM 是列主序（column-major），与 OpenGL/GLSL 一致，但如果手写矩阵数组时用行主序，传入后变换错误。

**解决：** 始终使用 `glUniformMatrix4fv(..., GL_FALSE, ...)` 的 `GL_FALSE`（不转置）——GLM 与 GLSL 都是列主序，无需转置。

### 坑 6：EBO 未绑定到 VAO
**问题：** `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)` 在 `glBindVertexArray(0)` 解绑后调用，EBO 绑定不会存入 VAO 状态。

**解决：** 确保 EBO 的 `glBindBuffer` 在 VAO 绑定期间（`glBindVertexArray(vao)` 之后）调用。

---

## 9. 测试

### 视觉测试
1. 启动程序，应看到 5 个不同颜色的立方体在场景中旋转
2. 左侧立方体（蓝色，scale y=0.3 压扁）光照应显示正确——压扁面的顶面/底面应有高光，说明法向量矩阵正常工作
3. WASD 移动相机，立方体应保持在正确位置（视角变化但物体不动）

### 数值测试
终端输出示例（t=2s）：
```
=== t=2.0s  cam_pos=(0.00,1.50,6.00) ===
View:
  [ 1.0000   0.0000   0.0000  0.0000]
  [ 0.0000   1.0000   0.0000 -1.5000]
  [ 0.0000   0.0000   1.0000 -6.0000]
  [ 0.0000   0.0000   0.0000  1.0000]
Projection:
  [ 1.8107   0.0000   0.0000  0.0000]
  [ 0.0000   2.4142   0.0000  0.0000]
  [ 0.0000   0.0000  -1.0020 -0.2002]
  [ 0.0000   0.0000  -1.0000  0.0000]
```

验证 View 矩阵：相机在 (0,1.5,6)，无旋转，故 R=I，T=-(0,1.5,6)，`View[3][1]=-1.5`，`View[3][2]=-6` ✓

---

## 10. 构建

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target module05_transforms -j$(nproc)
./build/module05_transforms/module05_transforms
```

---

## 11. 延伸阅读

1. **learnopengl.com — Coordinate Systems**
   https://learnopengl.com/Getting-started/Coordinate-Systems
   图文并茂的坐标系讲解，是本模块内容的直接对应。

2. **Song Ho Ahn — OpenGL Projection Matrix**
   http://www.songho.ca/opengl/gl_projectionmatrix.html
   最详细的投影矩阵推导，含截图与推导步骤。

3. **Depth Precision Visualized — Nathan Reed**
   https://developer.nvidia.com/content/depth-precision-visualized
   深度精度问题的可视化分析，介绍 Reverse-Z 技术。

4. **Real-Time Rendering 4th Ed. — Akenine-Möller et al.**
   第 4 章（Transforms）是本模块所有内容的权威参考。

5. **GLM Documentation**
   https://glm.g-truc.net/
   `glm::translate`, `glm::rotate`, `glm::scale`, `glm::lookAt`, `glm::perspective` 的精确定义。

6. **Scratchapixel — The Perspective and Orthographic Projection Matrix**
   https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/
   从头推导，含代码验证。

7. **FGED Book 1 — Foundations of Game Engine Development**
   Eric Lengyel 著，第 1-3 章系统讲解变换数学。

8. **Gribb & Hartmann — Fast Extraction of Viewing Frustum Planes**
   （详见 Module06）视锥体提取与 MVP 矩阵的关系。
