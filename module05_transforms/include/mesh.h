#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Vertex 布局（location 0/1/2 与着色器对应）
// ─────────────────────────────────────────────────────────────────────────────
struct Vertex {
    glm::vec3 position;    // location 0
    glm::vec3 normal;      // location 1
    glm::vec2 tex_coords;  // location 2
};

// ─────────────────────────────────────────────────────────────────────────────
// Mesh
//   封装 VAO/VBO/EBO，提供工厂方法生成常用几何体
// ─────────────────────────────────────────────────────────────────────────────
class Mesh {
public:
    Mesh(std::vector<Vertex> verts, std::vector<uint32_t> indices);
    ~Mesh();

    // 不可复制
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    // 可移动
    Mesh(Mesh&& o) noexcept;
    Mesh& operator=(Mesh&& o) noexcept;

    // 绑定 VAO 并发起 glDrawElements
    void draw() const;

    // ── 内置几何体工厂 ────────────────────────────────────────────────────

    // 单位立方体（每面 4 顶点共 24，正确法线和 UV）
    static Mesh create_cube();

    // 全屏四边形（后处理用，z=0，NDC [-1,1]^2）
    static Mesh create_quad();

    // UV 球体（stacks 纬度段，slices 经度段）
    static Mesh create_sphere(int stacks = 16, int slices = 32);

private:
    GLuint vao_{0}, vbo_{0}, ebo_{0};
    size_t index_count_{0};

    void setup(const std::vector<Vertex>& verts,
               const std::vector<uint32_t>& indices);
    void release();
};
