#version 450

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"


layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec3 surface_normal;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 light_dir = normalize(vec3(0, 1, 0.4));

    float light_value = max(dot(surface_normal, light_dir), 0.1f);

    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];

    vec4 color = mat.color_factors * texture(tex_samplers[mat.color_tex_i], color_uv);
    //vec4 color = mat_data.color_factors * texture(color_tex, color_uv);
    vec4 ambient = color * vec4(0.3, 0.3, 0.3, 1.0);

    out_color = color * light_value + ambient;
}