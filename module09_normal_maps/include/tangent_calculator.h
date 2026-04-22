#pragma once
#include "mesh.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct TangentData {
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
};

// 为三角形网格计算切线和副切线
// 输入：顶点数组（含 position/uv）以及索引数组（三角形列表）
// 输出：每顶点的 Tangent / Bitangent（已做平均 + Gram-Schmidt 重正交化）
TangentData calc_tangents(
    const std::vector<Vertex>& verts,
    const std::vector<uint32_t>& indices);
