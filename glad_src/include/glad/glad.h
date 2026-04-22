/* glad/glad.h — compatibility shim for glad v1 API over glad2 */
#pragma once
#include <glad/gl.h>

/* glad v1 used GLADloadproc, glad2 uses GLADloadfunc (same signature) */
typedef GLADloadfunc GLADloadproc;

/* glad v1 function: gladLoadGLLoader(proc) */
static inline int gladLoadGLLoader(GLADloadproc load) {
    return gladLoadGL(load);
}
