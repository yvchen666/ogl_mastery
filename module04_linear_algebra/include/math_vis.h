#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// ── 渲染器初始化（在 OpenGL 上下文创建后调用一次）──────────────────────────
void math_vis_init();

// ── 资源释放 ───────────────────────────────────────────────────────────────
void math_vis_shutdown();

// ── 绘制 2D 箭头（从 start 到 end，NDC 坐标）──────────────────────────────
void draw_arrow_2d(glm::vec2 start, glm::vec2 end, glm::vec3 color);

// ── 绘制 3D 坐标轴（X=红, Y=绿, Z=蓝）────────────────────────────────────
void draw_axes_3d(float length = 1.0f);

// ── 绘制经矩阵变换后的网格点云 ────────────────────────────────────────────
//   M   : 4×4 变换矩阵（在 NDC 空间中应用）
//   color: 点/线颜色
void draw_transformed_grid(const glm::mat4& M, glm::vec3 color);

// ── 设置当前 MVP（由 main.cpp 提供，math_vis 使用） ────────────────────────
void math_vis_set_mvp(const glm::mat4& mvp);
