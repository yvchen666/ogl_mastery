#pragma once
#include "ecs.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// 用 tinygltf 加载 glTF 2.0 文件，并在 World 中创建对应 ECS 实体
// 每个 glTF Node 对应一个实体，包含：
//   - NameComponent（节点名）
//   - TransformComponent（TRS 变换）
//   - MeshComponent（如果节点有 mesh）
// ─────────────────────────────────────────────────────────────────────────────
void load_gltf(const std::string& path, World& world);
