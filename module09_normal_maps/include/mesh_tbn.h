#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// Extended vertex with tangent + bitangent for normal mapping
struct VertexTBN {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

class MeshTBN {
public:
    GLuint vao{0}, vbo{0}, ebo{0};
    GLsizei index_count{0};

    void upload(const std::vector<VertexTBN>& verts,
                const std::vector<uint32_t>& indices)
    {
        index_count = (GLsizei)indices.size();
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     verts.size()*sizeof(VertexTBN),
                     verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size()*sizeof(uint32_t),
                     indices.data(), GL_STATIC_DRAW);
        // layout(location=0) position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VertexTBN),
                              (void*)offsetof(VertexTBN,position));
        // layout(location=1) normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VertexTBN),
                              (void*)offsetof(VertexTBN,normal));
        // layout(location=2) uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(VertexTBN),
                              (void*)offsetof(VertexTBN,uv));
        // layout(location=3) tangent
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(VertexTBN),
                              (void*)offsetof(VertexTBN,tangent));
        // layout(location=4) bitangent
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(VertexTBN),
                              (void*)offsetof(VertexTBN,bitangent));
        glBindVertexArray(0);
    }

    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void destroy() {
        if(vao){glDeleteVertexArrays(1,&vao);vao=0;}
        if(vbo){glDeleteBuffers(1,&vbo);vbo=0;}
        if(ebo){glDeleteBuffers(1,&ebo);ebo=0;}
    }
};
