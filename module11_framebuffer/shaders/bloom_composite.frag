#version 460 core

in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uHdrBuffer;
uniform sampler2D uBloomBlur;
uniform float     uBloomStrength; // default ~0.04

void main() {
    vec3 hdr   = texture(uHdrBuffer, vTexCoord).rgb;
    vec3 bloom = texture(uBloomBlur, vTexCoord).rgb;

    // Additive composite: bloom adds light energy on top of scene
    vec3 result = hdr + bloom * uBloomStrength;

    // ACES tone mapping
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    result = clamp((result*(a*result+b))/(result*(c*result+d)+e), 0.0, 1.0);

    // Gamma
    result = pow(result, vec3(1.0/2.2));
    FragColor = vec4(result, 1.0);
}
