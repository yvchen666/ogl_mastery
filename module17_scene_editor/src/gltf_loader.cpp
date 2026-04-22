#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include "gltf_loader.h"
#include "components.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// 内部辅助：将 tinygltf accessor 的数据上传到 OpenGL Buffer
// ─────────────────────────────────────────────────────────────────────────────
static GLuint upload_buffer(const tinygltf::Model& model,
                             int accessor_idx,
                             GLenum target) {
    const auto& acc  = model.accessors[accessor_idx];
    const auto& bv   = model.bufferViews[acc.bufferView];
    const auto& buf  = model.buffers[bv.buffer];
    const uint8_t* ptr = buf.data.data() + bv.byteOffset + acc.byteOffset;

    GLuint gl_buf;
    glGenBuffers(1, &gl_buf);
    glBindBuffer(target, gl_buf);
    glBufferData(target, (GLsizeiptr)bv.byteLength, ptr, GL_STATIC_DRAW);
    return gl_buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// 内部辅助：为 glTF mesh primitive 创建 VAO
// ─────────────────────────────────────────────────────────────────────────────
static MeshComponent build_mesh(const tinygltf::Model&     model,
                                 const tinygltf::Primitive& prim) {
    MeshComponent mc{};
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // ── 顶点属性 ──────────────────────────────────────────────────────
    auto bind_attrib = [&](const std::string& name, int location) {
        auto it = prim.attributes.find(name);
        if (it == prim.attributes.end()) return;
        const auto& acc = model.accessors[it->second];
        GLuint vbo = upload_buffer(model, it->second, GL_ARRAY_BUFFER);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        int num_comp = (acc.type == TINYGLTF_TYPE_VEC3) ? 3 :
                       (acc.type == TINYGLTF_TYPE_VEC2) ? 2 : 4;
        glVertexAttribPointer(location, num_comp, acc.componentType,
                              acc.normalized ? GL_TRUE : GL_FALSE,
                              (GLsizei)model.bufferViews[acc.bufferView].byteStride,
                              (void*)(uintptr_t)acc.byteOffset);
        glEnableVertexAttribArray(location);
    };
    bind_attrib("POSITION",   0);
    bind_attrib("NORMAL",     1);
    bind_attrib("TEXCOORD_0", 2);

    // ── 索引缓冲 ──────────────────────────────────────────────────────
    mc.index_count = 0;
    if (prim.indices >= 0) {
        const auto& acc = model.accessors[prim.indices];
        mc.index_count = (int)acc.count;
        GLuint ebo = upload_buffer(model, prim.indices, GL_ELEMENT_ARRAY_BUFFER);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    glBindVertexArray(0);
    mc.vao = vao;

    // ── 基础色纹理 ─────────────────────────────────────────────────────
    if (prim.material >= 0) {
        const auto& mat = model.materials[prim.material];
        int tex_idx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (tex_idx >= 0) {
            const auto& gltf_tex = model.textures[tex_idx];
            const auto& img = model.images[gltf_tex.source];
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            GLenum fmt = (img.component == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, (GLint)fmt, img.width, img.height,
                         0, fmt, GL_UNSIGNED_BYTE, img.image.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            mc.albedo_tex = tex;
        }
    }

    return mc;
}

// ─────────────────────────────────────────────────────────────────────────────
// load_gltf
// ─────────────────────────────────────────────────────────────────────────────
void load_gltf(const std::string& path, World& world) {
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;

    bool ok = path.size() > 4 && path.substr(path.size()-4) == ".glb"
            ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
            : loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty()) std::cerr << "[gltf] warn: " << warn << "\n";
    if (!ok)           { std::cerr << "[gltf] error: " << err << "\n"; return; }

    std::cout << "[gltf] Loaded: " << path
              << " | meshes=" << model.meshes.size()
              << " | nodes=" << model.nodes.size() << "\n";

    // ── 遍历所有节点 ──────────────────────────────────────────────────
    for (int ni = 0; ni < (int)model.nodes.size(); ++ni) {
        const auto& node = model.nodes[ni];
        EntityId eid = world.create_entity();

        // 名称
        NameComponent nc;
        nc.name = node.name.empty() ? ("Node_" + std::to_string(ni)) : node.name;
        world.add_component(eid, nc);

        // 变换
        TransformComponent tc;
        if (!node.translation.empty())
            tc.pos = glm::vec3((float)node.translation[0],
                               (float)node.translation[1],
                               (float)node.translation[2]);
        if (!node.scale.empty())
            tc.scale = glm::vec3((float)node.scale[0],
                                 (float)node.scale[1],
                                 (float)node.scale[2]);
        if (!node.rotation.empty())
            tc.rot = glm::quat((float)node.rotation[3],  // w,x,y,z (glm 构造顺序)
                               (float)node.rotation[0],
                               (float)node.rotation[1],
                               (float)node.rotation[2]);
        world.add_component(eid, tc);

        // 网格
        if (node.mesh >= 0) {
            const auto& mesh = model.meshes[node.mesh];
            for (const auto& prim : mesh.primitives) {
                MeshComponent mc = build_mesh(model, prim);
                world.add_component(eid, mc);
                break;  // 每节点只取第一个 primitive（简化）
            }
        }
    }
}
