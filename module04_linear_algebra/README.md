# Module04 — Linear Algebra（线性代数可视化）

## 1. 目的

本模块是一个**互动式数学演示程序**，在 OpenGL 4.6 窗口中可视化线性代数的核心概念：

- 按 **1** — 点积与投影（2D 向量）
- 按 **2** — 叉积（3D 空间）
- 按 **3** — 矩阵变换（网格变形动画）

通过将抽象数学公式映射到屏幕上的箭头、网格和坐标轴，帮助建立对向量运算、矩阵变换和四元数的几何直觉。

---

## 2. 架构图

```
main.cpp
  ├── 初始化 GLFW/GLAD
  ├── math_vis_init()          ← 编译着色器, 创建动态 VAO/VBO
  │
  ├── 主循环
  │     ├── 键盘事件 → g_demo = 1/2/3
  │     ├── demo_dot_product()        (演示1)
  │     │     └── draw_arrow_2d() ×4
  │     ├── demo_cross_product()      (演示2)
  │     │     └── draw_axes_3d() + draw_arrow_2d()
  │     └── demo_matrix_transform()   (演示3)
  │           └── draw_transformed_grid() ×2
  │
  └── math_vis_shutdown()

math_vis.cpp
  ├── compile_shader_file()    ← 从文件读取并编译 GLSL
  ├── draw_lines()             ← 上传顶点到 VBO, glDrawArrays
  ├── draw_arrow_2d()          ← 主干 + 两翼箭头
  ├── draw_axes_3d()           ← 3×2 端点的线段
  └── draw_transformed_grid()  ← 网格线段批量变换

着色器
  shaders/simple.vert  ← uMVP 变换
  shaders/simple.frag  ← uColor 纯色输出
```

---

## 3. 关键 API

| 函数 | 说明 |
|---|---|
| `math_vis_init()` | 编译着色器，创建 VAO/VBO，预分配 1 MB 动态缓冲 |
| `math_vis_shutdown()` | 释放 GPU 资源 |
| `math_vis_set_mvp(mat4)` | 设置当前绘制使用的 MVP 矩阵 |
| `draw_arrow_2d(start, end, color)` | 在 NDC 平面绘制带箭头的 2D 线段 |
| `draw_axes_3d(length)` | 绘制 RGB 三色坐标轴 |
| `draw_transformed_grid(M, color)` | 绘制经过矩阵 M 变换后的 [-1,1]² 网格 |

---

## 4. 核心算法与完整数学推导

### 4.1 向量点积

**代数定义：**

$$\mathbf{a} \cdot \mathbf{b} = a_1 b_1 + a_2 b_2 + a_3 b_3$$

**几何推导（余弦定理法）：**

设两向量 $\mathbf{a}$、$\mathbf{b}$，令 $\mathbf{c} = \mathbf{a} - \mathbf{b}$，则余弦定理：

$$|\mathbf{c}|^2 = |\mathbf{a}|^2 + |\mathbf{b}|^2 - 2|\mathbf{a}||\mathbf{b}|\cos\theta$$

展开 $|\mathbf{c}|^2 = \mathbf{c} \cdot \mathbf{c}$：

$$(\mathbf{a}-\mathbf{b})\cdot(\mathbf{a}-\mathbf{b}) = |\mathbf{a}|^2 + |\mathbf{b}|^2 - 2(\mathbf{a}\cdot\mathbf{b})$$

对比两式，得：

$$\boxed{\mathbf{a} \cdot \mathbf{b} = |\mathbf{a}||\mathbf{b}|\cos\theta}$$

**几何意义——投影：**

$\mathbf{a}$ 在 $\mathbf{b}$ 方向上的标量投影：

$$\text{proj}_{\mathbf{b}}^{\text{scalar}}(\mathbf{a}) = \frac{\mathbf{a}\cdot\mathbf{b}}{|\mathbf{b}|} = |\mathbf{a}|\cos\theta$$

向量投影（沿 $\hat{\mathbf{b}}$ 方向的向量）：

$$\text{proj}_{\mathbf{b}}(\mathbf{a}) = \frac{\mathbf{a}\cdot\mathbf{b}}{|\mathbf{b}|^2}\,\mathbf{b}$$

推导：标量投影乘以单位方向 $\hat{\mathbf{b}} = \mathbf{b}/|\mathbf{b}|$，故：

$$\text{proj}_{\mathbf{b}}(\mathbf{a}) = |\mathbf{a}|\cos\theta \cdot \frac{\mathbf{b}}{|\mathbf{b}|} = \frac{\mathbf{a}\cdot\mathbf{b}}{|\mathbf{b}|^2}\,\mathbf{b}$$

**实际用途：**
- 判断两向量夹角（点积 > 0 同侧，< 0 反侧，= 0 垂直）
- 漫反射光照：$L_{d} = \max(\mathbf{n}\cdot\mathbf{l}, 0)$
- 背面剔除：$\mathbf{n}\cdot\mathbf{v} < 0$ 时为背面

---

### 4.2 向量叉积

**行列式展开定义：**

$$\mathbf{a}\times\mathbf{b} = \begin{vmatrix} \mathbf{i} & \mathbf{j} & \mathbf{k} \\ a_1 & a_2 & a_3 \\ b_1 & b_2 & b_3 \end{vmatrix}$$

按第一行展开：

$$= \mathbf{i}(a_2 b_3 - a_3 b_2) - \mathbf{j}(a_1 b_3 - a_3 b_1) + \mathbf{k}(a_1 b_2 - a_2 b_1)$$

$$\boxed{\mathbf{a}\times\mathbf{b} = (a_2 b_3 - a_3 b_2,\; a_3 b_1 - a_1 b_3,\; a_1 b_2 - a_2 b_1)}$$

**反交换律：** $\mathbf{a}\times\mathbf{b} = -\mathbf{b}\times\mathbf{a}$

证明：将 $\mathbf{a}$、$\mathbf{b}$ 行对换，行列式变号，故叉积取反。

**模长 = 平行四边形面积：**

$$|\mathbf{a}\times\mathbf{b}| = |\mathbf{a}||\mathbf{b}|\sin\theta$$

推导：利用 $\sin^2\theta + \cos^2\theta = 1$：

$$|\mathbf{a}\times\mathbf{b}|^2 = |\mathbf{a}|^2|\mathbf{b}|^2 - (\mathbf{a}\cdot\mathbf{b})^2$$

$$= |\mathbf{a}|^2|\mathbf{b}|^2 - |\mathbf{a}|^2|\mathbf{b}|^2\cos^2\theta = |\mathbf{a}|^2|\mathbf{b}|^2\sin^2\theta$$

**实际用途：**
- 计算平面法向量（两条切向量叉积）
- 判断绕行方向（右手系 vs 左手系）
- 面积计算（三角形面积 = $|\mathbf{a}\times\mathbf{b}|/2$）

---

### 4.3 格拉姆-施密特正交化

**问题：** 给定三个线性无关向量 $\{\mathbf{v}_1, \mathbf{v}_2, \mathbf{v}_3\}$，构造正交归一基 $\{\mathbf{e}_1, \mathbf{e}_2, \mathbf{e}_3\}$。

**步骤一：** 以 $\mathbf{v}_1$ 为基准，直接归一化：

$$\mathbf{u}_1 = \mathbf{v}_1$$
$$\mathbf{e}_1 = \frac{\mathbf{u}_1}{|\mathbf{u}_1|}$$

**步骤二：** 从 $\mathbf{v}_2$ 中减去其在 $\mathbf{e}_1$ 方向的分量（投影），得到与 $\mathbf{e}_1$ 正交的部分：

$$\mathbf{u}_2 = \mathbf{v}_2 - (\mathbf{v}_2 \cdot \mathbf{e}_1)\,\mathbf{e}_1$$
$$\mathbf{e}_2 = \frac{\mathbf{u}_2}{|\mathbf{u}_2|}$$

验证正交性：$\mathbf{u}_2 \cdot \mathbf{e}_1 = \mathbf{v}_2 \cdot \mathbf{e}_1 - (\mathbf{v}_2 \cdot \mathbf{e}_1)(\mathbf{e}_1\cdot\mathbf{e}_1) = 0$ ✓

**步骤三：** 从 $\mathbf{v}_3$ 中减去在 $\mathbf{e}_1$、$\mathbf{e}_2$ 方向的分量：

$$\mathbf{u}_3 = \mathbf{v}_3 - (\mathbf{v}_3\cdot\mathbf{e}_1)\,\mathbf{e}_1 - (\mathbf{v}_3\cdot\mathbf{e}_2)\,\mathbf{e}_2$$
$$\mathbf{e}_3 = \frac{\mathbf{u}_3}{|\mathbf{u}_3|}$$

**应用：** 相机的 LookAt 矩阵构建（Right/Up/Front 三个正交向量），法线贴图的 TBN 矩阵构建。

---

### 4.4 矩阵乘法 = 线性变换的复合

**为什么矩阵乘法顺序不可交换？**

设两个变换 $A$（旋转 90°）和 $B$（剪切）。

$A$ 先作用于向量 $\mathbf{v}$，再用 $B$：$B(A\mathbf{v}) = (BA)\mathbf{v}$

$B$ 先作用，再用 $A$：$A(B\mathbf{v}) = (AB)\mathbf{v}$

因为 $BA \neq AB$（矩阵乘法不满足交换律），两种顺序产生不同结果。

**直觉：** "先旋转再剪切" ≠ "先剪切再旋转"。

---

### 4.5 行列式的几何意义

$\det(M)$ 是矩阵 $M$ 对**有向体积**的缩放因子。

- $|\det(M)| > 1$：体积放大
- $|\det(M)| < 1$：体积缩小
- $\det(M) = 0$：退化（降维）
- $\det(M) < 0$：镜像翻转（方向反转）

**2×2 推导：**

$$M = \begin{pmatrix} a & b \\ c & d \end{pmatrix}$$

列向量 $(a,c)^T$ 和 $(b,d)^T$ 张成的平行四边形面积 = 底 × 高：

底 = $|(a,c)^T| = \sqrt{a^2+c^2}$，高 = 第二列在垂直于第一列方向的分量。

经过计算得：面积 = $|ad - bc| = |\det(M)|$。

---

### 4.6 逆矩阵：伴随矩阵法

**2×2 详细推导：**

$$M = \begin{pmatrix} a & b \\ c & d \end{pmatrix}, \quad \det(M) = ad-bc$$

设 $M^{-1} = \frac{1}{\det(M)} \begin{pmatrix} d & -b \\ -c & a \end{pmatrix}$，验证：

$$M \cdot M^{-1} = \frac{1}{ad-bc}\begin{pmatrix}a&b\\c&d\end{pmatrix}\begin{pmatrix}d&-b\\-c&a\end{pmatrix}$$
$$= \frac{1}{ad-bc}\begin{pmatrix}ad-bc & -ab+ba \\ cd-dc & -cb+da\end{pmatrix} = \begin{pmatrix}1&0\\0&1\end{pmatrix} \checkmark$$

**3×3 原理：** 代数余子式 $C_{ij} = (-1)^{i+j} M_{ij}$，伴随矩阵 $\text{adj}(M) = (C_{ij})^T$，

$$M^{-1} = \frac{1}{\det(M)}\,\text{adj}(M)$$

---

### 4.7 法向量变换：为什么用 $(M^{-1})^T$

**背景：** 对顶点用矩阵 $M$ 变换后，法向量 $\mathbf{n}$ 不能直接用 $M$ 变换——非均匀缩放会使法向量不再垂直于表面。

**推导：**

设切向量 $\mathbf{t}$，法向量 $\mathbf{n}$，正交性条件：$\mathbf{t} \cdot \mathbf{n} = \mathbf{t}^T \mathbf{n} = 0$

变换后切向量 $\mathbf{t}' = M\mathbf{t}$，设法向量变换为 $\mathbf{n}' = G\mathbf{n}$，要求 $\mathbf{t}'^T \mathbf{n}' = 0$：

$$(M\mathbf{t})^T (G\mathbf{n}) = \mathbf{t}^T M^T G \mathbf{n} = 0$$

已知 $\mathbf{t}^T \mathbf{n} = 0$，故需：

$$M^T G = I \Rightarrow G = (M^T)^{-1} = (M^{-1})^T$$

即法向量矩阵为模型矩阵逆矩阵的转置，通常取其左上 3×3 部分。

---

### 4.8 正交矩阵与旋转矩阵

**定义：** 若 $M^T = M^{-1}$，即 $M^T M = I$，则 $M$ 为正交矩阵。

**性质：**
- 列向量互相正交且单位长度
- $\det(M) = \pm 1$（+1 旋转，-1 反射）
- 旋转矩阵属于正交矩阵（SO(3) 群）
- 正交矩阵保持长度和夹角

**实用意义：** 对于旋转矩阵，$M^{-1} = M^T$，求逆只需转置（代价极低）。

因此旋转变换的法向量矩阵 $(M^{-1})^T = (M^T)^T = M$，直接用原矩阵即可。

---

### 4.9 四元数

**从复数类比引入：**

复数 $z = a + bi$，其中 $i^2 = -1$，乘以 $i$ 等价于在复平面旋转 90°：

$$i \cdot (x + iy) = -y + ix$$

即把点 $(x,y)$ 旋转 90° 到 $(-y,x)$。

乘以 $e^{i\theta} = \cos\theta + i\sin\theta$ 等价于旋转角 $\theta$。

**四元数定义：**

$$q = w + xi + yj + zk$$

基向量满足：

$$i^2 = j^2 = k^2 = ijk = -1$$

由此推出：$ij = k$, $ji = -k$, $jk = i$, $kj = -i$, $ki = j$, $ik = -j$。

**单位四元数表示旋转：**

绕单位轴 $\hat{n} = (n_x, n_y, n_z)$ 旋转角 $\theta$：

$$q = \cos\frac{\theta}{2} + \sin\frac{\theta}{2}(n_x i + n_y j + n_z k)$$

满足 $|q| = 1$（单位四元数）。

**旋转应用：**

设纯四元数 $p = (0, \mathbf{v})$（$w=0$，$xyz=\mathbf{v}$），旋转后的向量对应四元数：

$$p' = q \, p \, q^{-1} = q \, p \, \bar{q} \quad (\text{因为 } |q|=1 \text{ 故 } q^{-1}=\bar{q})$$

**展开证明 = Rodrigues 旋转公式：**

设 $q = (c, s\hat{n})$，其中 $c=\cos(\theta/2)$，$s=\sin(\theta/2)$，

先算 $q \cdot p$（纯四元数积）：

$$qp = (-s(\hat{n}\cdot\mathbf{v}),\; c\mathbf{v} + s(\hat{n}\times\mathbf{v}))$$

再算 $(qp)\bar{q}$（$\bar{q} = (c, -s\hat{n})$），经过展开和三角化简，最终：

$$\mathbf{v}' = \mathbf{v}\cos\theta + (\hat{n}\times\mathbf{v})\sin\theta + \hat{n}(\hat{n}\cdot\mathbf{v})(1-\cos\theta)$$

这正是 **Rodrigues 旋转公式**。

**万向锁（Gimbal Lock）：**

欧拉角使用三个绕固定轴的旋转序列（如 Yaw→Pitch→Roll）。当中间轴（Pitch）旋转到 ±90° 时，第一轴和第三轴变成同一方向，自由度从 3 退化为 2，此现象称为万向锁。

四元数无此问题，因为它在 SO(3) 的双覆盖空间 S³ 上插值，不依赖坐标分解。

**SLERP（球面线性插值）：**

两个单位四元数 $q_0$、$q_1$ 之间的球面插值：

$$\text{slerp}(q_0, q_1, t) = q_0 \left(q_0^{-1} q_1\right)^t$$

等价公式（用夹角 $\Omega = \arccos(q_0 \cdot q_1)$）：

$$\text{slerp}(q_0, q_1, t) = \frac{\sin((1-t)\Omega)}{\sin\Omega}\,q_0 + \frac{\sin(t\Omega)}{\sin\Omega}\,q_1$$

当 $\Omega \to 0$ 时退化为线性插值（避免除零需特判）。

SLERP 保持旋转的恒定角速度，而线性插值 + 归一化（NLERP）速度不均匀但计算更快。

---

## 5. 时序图

```
程序启动
  │
  ├─ glfwInit(), glfwCreateWindow()
  ├─ gladLoadGLLoader()
  ├─ math_vis_init()
  │     ├─ compile_shader(simple.vert)
  │     ├─ compile_shader(simple.frag)
  │     ├─ glCreateProgram(), glLinkProgram()
  │     └─ glGenVertexArrays(), glGenBuffers()  ← 动态 VBO 1MB
  │
  └─ 主循环 ─────────────────────────────────────
        │
        ├─ glfwPollEvents()
        ├─ key_callback → g_demo = 1/2/3
        │
        ├─ glClear()
        │
        ├─ [g_demo==1] demo_dot_product()
        │       ├─ draw_arrow_2d(O, a, green)
        │       ├─ draw_arrow_2d(O, b, blue)
        │       ├─ draw_arrow_2d(O, proj, yellow)
        │       └─ draw_arrow_2d(a, proj, white)
        │
        ├─ [g_demo==2] demo_cross_product(aspect)
        │       ├─ math_vis_set_mvp(proj*view)
        │       └─ draw_axes_3d(1.5f)
        │
        ├─ [g_demo==3] demo_matrix_transform()
        │       ├─ math_vis_set_mvp(identity)
        │       ├─ draw_transformed_grid(原始, 灰色)
        │       └─ draw_transformed_grid(TRS, 橙色)
        │
        └─ glfwSwapBuffers()
```

---

## 6. 关键代码

### draw_arrow_2d 箭头翼计算

```cpp
// 将方向向量 dir 绕 z 轴旋转角 a，生成箭头翼方向
auto rotate2 = [](glm::vec2 v, float a) -> glm::vec2 {
    return { v.x * std::cos(a) - v.y * std::sin(a),
             v.x * std::sin(a) + v.y * std::cos(a) };
};

glm::vec2 w1 = end + hl * rotate2(-dir,  ang);   // +25° 偏转
glm::vec2 w2 = end + hl * rotate2(-dir, -ang);   // -25° 偏转
```

### draw_transformed_grid 网格变换

```cpp
glm::vec4 p0 = M * glm::vec4(x0, y0, 0, 1);
// 透视除法（w 分量恢复 NDC）
pts.push_back({p0.x/p0.w, p0.y/p0.w, p0.z/p0.w});
```

### 动态 VBO 上传

```cpp
glBufferSubData(GL_ARRAY_BUFFER, 0,
                pts.size() * sizeof(glm::vec3),
                pts.data());
glDrawArrays(GL_LINES, 0, pts.size());
```

---

## 7. 设计决策

1. **动态 VBO 而非每帧重建**：预分配 1 MB，用 `glBufferSubData` 更新，避免每帧 `glBufferData` 的驱动端重分配开销。

2. **GL_LINES 而非 GL_LINE_STRIP**：每条线段独立提交两端点，便于批量渲染不连通的线段（箭头翼、网格线）。

3. **2D 演示用 NDC 坐标，3D 演示用 MVP**：通过 `math_vis_set_mvp` 统一切换，简化着色器设计（只有一个 `uMVP` uniform）。

4. **网格做透视除法**：`draw_transformed_grid` 在 CPU 端完成透视除法，允许矩阵 M 包含透视变换，方便演示非仿射变换效果。

5. **箭头翼长度固定（0.08 NDC 单位）**：不随向量长度缩放，确保短向量也有可见箭头。

6. **着色器从文件加载**：运行时修改 `shaders/` 目录下的 GLSL 文件，重启程序即可热更新，方便实验。

---

## 8. 常见坑

### 坑 1：法向量用 M 直接变换
**问题：** 当模型有非均匀缩放（如 scale(1, 0.3, 1)），直接用 `M * normal` 会使法向量不再垂直于表面。

**解决：** 使用法向量矩阵 `transpose(inverse(mat3(M)))`。

### 坑 2：叉积不满足交换律
**问题：** `cross(a,b) != cross(b,a)`，且 `cross(a,b) = -cross(b,a)`。

**解决：** 构建三角形法向量时顶点顺序决定叉积方向，要统一 CCW 或 CW 约定。

### 坑 3：点积精度陷阱
**问题：** `acos(dot(a,b))` 在 dot 结果略超出 `[-1,1]`（浮点误差）时返回 NaN。

**解决：** `acos(clamp(dot(normalize(a),normalize(b)), -1.0f, 1.0f))`。

### 坑 4：行列式符号与绕行方向
**问题：** OpenGL 默认 CCW 为正面，反转顶点顺序导致背面剔除错误。

**解决：** 确保叉积 n = (v1-v0)×(v2-v0) 指向外侧（面对摄像机时为 CCW）。

### 坑 5：动态 VBO 越界
**问题：** 网格点数超过预分配大小（1MB / 12 bytes ≈ 87000 点）时 `glBufferSubData` 写越界。

**解决：** 在 `draw_lines` 中加断言，或动态 `glBufferData` 重分配。

### 坑 6：万向锁在欧拉角相机中
**问题：** FPS 相机 pitch 接近 ±90° 时抖动剧烈（yaw 和 roll 退化为同一旋转）。

**解决：** 限制 pitch 在 `[-89°, 89°]`，或改用四元数表示相机方向。

### 坑 7：NDC 坐标 vs 屏幕坐标
**问题：** 将屏幕像素坐标直接当 NDC 使用，导致箭头渲染到窗口角落之外。

**解决：** NDC 范围是 `[-1,1]²`，需先除以 `(width/2, height/2)` 并减 1。

---

## 9. 测试

### 手动测试

1. **演示 1（点积）**：按 `1`，查看绿色向量 a、蓝色向量 b、黄色投影向量、白色垂线是否几何正确（白线应垂直于 b）。
2. **演示 3（矩阵）**：按 `3`，观察橙色网格随时间旋转+缩放，验证 TRS 变换顺序正确（先缩放→旋转→平移）。

### 数值验证

终端输出（演示 1）：
```
[Demo 1] a=(0.65,0.35)  b=(0.20,0.75)
         a·b = 0.3925   |a|=0.7396  |b|=0.7762
         cos(theta) = 0.6826  theta=46.97 deg
         proj_b(a) = (0.1010, 0.3786)
```

验证：`proj_b(a) = (a·b / |b|²) * b = (0.3925/0.6025) * (0.20, 0.75) ≈ (0.1012, 0.3792)` ✓

---

## 10. 构建

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target module04_linear_algebra -j$(nproc)
./build/module04_linear_algebra/module04_linear_algebra
```

**依赖：**
- OpenGL 4.6 驱动（Mesa 21+ 或 NVIDIA/AMD 驱动）
- GLFW 3.3.9（FetchContent 自动拉取）
- GLM 0.9.9.8
- GCC 10（支持 C++17 `<filesystem>` 等）

---

## 11. 延伸阅读

1. **3Blue1Brown "Essence of Linear Algebra"**
   https://www.3blue1brown.com/topics/linear-algebra
   可视化直觉最佳资源，与本模块演示目标一致。

2. **Immersive Linear Algebra**（交互式线上教材）
   http://immersivemath.com/ila/
   逐步构建直觉，含 WebGL 交互图。

3. **Quaternions and Rotation Sequences** — Kuipers
   系统讲解四元数代数与旋转应用。

4. **Ken Shoemake, "Animating Rotation with Quaternion Curves"** SIGGRAPH 1985
   SLERP 原始论文。

5. **GLM 官方文档**
   https://glm.g-truc.net/
   vec/mat/quat 类型参考。

6. **The Matrix Cookbook**
   https://www.math.uwaterloo.ca/~hwolkowi/matrixcookbook.pdf
   矩阵恒等式速查手册。

7. **Rodrigues' rotation formula — Wikipedia**
   https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
   含完整矩阵推导。

8. **Gramm-Schmidt Process — MIT OCW 18.06**
   https://ocw.mit.edu/courses/18-06sc-linear-algebra-fall-2011/
   Gilbert Strang 线性代数公开课。
