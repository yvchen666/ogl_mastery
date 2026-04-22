#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh {
public:
    GLuint vao{0}, vbo{0}, ebo{0};
    GLsizei index_count{0};

    // Upload geometry to GPU
    void upload(const std::vector<Vertex>& verts,
                const std::vector<uint32_t>& indices)
    {
        index_count = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     verts.size() * sizeof(Vertex),
                     verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(uint32_t),
                     indices.data(), GL_STATIC_DRAW);

        // position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, position));
        // normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, normal));
        // uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, uv));

        glBindVertexArray(0);
    }

    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void destroy() {
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    }

    // ---- Factory helpers ----
    static Mesh make_quad(float size = 1.0f) {
        float h = size * 0.5f;
        std::vector<Vertex> verts = {
            {{-h,-h,0},{0,0,1},{0,0}},
            {{ h,-h,0},{0,0,1},{1,0}},
            {{ h, h,0},{0,0,1},{1,1}},
            {{-h, h,0},{0,0,1},{0,1}},
        };
        std::vector<uint32_t> idx = {0,1,2, 2,3,0};
        Mesh m; m.upload(verts, idx); return m;
    }

    static Mesh make_cube(float size = 1.0f) {
        float h = size * 0.5f;
        // 6 faces × 4 verts
        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;
        auto face = [&](glm::vec3 n,
                        glm::vec3 p0, glm::vec3 p1,
                        glm::vec3 p2, glm::vec3 p3) {
            uint32_t base = (uint32_t)verts.size();
            verts.push_back({p0, n, {0,0}});
            verts.push_back({p1, n, {1,0}});
            verts.push_back({p2, n, {1,1}});
            verts.push_back({p3, n, {0,1}});
            idx.insert(idx.end(), {base,base+1,base+2, base+2,base+3,base});
        };
        face({ 0, 0, 1},{-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}); // +Z
        face({ 0, 0,-1},{ h,-h,-h},{-h,-h,-h},{-h, h,-h},{ h, h,-h}); // -Z
        face({-1, 0, 0},{-h,-h,-h},{-h,-h, h},{-h, h, h},{-h, h,-h}); // -X
        face({ 1, 0, 0},{ h,-h, h},{ h,-h,-h},{ h, h,-h},{ h, h, h}); // +X
        face({ 0, 1, 0},{-h, h, h},{ h, h, h},{ h, h,-h},{-h, h,-h}); // +Y
        face({ 0,-1, 0},{-h,-h,-h},{ h,-h,-h},{ h,-h, h},{-h,-h, h}); // -Y
        Mesh m; m.upload(verts, idx); return m;
    }

    static Mesh make_sphere(float radius = 1.0f,
                            int stacks = 32, int slices = 32) {
        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;
        const float pi = 3.14159265358979f;
        for (int i = 0; i <= stacks; ++i) {
            float phi = pi * i / stacks;
            for (int j = 0; j <= slices; ++j) {
                float theta = 2.0f * pi * j / slices;
                glm::vec3 n{
                    std::sin(phi)*std::cos(theta),
                    std::cos(phi),
                    std::sin(phi)*std::sin(theta)
                };
                Vertex v;
                v.position = n * radius;
                v.normal   = n;
                v.uv       = {(float)j/slices, (float)i/stacks};
                verts.push_back(v);
            }
        }
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < slices; ++j) {
                uint32_t a = i*(slices+1)+j;
                uint32_t b = a+1;
                uint32_t c = (i+1)*(slices+1)+j;
                uint32_t d = c+1;
                idx.insert(idx.end(), {a,c,b, b,c,d});
            }
        }
        Mesh m; m.upload(verts, idx); return m;
    }
};
