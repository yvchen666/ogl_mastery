#version 460 core

in vec3 vLocalPos;
out vec4 FragColor;

uniform samplerCube uEnvMap;

const float PI = 3.14159265359;

// Convolve the environment cubemap over the hemisphere defined by N
// to produce an irradiance (diffuse IBL) map.
// E(n) = ∫_Ω L(ωi) (n·ωi) sin(θ) dθ dφ
// Discretised over a uniform spherical grid with step 0.025 rad.
void main() {
    vec3 N = normalize(vLocalPos);

    // Build a tangent frame around N
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    vec3  irradiance   = vec3(0.0);
    float sample_count = 0.0;
    float delta_phi    = 0.025;
    float delta_theta  = 0.025;

    for (float phi = 0.0; phi < 2.0 * PI; phi += delta_phi) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += delta_theta) {
            // Spherical to Cartesian (tangent space)
            vec3 tangent_sample = vec3(sin(theta) * cos(phi),
                                       sin(theta) * sin(phi),
                                       cos(theta));
            // Tangent to world
            vec3 sample_vec = tangent_sample.x * right
                            + tangent_sample.y * up
                            + tangent_sample.z * N;

            irradiance   += texture(uEnvMap, sample_vec).rgb * cos(theta) * sin(theta);
            sample_count += 1.0;
        }
    }

    irradiance = PI * irradiance / sample_count;
    FragColor  = vec4(irradiance, 1.0);
}
