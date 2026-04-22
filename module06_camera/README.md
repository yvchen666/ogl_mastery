# Module06 — Camera（摄像机系统与视锥体剔除）

## 1. 目的

本模块实现完整的摄像机系统，包括两种常用摄像机模式，以及基于视锥体的可见性剔除：

- **FPS 摄像机**：WASD 平移，鼠标旋转（Yaw/Pitch），滚轮调节 FOV
- **轨道摄像机**：鼠标右键拖拽绕目标旋转，滚轮缩放距离
- **Tab 键**在两种摄像机之间切换
- **视锥体剔除**：使用 Gribb-Hartmann 方法从 VP 矩阵提取 6 个平面，对场景中 20 个物体做 AABB 测试，终端打印每帧剔除数量

---

## 2. 架构图

```
main.cpp
  ├── FpsCamera   g_fps          ← 欧拉角 + 方向向量
  ├── OrbitCamera g_orbit        ← 球坐标 (r, yaw, pitch)
  │
  ├── 回调
  │     ├── key_cb        → Tab切换相机 / ESC退出
  │     ├── cursor_pos_cb → FPS鼠标旋转 / Orbit拖拽旋转
  │     ├── scroll_cb     → FOV/距离调节
  │     └── mouse_button_cb → 右键状态记录
  │
  └── 主循环
        ├── 键盘 WASD → g_fps.process_keyboard(key, dt)
        ├── view / proj 取当前激活相机
        ├── Frustum::from_vp(proj * view)
        │     └── Gribb-Hartmann 平面提取
        ├── for each obj:
        │     ├── frustum.intersects_aabb(min, max)  ← 剔除测试
        │     └── draw if visible
        └── 打印 drawn/culled 统计

camera.h / camera.cpp
  ├── FpsCamera
  │     ├── process_keyboard(GLFW_KEY_*, dt)
  │     ├── process_mouse(dx, dy)  → yaw/pitch → update_vectors()
  │     ├── process_scroll(dy)     → fov 调节
  │     └── view_matrix()          → lookAt(position, position+front, up)
  └── OrbitCamera
        ├── process_mouse_drag(dx, dy) → yaw/pitch
        ├── process_scroll(dy)         → distance
        ├── position()                 → 球坐标转笛卡尔
        └── view_matrix()              → lookAt(position, target, up)

frustum.h / frustum.cpp
  ├── from_vp(vp)               → Gribb-Hartmann 提取 6 平面
  ├── intersects_aabb(min, max) → p-vertex 距离测试
  └── intersects_sphere(c, r)   → 有符号距离测试

着色器
  shaders/scene.vert   ← MVP + 法向量矩阵
  shaders/scene.frag   ← Blinn-Phong 光照
```

---

## 3. 关键 API

| 类/方法 | 说明 |
|---|---|
| `FpsCamera::process_keyboard(key, dt)` | 处理 GLFW_KEY_W/S/A/D/SPACE/CTRL，帧率解耦移动 |
| `FpsCamera::process_mouse(dx, dy)` | 更新 yaw/pitch，重建方向向量 |
| `FpsCamera::view_matrix()` | `lookAt(position, position+front, up)` |
| `FpsCamera::proj_matrix(aspect)` | `perspective(fov, aspect, 0.1, 200)` |
| `OrbitCamera::process_mouse_drag(dx, dy)` | 更新球坐标 yaw/pitch |
| `OrbitCamera::process_scroll(dy)` | 调节 distance，限制最小 0.5 |
| `OrbitCamera::position()` | 球坐标 → 笛卡尔，返回相机世界坐标 |
| `Frustum::from_vp(vp)` | Gribb-Hartmann 法提取 6 个平面（含归一化） |
| `Frustum::intersects_aabb(min, max)` | p-vertex 方法，$O(6 \times 3)$ 每物体 |
| `Frustum::intersects_sphere(c, r)` | 有符号距离测试，$O(6)$ 每物体 |

---

## 4. 核心算法与完整数学推导

### 4.1 欧拉角 Yaw/Pitch/Roll

**定义（航空惯例）：**
- **Yaw（偏航）**：绕 Y 轴旋转，影响水平朝向
- **Pitch（俯仰）**：绕 X 轴旋转，影响上下倾斜
- **Roll（滚转）**：绕 Z 轴旋转，影响侧倾（FPS 相机通常不使用）

**FPS 相机方向向量计算：**

给定 yaw（水平角，度）和 pitch（俯仰角，度）：

$$\mathbf{front} = \begin{pmatrix}\cos(\text{pitch})\cos(\text{yaw})\\\sin(\text{pitch})\\\cos(\text{pitch})\sin(\text{yaw})\end{pmatrix}$$

**推导：** 先在水平面（pitch=0）旋转 yaw 角，再将向量在 XZ 平面内压缩 $\cos(\text{pitch})$ 倍，同时 Y 分量为 $\sin(\text{pitch})$。

初始方向为 $-Z = (0, 0, -1)$，对应 yaw = -90°（$\cos(-90°) = 0$，$\sin(-90°) = -1$）→ 得到 $(0, 0, -1)$ ✓

重建 Right 和 Up：

$$\mathbf{right} = \text{normalize}(\mathbf{front} \times \mathbf{worldUp})$$
$$\mathbf{up} = \text{normalize}(\mathbf{right} \times \mathbf{front})$$

（重正交化，避免浮点误差积累导致基向量不正交）

---

### 4.2 万向锁（Gimbal Lock）

**现象：** 当 pitch = ±90° 时，$\cos(\text{pitch}) = 0$，front 向量变为 $(0, \pm 1, 0)$（正上或正下）。

此时 $\mathbf{front} \times \mathbf{worldUp} = (0,\pm1,0) \times (0,1,0) = \mathbf{0}$，right 向量无法计算，相机失去一个自由度（Yaw 无效）。

**表现：** 相机在俯仰到 ±90° 时，横向移动鼠标不再旋转 Yaw，而是产生 Roll 效果。

**FPS 相机解决方法：** 限制 pitch 范围为 $[-89°, 89°]$：

```cpp
pitch = std::clamp(pitch, -89.0f, 89.0f);
```

**根本解决方法（四元数）：** 使用四元数表示朝向，在四元数空间插值，不存在万向锁问题。代价是更新相机方向时需要从四元数中提取 front/right/up 向量。

---

### 4.3 轨道摄像机：球坐标系

**球坐标系定义：** $(r, \theta, \phi)$，其中：
- $r$：到目标点的距离
- $\theta$（yaw）：在水平面内的方位角（绕 Y 轴）
- $\phi$（pitch）：仰角，从水平面向上为正

**转换到笛卡尔坐标（以 target 为原点）：**

$$x = r\cos\phi\sin\theta$$
$$y = r\sin\phi$$
$$z = r\cos\phi\cos\theta$$

**推导：** 先在 XZ 平面内取水平距离 $r\cos\phi$，再按 yaw 角分解为 X/Z 分量；Y 分量直接为 $r\sin\phi$。

相机世界坐标：$\mathbf{eye} = \mathbf{target} + (x, y, z)$

View 矩阵：`lookAt(eye, target, worldUp)`。

---

### 4.4 LookAt 矩阵（再次推导，对应代码）

相机的 `view_matrix()` 调用 `glm::lookAt(pos, target, {0,1,0})`。

**GLM 内部步骤（与 module05 推导一致）：**

```
front = normalize(target - pos)
right = normalize(front × worldUp)
up    = right × front          // 注意：不是 front × right
```

注意：因为右手坐标系相机朝向 -Z，GLM lookAt 的 front 实际对应视图矩阵第三行的 -front（详见 module05 推导）。

构建矩阵：

$$\text{LookAt} = \begin{pmatrix}
r_x & r_y & r_z & -\mathbf{r}\cdot\mathbf{pos} \\
u_x & u_y & u_z & -\mathbf{u}\cdot\mathbf{pos} \\
-f_x & -f_y & -f_z & \mathbf{f}\cdot\mathbf{pos} \\
0 & 0 & 0 & 1
\end{pmatrix}$$

---

### 4.5 视锥体剔除：Gribb-Hartmann 平面提取

**参考：** Gribb & Hartmann, "Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix" (2001)

**推导：**

设 VP 矩阵（列主序），第 $i$ 行向量 $\mathbf{r}_i$（$i=0,1,2,3$）：

一个点 $\mathbf{p}$ 在 NDC 中可见的条件（OpenGL，NDC $\in [-1,1]^3$）：

$$-1 \le x_{\text{ndc}} \le 1, \quad -1 \le y_{\text{ndc}} \le 1, \quad -1 \le z_{\text{ndc}} \le 1$$

裁剪空间点 $\tilde{\mathbf{p}} = \text{VP} \cdot \mathbf{p}$，NDC $x = \tilde{p}_x / \tilde{p}_w$。

可见性条件 $-\tilde{p}_w \le \tilde{p}_x \le \tilde{p}_w$ 展开：

**左平面**（$\tilde{p}_x \ge -\tilde{p}_w$）：

$$\mathbf{r}_0 \cdot \mathbf{p} \ge -\mathbf{r}_3 \cdot \mathbf{p} \Rightarrow (\mathbf{r}_3 + \mathbf{r}_0) \cdot \mathbf{p} \ge 0$$

**右平面**（$\tilde{p}_x \le \tilde{p}_w$）：

$$\mathbf{r}_0 \cdot \mathbf{p} \le \mathbf{r}_3 \cdot \mathbf{p} \Rightarrow (\mathbf{r}_3 - \mathbf{r}_0) \cdot \mathbf{p} \ge 0$$

类似地，6 个平面：

| 平面 | 系数向量 |
|---|---|
| 左 | $\mathbf{r}_3 + \mathbf{r}_0$ |
| 右 | $\mathbf{r}_3 - \mathbf{r}_0$ |
| 下 | $\mathbf{r}_3 + \mathbf{r}_1$ |
| 上 | $\mathbf{r}_3 - \mathbf{r}_1$ |
| 近 | $\mathbf{r}_3 + \mathbf{r}_2$ |
| 远 | $\mathbf{r}_3 - \mathbf{r}_2$ |

提取 GLM 列主序矩阵的第 $i$ 行：

```cpp
auto row = [&](int i) {
    return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
};
```

提取后归一化（使法向量为单位长度，方便直接用于距离测试）：

```cpp
float len = sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
p /= len;
```

---

### 4.6 AABB 视锥体测试（p-vertex 方法）

**问题：** 判断轴对齐包围盒（AABB）是否在视锥体内。

**方法：** 对每个视锥体平面，测试 AABB 与平面的关系。

平面方程：$\mathbf{n}\cdot\mathbf{x} + d = 0$（法向量朝内）。

**p-vertex**：AABB 的 8 个顶点中，沿法向量 $\mathbf{n}$ 方向最远的顶点（即到平面正侧距离最大）：

$$p_{v,x} = \begin{cases}max_x & n_x \ge 0 \\ min_x & n_x < 0\end{cases}$$

（对 y、z 分量同理）

**判断：** 若 $p_v$ 的有符号距离 $\mathbf{n}\cdot p_v + d < 0$，则 AABB 完全在该平面负侧（外侧），可剔除。

若所有 6 个平面的 $p_v$ 均在正侧，则 AABB 与视锥体相交（可能可见）。

**复杂度：** $O(6 \times 3)$ 次比较，非常高效。

**注意：** 这是保守测试（有可能保留实际在视锥体外的 AABB），但绝不会错误剔除可见物体。

---

### 4.7 球体视锥体测试

球心 $\mathbf{c}$，半径 $r$。

对每个平面，计算球心到平面的有符号距离：

$$d = \mathbf{n}\cdot\mathbf{c} + d_{\text{plane}}$$

若 $d < -r$（球心在平面外侧且距离超过半径），球完全在外 → 剔除。

若所有平面 $d \ge -r$ → 球与视锥体相交（保留）。

---

## 5. 时序图

```
程序启动
  │
  ├─ glfwInit / glfwCreateWindow
  ├─ glfwSetInputMode(CURSOR_DISABLED)  ← FPS 模式锁定鼠标
  ├─ gladLoadGLLoader
  ├─ make_program("scene.vert", "scene.frag")
  ├─ make_cube() + make_sphere()
  │
  └─ 主循环
        ├─ glfwPollEvents()
        │     ├─ key_cb: Tab → 切换相机 + CURSOR 模式
        │     ├─ cursor_pos_cb:
        │     │     ├─ g_first_mouse=true → 跳过第一帧（防止跳跃）
        │     │     ├─ [FPS]   g_fps.process_mouse(dx, dy)
        │     │     └─ [Orbit+RMB] g_orbit.process_mouse_drag(dx, dy)
        │     └─ scroll_cb: fps.fov 或 orbit.distance
        │
        ├─ [FPS] for key in {W,S,A,D,SPACE,CTRL}:
        │          if pressed → g_fps.process_keyboard(key, dt)
        │
        ├─ view, proj = active_camera.view/proj_matrix(aspect)
        ├─ Frustum f = Frustum::from_vp(proj * view)
        │
        ├─ glClear()
        ├─ glUseProgram, set uniforms
        │
        ├─ for obj in scene[20]:
        │     ├─ f.intersects_aabb(obj.pos-half, obj.pos+half)
        │     │     → false: ++culled, continue
        │     ├─ model = T * S
        │     ├─ set_mat4("uModel", ...)
        │     └─ sphere/cube.draw()
        │
        ├─ [每2秒] printf(drawn, culled)
        └─ glfwSwapBuffers()
```

---

## 6. 关键代码

### FpsCamera::update_vectors

```cpp
void FpsCamera::update_vectors()
{
    float yawR   = glm::radians(yaw);
    float pitchR = glm::radians(pitch);

    glm::vec3 f;
    f.x = std::cos(pitchR) * std::cos(yawR);
    f.y = std::sin(pitchR);
    f.z = std::cos(pitchR) * std::sin(yawR);
    front_ = glm::normalize(f);

    right_ = glm::normalize(glm::cross(front_, glm::vec3{0,1,0}));
    up_    = glm::normalize(glm::cross(right_, front_));
}
```

### OrbitCamera::position

```cpp
glm::vec3 OrbitCamera::position() const
{
    float pitchR = glm::radians(pitch);
    float yawR   = glm::radians(yaw);
    glm::vec3 offset;
    offset.x = distance * std::cos(pitchR) * std::sin(yawR);
    offset.y = distance * std::sin(pitchR);
    offset.z = distance * std::cos(pitchR) * std::cos(yawR);
    return target + offset;
}
```

### Frustum::from_vp（Gribb-Hartmann）

```cpp
auto row = [&](int i) {
    return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
};
f.planes[0] = row(3) + row(0);   // 左
f.planes[1] = row(3) - row(0);   // 右
// ...归一化...
```

### AABB 测试（p-vertex）

```cpp
for (const auto& p : planes) {
    glm::vec3 pv;
    pv.x = (p.x >= 0) ? max.x : min.x;
    pv.y = (p.y >= 0) ? max.y : min.y;
    pv.z = (p.z >= 0) ? max.z : min.z;
    if (p.x*pv.x + p.y*pv.y + p.z*pv.z + p.w < 0.0f) return false;
}
```

---

## 7. 设计决策

1. **FpsCamera 限制 pitch = [-89°, 89°]**：避免万向锁。0.001° 的裕量确保 cross(front, worldUp) 永远不为零向量。

2. **鼠标第一帧跳跃防护（g_first_mouse）**：程序启动时 `glfwSetCursorPos` 前后鼠标位置可能跳跃很大。通过在第一帧仅记录位置而不计算位移来避免。Tab 切换相机时也重置此标志。

3. **OrbitCamera 右键拖拽而非左键**：左键通常用于选择，右键拖拽是 3D 软件（Maya、Blender）的惯例，避免与 UI 交互冲突。

4. **Frustum 平面归一化**：归一化后平面方程 $\mathbf{n}\cdot\mathbf{x}+d=0$ 中 $\mathbf{n}$ 为单位向量，$d$ 直接表示有符号距离，便于球体测试。AABB 测试也可不归一化（仅需正负号），但归一化有利于调试。

5. **场景 20 个物体用固定 seed 伪随机**：避免引入 `<random>`，确保每次运行场景布局一致，便于测试验证。

6. **Blinn-Phong 而非 Phong**：Blinn-Phong 用半向量替代反射向量，视角掠射时高光形状更自然，且计算量更小（无需 reflect 调用）。

---

## 8. 常见坑

### 坑 1：万向锁（Gimbal Lock）
**问题：** Pitch = ±90° 时，front = (0,±1,0)，与 worldUp = (0,1,0) 平行，cross product 为零。

**解决：** 限制 pitch 范围到 [-89°, 89°]。根本解决方案为使用四元数表示朝向。

### 坑 2：鼠标第一帧跳跃
**问题：** 程序启动/Tab 切换时，GLFW 报告鼠标位于上次离开的位置，与窗口中心差距很大，导致相机突然转向。

**解决：** 用 `g_first_mouse` 标志，第一帧只记录位置不计算位移。

### 坑 3：Near plane 过小导致 Z-fighting
**问题：** near = 0.001 时，深度精度集中在极近范围，远处物体出现闪烁斑纹。

**解决：** 合理设置 near/far 比值（far/near < 10000），本模块 near=0.1, far=200。

### 坑 4：GLM 矩阵行列混淆（Gribb-Hartmann）
**问题：** GLM 矩阵是列主序，`vp[col][row]`，提取行向量时写成 `vp[i][0..3]` 实际是列不是行。

**解决：** 正确的行向量提取：`glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i])`。

### 坑 5：视锥体剔除误判（AABB 全在外）
**问题：** Frustum 测试是保守的，但若使用错误的 NDC 范围（如将 DirectX 的 [0,1] 深度规则用于 OpenGL），远平面方程符号错误，远处物体被错误剔除。

**解决：** GLM 默认生成 OpenGL NDC（z∈[-1,1]），确保与视锥体提取用同一规范。

### 坑 6：Tab 切换时忘记释放/锁定鼠标
**问题：** 从 Orbit 切换到 FPS 时，若未调用 `glfwSetInputMode(CURSOR_DISABLED)`，鼠标光标仍可见，用户拖动鼠标时 FPS 相机不响应。

**解决：** Tab 键回调中根据当前模式切换 GLFW 鼠标模式，并重置 `g_first_mouse = true`。

### 坑 7：Orbit 相机 pitch 靠近 ±90° 时 up 向量翻转
**问题：** pitch = 90° 时，相机在目标正上方，`lookAt(pos, target, {0,1,0})` 中 front 向量平行于 worldUp，产生数值问题。

**解决：** 限制 pitch 在 [-89°, 89°]。若需极角相机，改用四元数或动态切换 up 向量。

---

## 9. 测试

### 视觉测试

1. **FPS 相机**：启动后（默认 FPS），移动鼠标，相机应平滑旋转；WASD 移动，速度与帧率无关（dt 解耦）；Scroll 调节 FOV（场景放大/缩小感）。

2. **Orbit 相机**：按 Tab 切换，右键拖拽，场景应绕世界原点旋转；Scroll 调节到目标距离。

3. **视锥体剔除**：移动到场景边缘，终端应打印 `drawn=N culled=M`，M > 0 表示有物体被剔除。转动相机朝向一侧，被剔除数量增加。

### 数值测试

终端输出示例：

```
[t=4.0s] cam=(0.00,0.00,3.00)  drawn=12  culled=8
```

相机在原点附近，约 40% 物体在视锥体外被剔除，剔除逻辑正常工作。

---

## 10. 构建

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target module06_camera -j$(nproc)
./build/module06_camera/module06_camera
```

**操作说明：**

| 操作 | FPS 相机 | Orbit 相机 |
|---|---|---|
| Tab | 切换到 Orbit | 切换到 FPS |
| WASD | 前后左右移动 | 无效 |
| Space/Ctrl | 上下移动 | 无效 |
| 鼠标移动 | 旋转视角 | 无效 |
| 右键 + 拖拽 | 无效 | 旋转轨道 |
| 滚轮 | 调节 FOV | 缩放距离 |
| ESC | 退出 | 退出 |

---

## 11. 延伸阅读

1. **learnopengl.com — Camera**
   https://learnopengl.com/Getting-started/Camera
   FPS 相机实现的经典教程，与本模块直接对应。

2. **Gribb & Hartmann 原文**
   "Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix"
   http://www.cs.otago.ac.nz/postgrads/alexis/planeExtraction.pdf
   视锥体提取的原始论文，2 页，简洁实用。

3. **Real-Time Rendering 4th Ed. — Chapter 19: Acceleration Algorithms**
   视锥体剔除、层次包围盒、遮挡剔除的系统讲解。

4. **Scratchapixel — Ray Tracing: Generating Camera Rays**
   https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-generating-camera-rays/
   从光线投射角度理解相机的 View/Projection。

5. **Depth Precision Visualized — Nathan Reed (NVIDIA)**
   https://developer.nvidia.com/content/depth-precision-visualized
   深度精度分析，Reverse-Z 实践。

6. **GDC 2015: Optimizing the Graphics Pipeline with Compute (AMD)**
   GPU 端视锥体剔除（Compute Shader）的工业实践。

7. **Cesium Blog: Depth Precision**
   https://cesium.com/blog/2013/05/09/reentrant-rendering-in-cesium/
   地理可视化场景中的深度精度问题与 Logarithmic Depth 解决方案。

8. **Quaternion Camera — Ogre3D Wiki**
   用四元数实现无万向锁相机的参考实现。
