#version 460 core

in vec3 vNormal;
in vec3 vWorldPos;
flat in int vInstanceID;

out vec4 FragColor;

void main() {
    // Simple hash-based color per instance group
    float hue = float(vInstanceID % 360) / 360.0;
    // HSV to RGB (simple version)
    float h = hue * 6.0;
    float r = abs(h - 3.0) - 1.0;
    float g = 2.0 - abs(h - 2.0);
    float b = 2.0 - abs(h - 4.0);
    vec3 color = clamp(vec3(r, g, b), 0.0, 1.0);

    // Simple diffuse lighting from above
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(1.0, 2.0, 1.0));
    float diff = max(dot(N, L), 0.0) * 0.8 + 0.2; // ambient + diffuse

    FragColor = vec4(color * diff, 1.0);
}
