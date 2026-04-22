#include "frustum.h"
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Gribb-Hartmann 方法
//
// 设 VP 矩阵（列主序，GLM 惯例）的行向量为 r0..r3，
// 则 6 个平面（法向量朝内）的系数为：
//   左   = r3 + r0
//   右   = r3 - r0
//   下   = r3 + r1
//   上   = r3 - r1
//   近   = r3 + r2
//   远   = r3 - r2
//
// GLM 是列主序：M[col][row]，所以第 i 行向量：
//   row_i = vec4(M[0][i], M[1][i], M[2][i], M[3][i])
// ─────────────────────────────────────────────────────────────────────────────
Frustum Frustum::from_vp(const glm::mat4& vp)
{
    Frustum f;

    auto row = [&](int i) {
        return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
    };

    glm::vec4 r0 = row(0);
    glm::vec4 r1 = row(1);
    glm::vec4 r2 = row(2);
    glm::vec4 r3 = row(3);

    f.planes[0] = r3 + r0;   // 左
    f.planes[1] = r3 - r0;   // 右
    f.planes[2] = r3 + r1;   // 下
    f.planes[3] = r3 - r1;   // 上
    f.planes[4] = r3 + r2;   // 近
    f.planes[5] = r3 - r2;   // 远

    // 归一化（使 .xyz 为单位法向量，.w 为距离）
    for (auto& p : f.planes) {
        float len = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
        if (len > 1e-7f) p /= len;
    }

    return f;
}

// ─────────────────────────────────────────────────────────────────────────────
// AABB 测试
//   对每个平面，取距平面最近的 AABB 顶点（p_vertex）做点面距离测试
//   若所有平面的 p_vertex 都在正侧（内侧），则 AABB 可见
// ─────────────────────────────────────────────────────────────────────────────
bool Frustum::intersects_aabb(glm::vec3 min, glm::vec3 max) const
{
    for (const auto& p : planes) {
        // 选出沿平面法向量方向最远的顶点（p-vertex）
        glm::vec3 pv;
        pv.x = (p.x >= 0) ? max.x : min.x;
        pv.y = (p.y >= 0) ? max.y : min.y;
        pv.z = (p.z >= 0) ? max.z : min.z;

        float dist = p.x*pv.x + p.y*pv.y + p.z*pv.z + p.w;
        if (dist < 0.0f) return false;   // AABB 完全在该平面的负侧
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 球体测试
//   球心到平面的有符号距离 < -radius → 完全在外
// ─────────────────────────────────────────────────────────────────────────────
bool Frustum::intersects_sphere(glm::vec3 center, float radius) const
{
    for (const auto& p : planes) {
        float dist = p.x*center.x + p.y*center.y + p.z*center.z + p.w;
        if (dist < -radius) return false;
    }
    return true;
}
