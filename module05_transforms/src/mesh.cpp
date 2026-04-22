#include "mesh.h"

#include <stdexcept>
#include <cmath>
#include <utility>

// ─────────────────────────────────────────────────────────────────────────────
// 内部工具：上传顶点/索引到 GPU
// ─────────────────────────────────────────────────────────────────────────────
void Mesh::setup(const std::vector<Vertex>& verts,
                 const std::vector<uint32_t>& indices)
{
    index_count_ = indices.size();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    // location 0 : position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // location 1 : normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // location 2 : tex_coords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, tex_coords));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Mesh::release()
{
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    index_count_ = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// 构造 / 析构
// ─────────────────────────────────────────────────────────────────────────────
Mesh::Mesh(std::vector<Vertex> verts, std::vector<uint32_t> indices)
{
    setup(verts, indices);
}

Mesh::~Mesh()
{
    release();
}

Mesh::Mesh(Mesh&& o) noexcept
    : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_), index_count_(o.index_count_)
{
    o.vao_ = o.vbo_ = o.ebo_ = 0;
    o.index_count_ = 0;
}

Mesh& Mesh::operator=(Mesh&& o) noexcept
{
    if (this != &o) {
        release();
        vao_         = o.vao_;
        vbo_         = o.vbo_;
        ebo_         = o.ebo_;
        index_count_ = o.index_count_;
        o.vao_ = o.vbo_ = o.ebo_ = 0;
        o.index_count_ = 0;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// draw
// ─────────────────────────────────────────────────────────────────────────────
void Mesh::draw() const
{
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_cube
//   单位立方体 [-0.5, 0.5]^3，6 面 × 4 顶点 = 24 顶点
//   每面有正确的外法线和 UV [0,1]^2
// ─────────────────────────────────────────────────────────────────────────────
Mesh Mesh::create_cube()
{
    // 辅助：构建一个面的 4 个顶点
    // p0..p3 逆时针排列（正面朝外）
    auto face = [](std::vector<Vertex>& V, std::vector<uint32_t>& I,
                   glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                   glm::vec3 n) {
        uint32_t base = static_cast<uint32_t>(V.size());
        V.push_back({p0, n, {0,0}});
        V.push_back({p1, n, {1,0}});
        V.push_back({p2, n, {1,1}});
        V.push_back({p3, n, {0,1}});
        // 两个三角形（CCW）
        I.insert(I.end(), {base,base+1,base+2, base,base+2,base+3});
    };

    std::vector<Vertex>   V;
    std::vector<uint32_t> I;
    V.reserve(24);
    I.reserve(36);

    // +Z 面（前）
    face(V,I, {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
              { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {0,0,1});
    // -Z 面（后）—— 顶点顺序反转保持 CCW
    face(V,I, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f},
              {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {0,0,-1});
    // +X 面（右）
    face(V,I, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f},
              { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, {1,0,0});
    // -X 面（左）
    face(V,I, {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f},
              {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, {-1,0,0});
    // +Y 面（上）
    face(V,I, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
              { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, {0,1,0});
    // -Y 面（下）
    face(V,I, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
              { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, {0,-1,0});

    return Mesh(std::move(V), std::move(I));
}

// ─────────────────────────────────────────────────────────────────────────────
// create_quad  —  全屏四边形（后处理）
// ─────────────────────────────────────────────────────────────────────────────
Mesh Mesh::create_quad()
{
    std::vector<Vertex> V = {
        {{-1,-1,0}, {0,0,1}, {0,0}},
        {{ 1,-1,0}, {0,0,1}, {1,0}},
        {{ 1, 1,0}, {0,0,1}, {1,1}},
        {{-1, 1,0}, {0,0,1}, {0,1}},
    };
    std::vector<uint32_t> I = {0,1,2, 0,2,3};
    return Mesh(std::move(V), std::move(I));
}

// ─────────────────────────────────────────────────────────────────────────────
// create_sphere  —  UV 球体（单位半径）
// ─────────────────────────────────────────────────────────────────────────────
Mesh Mesh::create_sphere(int stacks, int slices)
{
    std::vector<Vertex>   V;
    std::vector<uint32_t> I;

    const float PI = 3.14159265358979323846f;

    for (int i = 0; i <= stacks; ++i) {
        float phi   = PI * i / stacks;          // 0 .. π
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int j = 0; j <= slices; ++j) {
            float theta    = 2.0f * PI * j / slices;  // 0 .. 2π
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            glm::vec3 pos = {
                sinPhi * cosTheta,
                cosPhi,
                sinPhi * sinTheta
            };
            glm::vec2 uv = {
                (float)j / slices,
                (float)i / stacks
            };
            V.push_back({pos, pos /*法线 = 位置（单位球）*/, uv});
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            uint32_t a = i * (slices+1) + j;
            uint32_t b = a + 1;
            uint32_t c = a + (slices+1);
            uint32_t d = c + 1;
            I.insert(I.end(), {a,c,b, b,c,d});
        }
    }

    return Mesh(std::move(V), std::move(I));
}
