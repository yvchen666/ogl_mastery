#pragma once
#include <glad/glad.h>
#include <vector>

struct FboAttachment {
    GLuint tex{0};
    GLenum internal_fmt;
    GLenum attachment;
};

// General-purpose framebuffer wrapper
class Framebuffer {
public:
    GLuint fbo{0};
    int    width{0}, height{0};

    bool create(int w, int h,
                std::vector<FboAttachment> color_attachments,
                bool with_depth_rbo = true);

    void bind()   const;
    static void unbind();

    GLuint color_tex(int idx = 0) const;
    void   destroy();

private:
    std::vector<GLuint> color_textures_;
    GLuint              depth_rbo_{0};
};
