#include "gbuffer.h"
#include <stdexcept>

bool GBuffer::create(int w, int h) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // ── Position (RGB16F) ─────────────────────────────────────────────────────
    glGenTextures(1, &pos_tex);
    glBindTexture(GL_TEXTURE_2D, pos_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pos_tex, 0);

    // ── Normal (RGB16F) ───────────────────────────────────────────────────────
    glGenTextures(1, &normal_tex);
    glBindTexture(GL_TEXTURE_2D, normal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_tex, 0);

    // ── Albedo + Specular (RGBA8) ─────────────────────────────────────────────
    glGenTextures(1, &albedo_spec_tex);
    glBindTexture(GL_TEXTURE_2D, albedo_spec_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, albedo_spec_tex, 0);

    // ── MRT: tell OpenGL we draw into all three color attachments ─────────────
    GLenum attachments[3] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2
    };
    glDrawBuffers(3, attachments);

    // ── Depth renderbuffer ────────────────────────────────────────────────────
    glGenRenderbuffers(1, &depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);

    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void GBuffer::bind_geometry_pass() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void GBuffer::bind_textures() {
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, pos_tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, normal_tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, albedo_spec_tex);
}

void GBuffer::destroy() {
    GLuint textures[] = {pos_tex, normal_tex, albedo_spec_tex};
    glDeleteTextures(3, textures);
    if (depth_rbo) { glDeleteRenderbuffers(1, &depth_rbo); depth_rbo = 0; }
    if (fbo)       { glDeleteFramebuffers(1, &fbo);        fbo = 0; }
}
