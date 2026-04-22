#include "indirect_draw.h"
#include <iostream>

void IndirectDrawBuffer::create(int max_commands) {
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER,
                 max_commands * sizeof(DrawElementsIndirectCommand),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void IndirectDrawBuffer::upload(const std::vector<DrawElementsIndirectCommand>& cmds) {
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
                    cmds.size() * sizeof(DrawElementsIndirectCommand),
                    cmds.data());
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void IndirectDrawBuffer::bind() const {
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer);
}

void IndirectDrawBuffer::destroy() {
    if (buffer) { glDeleteBuffers(1, &buffer); buffer = 0; }
}
