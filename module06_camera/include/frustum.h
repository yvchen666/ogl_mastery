#pragma once
#include <glm/glm.hpp>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// Frustum
//   视锥体由 6 个平面表示，平面方程 ax+by+cz+d=0，法向量朝内
//   用于可见性剔除（AABB / 球体测试）
// ─────────────────────────────────────────────────────────────────────────────
struct Frustum {
    // planes[0]=左  [1]=右  [2]=下  [3]=上  [4]=近  [5]=远
    std::array<glm::vec4, 6> planes;

    // ── 从 VP（View * Projection）矩阵提取视锥体平面 ─────────────────────
    // 使用 Gribb-Hartmann 方法（直接从矩阵列组合行向量）
    // 参考：Gribb & Hartmann, "Fast Extraction of Viewing Frustum Planes
    //        from the World-View-Projection Matrix"
    static Frustum from_vp(const glm::mat4& vp);

    // ── AABB 与视锥体相交测试 ──────────────────────────────────────────────
    // 若 AABB 完全在视锥体外，返回 false（剔除）
    bool intersects_aabb(glm::vec3 min, glm::vec3 max) const;

    // ── 球体与视锥体相交测试 ───────────────────────────────────────────────
    bool intersects_sphere(glm::vec3 center, float radius) const;
};
