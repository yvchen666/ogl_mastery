#version 460 core
// Post-process effects: Sobel edge detection and DoF (selected via uEffect)
// uEffect: 0=Sobel edge, 1=DoF blur

in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uColorBuffer;
uniform sampler2D uDepthBuffer;
uniform int       uEffect;
uniform float     uFocusDepth;  // NDC depth of focus plane (0-1), used for DoF
uniform float     uDofStrength; // DoF blur radius multiplier

// ── Sobel edge detection ──────────────────────────────────────────────────────
vec3 sobel_edge() {
    vec2 texel = 1.0 / textureSize(uColorBuffer, 0);
    // 3x3 neighborhood luminance
    float p[9];
    int k = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            vec3 c = texture(uColorBuffer, vTexCoord + vec2(dx,dy)*texel).rgb;
            p[k++] = dot(c, vec3(0.2126, 0.7152, 0.0722));
        }
    // Gx = [ -1  0  1; -2  0  2; -1  0  1 ]
    float Gx = -p[0] + p[2] - 2.0*p[3] + 2.0*p[5] - p[6] + p[8];
    // Gy = [ -1 -2 -1;  0  0  0;  1  2  1 ]
    float Gy = -p[0] - 2.0*p[1] - p[2] + p[6] + 2.0*p[7] + p[8];
    float G  = sqrt(Gx*Gx + Gy*Gy);
    return vec3(G);
}

// ── Depth of Field (Circle of Confusion) ─────────────────────────────────────
vec3 dof_blur() {
    float depth = texture(uDepthBuffer, vTexCoord).r;
    // CoC proportional to distance from focus plane
    float coc   = abs(depth - uFocusDepth) * uDofStrength;
    int   radius = int(clamp(coc * 10.0, 0.0, 6.0));

    vec2 texel = 1.0 / textureSize(uColorBuffer, 0);
    vec3 sum = vec3(0.0);
    int  count = 0;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            sum += texture(uColorBuffer, vTexCoord + vec2(dx,dy)*texel).rgb;
            ++count;
        }
    return sum / float(count);
}

void main() {
    vec3 result;
    if (uEffect == 0) result = sobel_edge();
    else              result = dof_blur();
    FragColor = vec4(result, 1.0);
}
