#include "tangent_calculator.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

TangentData calc_tangents(
    const std::vector<Vertex>& verts,
    const std::vector<uint32_t>& indices)
{
    size_t n = verts.size();
    std::vector<glm::vec3> tangents  (n, glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(n, glm::vec3(0.0f));

    // ── 1. Accumulate per-triangle tangents onto vertices ──────────────────
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i+0];
        uint32_t i1 = indices[i+1];
        uint32_t i2 = indices[i+2];

        const Vertex& v0 = verts[i0];
        const Vertex& v1 = verts[i1];
        const Vertex& v2 = verts[i2];

        glm::vec3 dP1 = v1.position - v0.position;
        glm::vec3 dP2 = v2.position - v0.position;
        glm::vec2 dUV1 = v1.uv - v0.uv;
        glm::vec2 dUV2 = v2.uv - v0.uv;

        // Solve:  [dP1; dP2] = [dU1 dV1; dU2 dV2] * [T; B]
        // => [T; B] = inv([dU1 dV1; dU2 dV2]) * [dP1; dP2]
        float det = dUV1.x * dUV2.y - dUV1.y * dUV2.x;
        if (std::abs(det) < 1e-8f) continue; // degenerate UV

        float inv = 1.0f / det;
        glm::vec3 T = inv * ( dUV2.y * dP1 - dUV1.y * dP2);
        glm::vec3 B = inv * (-dUV2.x * dP1 + dUV1.x * dP2);

        tangents  [i0] += T;  tangents  [i1] += T;  tangents  [i2] += T;
        bitangents[i0] += B;  bitangents[i1] += B;  bitangents[i2] += B;
    }

    // ── 2. Gram-Schmidt orthogonalization per vertex ───────────────────────
    // Ensure T is orthogonal to N and normalized.
    // Also compute handedness-corrected B = cross(N,T)*handedness.
    for (size_t i = 0; i < n; ++i) {
        const glm::vec3& N = verts[i].normal;
        glm::vec3 T = tangents[i];
        glm::vec3 B = bitangents[i];

        // Gram-Schmidt: T' = normalize(T - dot(T,N)*N)
        T = glm::normalize(T - glm::dot(T, N) * N);

        // Determine handedness: if cross(N,T)·B < 0, flip B
        float hand = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;
        B = glm::cross(N, T) * hand;

        tangents  [i] = T;
        bitangents[i] = B;
    }

    return TangentData{tangents, bitangents};
}
