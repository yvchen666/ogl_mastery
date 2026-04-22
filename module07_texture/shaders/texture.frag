#version 460 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform float     uMixRatio;   // 0 = only tex0, 1 = only tex1

// Mode: 0 = single tex, 1 = mix, 2 = filter demo (shows one of 4 sub-quads)
uniform int uMode;

void main() {
    if (uMode == 1) {
        vec4 c0 = texture(uTex0, vUV);
        vec4 c1 = texture(uTex1, vUV);
        FragColor = mix(c0, c1, uMixRatio);
    } else {
        FragColor = texture(uTex0, vUV);
    }
}
