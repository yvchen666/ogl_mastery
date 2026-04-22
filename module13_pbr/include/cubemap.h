#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

// Cubemap 封装（天空盒 + IBL）
class Cubemap {
public:
    GLuint id{0};

    // 从 6 张图片加载（右/左/上/下/前/后）
    static Cubemap from_faces(const std::vector<std::string>& paths);

    // 从 equirectangular HDR 贴图转换（FBO 渲染 6 面）
    static Cubemap from_equirect(GLuint equirect_tex, int size = 512);

    // 生成 irradiance map（漫反射 IBL，对半球积分）
    static Cubemap convolve_irradiance(GLuint env_cubemap, int size = 32);

    // 生成 prefilter map（镜面 IBL，不同 mip 对应不同 roughness）
    static Cubemap prefilter_env(GLuint env_cubemap, int size = 128);

    void bind(int slot = 0) const;
    void destroy();
};
