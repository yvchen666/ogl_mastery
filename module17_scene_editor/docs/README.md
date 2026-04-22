# Module 17 — Scene Editor

## 目录

1. [概述](#1-概述)
2. [ECS 模式：为什么比 OOP 继承更适合编辑器](#2-ecs-模式为什么比-oop-继承更适合编辑器)
3. [glTF 2.0 格式解析](#3-gltf-20-格式解析)
4. [对象拾取：FBO 方案 vs 射线-AABB 方案](#4-对象拾取fbo-方案-vs-射线-aabb-方案)
5. [ImGui 集成：GLFW + OpenGL3 后端](#5-imgui-集成glfw--opengl3-后端)
6. [GPU Query Object：测量各 Pass 耗时](#6-gpu-query-object测量各-pass-耗时)
7. [四元数在 ECS 中的使用](#7-四元数在-ecs-中的使用)
8. [构建与运行](#8-构建与运行)
9. [扩展方向](#9-扩展方向)
10. [参考资料](#10-参考资料)
11. [常见坑与调试技巧](#11-常见坑与调试技巧)

---

## 1. 概述

本模块实现一个小型 **3D 场景编辑器**：

- 加载 glTF 2.0 模型（tinygltf），构建 ECS 实体
- **ImGui 场景图**：层级列表，可选中/删除/移动物体
- **Framebuffer Object 对象拾取**：左键单击物体选中，渲染 EntityId 到 FBO
- 实时调节光源（位置/颜色/强度）
- GPU Query Object 显示各 Pass GPU 耗时

### 控制方式

| 操作 | 功能 |
|---|---|
| 右键拖拽 | 旋转摄像机 |
| W/A/S/D | 摄像机移动 |
| 左键点击 | 选中物体 |
| ImGui 面板 | 修改属性、删除实体 |
| ESC | 退出 |

### 启动方式

```bash
# 不带参数：加载内置 3 个立方体场景
./module17_scene_editor

# 带 glTF 文件：
./module17_scene_editor path/to/model.gltf
./module17_scene_editor path/to/model.glb
```

---

## 2. ECS 模式：为什么比 OOP 继承更适合编辑器

### 2.1 传统 OOP 继承的问题

```cpp
// 传统方案
class SceneObject { virtual void update(); glm::mat4 transform; };
class Mesh  : public SceneObject { VAO mesh; };
class Light : public SceneObject { glm::vec3 color; };
class LightedMesh : public Mesh, public Light {};  // 多重继承！
```

OOP 继承的痛点：
- **组合爆炸**：每种功能组合都需要一个子类（`LightedMesh`, `AnimatedLight`…）
- **虚函数表开销**：每次 `update()` 调用是间接跳转，破坏 CPU 分支预测
- **缓存不友好**：继承链中对象分散在堆，访问 `mesh->transform` 可能跨越多次 cache miss

### 2.2 ECS 核心思想

```
Entity（实体）= 一个整数 ID，没有数据，没有行为
Component（组件）= 纯数据结构（无方法）
System（系统）= 纯函数，遍历 Component
```

数据布局对比：

```
OOP:
  SceneObject[0]: [transform][color][mesh][...]  ← 混合在一起
  SceneObject[1]: [transform][color][mesh][...]
  ...（对象分散在堆）

ECS:
  TransformComponent[]: [t0][t1][t2]...  ← 连续内存，完美 cache line
  MeshComponent[]:      [m0][m1][m2]...
  LightComponent[]:     [l0][l1]...
```

### 2.3 本模块的极简 ECS 实现

```cpp
using EntityId = uint32_t;

class World {
    EntityId next_id_{1};
    // 类型 → (EntityId → std::any)
    std::unordered_map<std::type_index,
        std::unordered_map<EntityId, std::any>> components_;
};
```

性能说明：`std::any` 有动态分配开销，适合编辑器工具（每帧实体数量通常 < 10000）。高性能游戏引擎（如 EnTT、flecs）使用 Archetype 或 sparse set 实现真正的内存连续性。

### 2.4 遍历模式

```cpp
// 遍历所有有 TransformComponent 的实体
world.each<TransformComponent>([](EntityId eid, TransformComponent& tc) {
    // 更新变换矩阵
});

// 获取指定实体的特定 Component
auto* lc = world.get_component<LightComponent>(light_id);
if (lc) lc->intensity = 2.0f;
```

### 2.5 ECS 与编辑器的天然契合

- **序列化**：每个 Component 独立序列化/反序列化
- **撤销/重做**：Component 是纯数据，可以快照整个 Component 状态
- **运行时类型检查**：使用 `std::type_index` 可以在运行时查询实体拥有哪些 Component

---

## 3. glTF 2.0 格式解析

### 3.1 数据层次结构

```
glTF 文件
├── scenes[]          → 场景根节点列表
├── nodes[]           → 变换节点（TRS + 子节点）
│    └── mesh: int    → 引用 meshes[] 中的网格
├── meshes[]          → 网格
│    └── primitives[] → 基元（顶点 + 材质）
│         ├── attributes: { POSITION: 0, NORMAL: 1, TEXCOORD_0: 2 }
│         └── material: int
├── accessors[]       → 描述如何解释 bufferView 中的数据
│    ├── bufferView: int
│    ├── componentType: GL_FLOAT, GL_UNSIGNED_INT...
│    ├── type: "VEC3", "SCALAR"...
│    └── count: 顶点数
├── bufferViews[]     → Buffer 的切片（offset + length）
├── buffers[]         → 实际二进制数据（base64 或外部文件）
├── materials[]       → PBR 材质（baseColorTexture, metallicRoughness）
└── textures[]        → 引用 images[]
```

### 3.2 Accessor → OpenGL Buffer 的映射

```cpp
// glTF accessor 对应 OpenGL 顶点属性
const auto& acc = model.accessors[attr_idx];
const auto& bv  = model.bufferViews[acc.bufferView];
const void* ptr = buffer.data.data() + bv.byteOffset + acc.byteOffset;

glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, bv.byteLength, ptr, GL_STATIC_DRAW);

int num_comp = (acc.type == TINYGLTF_TYPE_VEC3) ? 3 : 2;
glVertexAttribPointer(location, num_comp, acc.componentType,
                      acc.normalized, bv.byteStride, (void*)acc.byteOffset);
```

### 3.3 glb vs gltf 格式

`.gltf`：JSON 文本 + 外部 `.bin` 二进制文件 + 外部纹理图片
`.glb`：单文件打包（JSON + 二进制块），适合分发

tinygltf 自动处理两种格式：
```cpp
bool ok = path.ends_with(".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
        : loader.LoadASCIIFromFile(&model, &err, &warn, path);
```

### 3.4 索引类型转换

glTF 索引可以是 `UNSIGNED_BYTE`、`UNSIGNED_SHORT`、`UNSIGNED_INT`。tinygltf 返回的 `componentType` 就是对应的 OpenGL 枚举值（`GL_UNSIGNED_BYTE=5121`, `GL_UNSIGNED_SHORT=5123`, `GL_UNSIGNED_INT=5125`），可直接传给 `glDrawElements`。

---

## 4. 对象拾取：FBO 方案 vs 射线-AABB 方案

### 4.1 射线-AABB 方案

```
鼠标坐标 → 射线方向（逆 View + Proj 变换）
射线 vs 每个物体的 AABB（轴对齐包围盒）→ 找最近交点
```

优点：不需要额外 GPU Pass，实现简单
缺点：
- 需要维护每个物体的 AABB
- 复杂形状（凸包不规则）拾取不准确
- 对于 10,000+ 物体需要 BVH 加速

### 4.2 FBO 颜色拾取（本模块）

```
① 渲染 ID Pass：
   - 绑定 FBO（R32UI 颜色缓冲）
   - 对每个物体渲染一遍，将 EntityId 编码到颜色
② 读取像素：
   - glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id)
③ 解码 EntityId
```

优点：
- 像素级精确（任何形状都能正确拾取）
- 与渲染几何完全对应（不受 AABB 不精确影响）
- 性能开销恒定（一次额外的渲染 Pass）

缺点：
- 需要额外的 FBO 和 GPU Pass
- `glReadPixels` 同步读回 CPU，有延迟（可用 PBO 异步）

### 4.3 R32UI 颜色纹理

拾取缓冲用 `GL_R32UI`（32 位无符号整数），直接存 EntityId：

```cpp
glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, w, h, 0,
             GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
```

Fragment Shader：
```glsl
out uint FragColor;  // 整数输出，不是 vec4
void main() {
    FragColor = uEntityId;  // 直接写 uint，无插值
}
```

清除时必须用整数清除函数（普通 `glClearColor` 无效）：
```cpp
GLuint clear_val = 0;
glClearBufferuiv(GL_COLOR, 0, &clear_val);
```

### 4.4 Y 轴翻转

OpenGL FBO 坐标系：左下角为 (0,0)，向上为正
GLFW 窗口坐标系：左上角为 (0,0)，向下为正

读取像素时需要翻转 Y：
```cpp
int flipped_y = height - 1 - mouse_y;
glReadPixels(mouse_x, flipped_y, 1, 1, ...);
```

---

## 5. ImGui 集成：GLFW + OpenGL3 后端

### 5.1 初始化顺序

```cpp
// 必须在 GLFW 窗口创建后、渲染循环前
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGui::StyleColorsDark();
ImGui_ImplGlfw_InitForOpenGL(window, true);   // 安装 GLFW 回调
ImGui_ImplOpenGL3_Init("#version 460");        // GLSL 版本
```

`true` 参数表示 ImGui 自动安装 GLFW 的键盘/鼠标回调（会覆盖之前设置的回调，或通过 chain callback 转发）。

### 5.2 每帧渲染顺序

```cpp
// 在渲染循环中
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();

// ... 定义 ImGui 窗口 ...

ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

ImGui 渲染必须在 **所有场景渲染之后**，因为它会修改 OpenGL 状态（混合、深度测试等）。

### 5.3 鼠标事件冲突处理

当 ImGui 窗口在鼠标下时，不应该触发场景拾取：
```cpp
if (!ImGui::GetIO().WantCaptureMouse) {
    // 处理场景鼠标拾取
}
if (!ImGui::GetIO().WantCaptureKeyboard) {
    // 处理摄像机键盘输入
}
```

### 5.4 DockSpace（可停靠窗口）

```cpp
// 在主窗口中开启 DockSpace
ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
ImGui::Begin("DockSpace", nullptr, flags);
ImGui::DockSpace(ImGui::GetID("MainDock"));
ImGui::End();
```

DockSpace 允许用户将 ImGui 窗口拖拽停靠到边缘，类似真实 IDE 的布局。

---

## 6. GPU Query Object：测量各 Pass GPU 耗时

### 6.1 Query Object 原理

CPU 无法直接测量 GPU 执行时间（GPU 与 CPU 异步）。Query Object 是 GPU 内部计时器：

```cpp
GLuint query;
glGenQueries(1, &query);

// 开始计时
glBeginQuery(GL_TIME_ELAPSED, query);

// ... GPU 命令（Draw calls, Compute, etc.) ...

// 结束计时
glEndQuery(GL_TIME_ELAPSED);

// 注意：结果不立即可用！GPU 尚未执行完
// 在下一帧读取（避免阻塞）
GLuint64 elapsed_ns;
glGetQueryObjectui64v(query, GL_QUERY_RESULT_NO_WAIT, &elapsed_ns);
double ms = elapsed_ns * 1e-6;
```

### 6.2 GL_QUERY_RESULT_NO_WAIT vs GL_QUERY_RESULT

`GL_QUERY_RESULT`：阻塞 CPU 直到 GPU 完成（导致 CPU/GPU 同步点，性能下降）
`GL_QUERY_RESULT_NO_WAIT`：如果结果不可用，返回 0（不阻塞，推荐）

最佳实践：在帧 N 提交 Query，在帧 N+1 或 N+2 读取结果（GPU 已经完成）。

### 6.3 可用的 Query 类型

```cpp
GL_TIME_ELAPSED        // 两次 BeginQuery/EndQuery 之间的 GPU 时间（纳秒）
GL_TIMESTAMP           // 当前 GPU 时间戳（glQueryCounter）
GL_SAMPLES_PASSED      // 通过深度测试的片段数
GL_PRIMITIVES_GENERATED // 生成的图元数（几何着色器输出）
GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN // Transform Feedback 写入数
```

---

## 7. 四元数在 ECS 中的使用

### 7.1 为什么不用欧拉角

欧拉角（pitch/yaw/roll）的问题：
- **万向锁（Gimbal Lock）**：当 pitch = ±90° 时，roll 和 yaw 的旋转轴重合，失去一个自由度
- **插值不平滑**：两个欧拉角的线性插值不对应最短旋转路径

### 7.2 四元数表示旋转

四元数 q = (w, x, y, z) 表示绕轴 (x,y,z) 旋转 2*arccos(w) 角度：

```cpp
// 绕 Y 轴旋转 45 度
glm::quat rot = glm::angleAxis(glm::radians(45.0f), glm::vec3(0,1,0));

// 转为旋转矩阵
glm::mat4 rot_mat = glm::mat4_cast(rot);
```

### 7.3 SLERP 插值

```cpp
glm::quat q1 = ..., q2 = ...;
float t = 0.5f;  // 0=q1, 1=q2
glm::quat interp = glm::slerp(q1, q2, t);
```

SLERP（球面线性插值）沿四元数单位超球面插值，保证：
- 角速度恒定（无"加速/减速"）
- 始终走最短路径
- 无万向锁

### 7.4 在 ECS 中的存储

```cpp
struct TransformComponent {
    glm::vec3 pos   = {0,0,0};
    glm::vec3 scale = {1,1,1};
    glm::quat rot   = {1,0,0,0};  // identity: w=1, xyz=0

    glm::mat4 matrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1), pos);
        m = m * glm::mat4_cast(rot);
        m = glm::scale(m, scale);
        return m;
    }
};
```

---

## 8. 构建与运行

### 8.1 依赖

- OpenGL 4.6
- GLFW 3.3+, GLAD, GLM
- Dear ImGui 1.90+（GLFW + OpenGL3 后端）
- tinygltf 2.8+

### 8.2 构建

```bash
cd /home/aoi/AWorkSpace/ogl_mastery
CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target module17_scene_editor -j$(nproc)
```

### 8.3 测试 glTF 资源

免费 glTF 测试模型：
- [Khronos Sample Models](https://github.com/KhronosGroup/glTF-Sample-Models)
- `DamagedHelmet.glb`（金属 PBR 头盔，经典测试）
- `Box.gltf`（简单立方体，调试用）

```bash
# 下载后：
./module17_scene_editor DamagedHelmet.glb
```

---

## 9. 扩展方向

### 9.1 Gizmo（变换操控器）

使用 [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) 库，在视口中直接拖拽物体：
```cpp
ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                     ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                     glm::value_ptr(model_matrix));
```

### 9.2 撤销/重做

将 Component 的每次修改记录到命令栈：
```cpp
struct Command {
    EntityId eid;
    TransformComponent before, after;
    void undo(World& w) { w.add_component(eid, before); }
    void redo(World& w) { w.add_component(eid, after);  }
};
std::vector<Command> history;
```

### 9.3 序列化/反序列化

将 World 的 Component 数据序列化为 JSON（使用 nlohmann/json）：
```cpp
json j;
world.each<TransformComponent>([&](EntityId eid, TransformComponent& tc) {
    j["entities"][eid]["transform"] = {
        {"pos",   {tc.pos.x, tc.pos.y, tc.pos.z}},
        {"scale", {tc.scale.x, tc.scale.y, tc.scale.z}}
    };
});
```

---

## 10. 参考资料

- [glTF 2.0 规范](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [tinygltf GitHub](https://github.com/syoyo/tinygltf)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [EnTT ECS 库](https://github.com/skypjack/entt)（高性能 ECS 参考实现）
- [Lösungen für ECS-Architekturen](https://ajmmertens.medium.com/building-an-ecs-1-where-are-my-entities-and-components-63d07c7da742)

---

## 11. 常见坑与调试技巧

### 坑 1：tinygltf 索引类型转换

glTF 索引可以是 8/16/32 位。`glDrawElements` 的第三个参数必须匹配：
```cpp
GLenum index_type = (GLenum)model.accessors[prim.indices].componentType;
// GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT
glDrawElements(GL_TRIANGLES, count, index_type, nullptr);
```

常见错误：固定写 `GL_UNSIGNED_INT`，但 glTF 用了 `GL_UNSIGNED_SHORT`，导致索引值被截断。

### 坑 2：Picking FBO 清除颜色为 0

`glClearColor(0,0,0,0)` 只设置浮点清除值，对整数格式的 FBO 无效：
```cpp
// 错误：不会清除 R32UI 颜色附件
glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
glClear(GL_COLOR_BUFFER_BIT);

// 正确
GLuint clear_val = 0;
glClearBufferuiv(GL_COLOR, 0, &clear_val);
```

### 坑 3：ImGui 鼠标事件与场景点击冲突

点击 ImGui 窗口内部时触发了场景拾取，导致误选/误触：
```cpp
if (!ImGui::GetIO().WantCaptureMouse) {
    // 只在 ImGui 不占用鼠标时处理场景点击
}
```

### 坑 4：glTF 节点变换顺序

glTF 规范：变换顺序 = Translation * Rotation * Scale（TRS）
```cpp
glm::mat4 m = glm::translate(glm::mat4(1), pos);
m = m * glm::mat4_cast(rot);   // 注意顺序：先旋转后缩放
m = glm::scale(m, scale);
```

不要用 `glm::scale(translate(...), scale)` 然后再乘旋转，顺序不同结果完全不同。

### 坑 5：Query Object 读取时机

```cpp
// 不要在同一帧内读取：
glBeginQuery(GL_TIME_ELAPSED, query);
// ... draw ...
glEndQuery(GL_TIME_ELAPSED);
GLuint64 t;
glGetQueryObjectui64v(query, GL_QUERY_RESULT, &t);  // 这会导致 CPU/GPU 同步！
```

正确做法：下一帧读取（`GL_QUERY_RESULT_NO_WAIT`），或使用双 Query 交替。

### 坑 6：Entity ID 0 的特殊含义

`INVALID_ENTITY = 0` 是保留值。确保 `next_id_` 从 1 开始，FBO 清除值为 0，这样点击空白处读到 0 就是"未命中任何物体"。

---

*Module 17 — Scene Editor | ogl_mastery 课程第五阶段*
