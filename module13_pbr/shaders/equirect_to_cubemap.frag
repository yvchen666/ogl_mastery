#version 460 core

in vec3 vLocalPos;
out vec4 FragColor;

uniform sampler2D uEquirectMap;

const vec2 inv_atan = vec2(0.1591, 0.3183); // 1/(2π), 1/π

// Convert a 3D direction to equirectangular UV
vec2 sample_spherical_map(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= inv_atan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv    = sample_spherical_map(normalize(vLocalPos));
    vec3 color = texture(uEquirectMap, uv).rgb;
    FragColor  = vec4(color, 1.0);
}
