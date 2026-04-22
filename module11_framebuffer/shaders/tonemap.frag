#version 460 core

in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uHdrBuffer;
// uMode: 0=passthrough(linear), 1=Reinhard, 2=Exposure, 3=ACES
uniform int   uMode;
uniform float uExposure;  // used in mode 2

// ACES Filmic tone mapping approximation (Narkowicz 2015)
vec3 aces_filmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHdrBuffer, vTexCoord).rgb;

    vec3 ldr;
    if      (uMode == 1) ldr = hdr / (hdr + vec3(1.0));                          // Reinhard
    else if (uMode == 2) ldr = vec3(1.0) - exp(-hdr * uExposure);                // Exposure
    else if (uMode == 3) ldr = aces_filmic(hdr);                                  // ACES Filmic
    else                  ldr = hdr;                                               // passthrough

    // Gamma correction (must happen AFTER tone mapping, not before)
    ldr = pow(ldr, vec3(1.0 / 2.2));
    FragColor = vec4(ldr, 1.0);
}
