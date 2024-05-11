#version 450

#extension GL_GOOGLE_include_directive : require

#include "../input_structures.glsl"

layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 surface_normal;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = material_data.color_factors * texture(color_tex, uv);
}
