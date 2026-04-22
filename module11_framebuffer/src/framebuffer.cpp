#include "framebuffer.h"
#include <stdexcept>
#include <vector>

bool Framebuffer::create(int w, int h,
                          std::vector<FboAttachment> color_attachments,
                          bool with_depth_rbo)
{
    width  = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    std::vector<GLenum> draw_bufs;

    for (auto& att : color_attachments) {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // Choose pixel type from internal format
        GLenum fmt  = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        if (att.internal_fmt == GL_RGBA16F || att.internal_fmt == GL_RGB16F) {
            fmt  = (att.internal_fmt == GL_RGB16F) ? GL_RGB : GL_RGBA;
            type = GL_FLOAT;
        } else if (att.internal_fmt == GL_RGB) {
            fmt = GL_RGB;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, att.internal_fmt, w, h, 0, fmt, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, att.attachment, GL_TEXTURE_2D, tex, 0);
        att.tex = tex;
        color_textures_.push_back(tex);
        draw_bufs.push_back(att.attachment);
    }

    if (!draw_bufs.empty())
        glDrawBuffers((GLsizei)draw_bufs.size(), draw_bufs.data());

    if (with_depth_rbo) {
        glGenRenderbuffers(1, &depth_rbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo_);
    }

    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint Framebuffer::color_tex(int idx) const {
    if (idx < 0 || idx >= (int)color_textures_.size()) return 0;
    return color_textures_[idx];
}

void Framebuffer::destroy() {
    for (auto t : color_textures_) glDeleteTextures(1, &t);
    color_textures_.clear();
    if (depth_rbo_) { glDeleteRenderbuffers(1, &depth_rbo_); depth_rbo_ = 0; }
    if (fbo)        { glDeleteFramebuffers(1, &fbo);         fbo = 0; }
}
