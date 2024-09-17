#version 450

#extension GL_GOOGLE_include_directive: require
#include "../input_structures.glsl"


layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec3 surface_normal;

layout (location = 0) out vec4 out_color;

void main() {
    // vec4 c_surface = mat_data.color_factors * texture(color_tex, uv);
    vec3 light_dir = normalize(vec3(0, 1, 0.4));
    // float light_value = max(dot(surface_normal, light_dir), 0.1f);

    // out_color = c_surface * 2.5 * light_value;

    float light_value = max(dot(surface_normal, light_dir), 0.1f);

    vec4 color = mat_data.color_factors * texture(color_tex, color_uv);
    vec4 ambient = color * vec4(0.3, 0.3, 0.3, 1.0);

    out_color = color * light_value + ambient;
}
