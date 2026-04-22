#pragma once
#include <glad/glad.h>
#include <cstdint>

enum class YuvColorSpace { BT601, BT709 };
enum class YuvRange { Studio, Full };

class YuvRenderer {
public:
    bool init(int width, int height);

    // 同步上传（glTexSubImage2D）
    void upload_frame(const uint8_t* y, const uint8_t* u, const uint8_t* v);

    // 双 PBO 异步上传（乒乓）
    void upload_frame_pbo(const uint8_t* y, const uint8_t* u, const uint8_t* v);

    void render(YuvColorSpace cs = YuvColorSpace::BT601,
                YuvRange range   = YuvRange::Studio);

    void destroy();

    int width()  const { return width_;  }
    int height() const { return height_; }

private:
    GLuint vao_{0}, vbo_{0};
    GLuint tex_y_{0}, tex_u_{0}, tex_v_{0};
    GLuint pbos_[2]{0, 0};  // 双 PBO（Y + U + V 分别三组，此处简化为一组/平面）
    int    pbo_idx_{0};
    GLuint prog_{0};
    int    width_{0}, height_{0};

    // 内部：编译着色器
    bool build_shader();
};
