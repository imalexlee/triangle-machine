#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 surface_normal;
layout(location = 3) in vec3 eye_pos;
layout(location = 4) in vec3 vert_pos;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D color_texture;

void main() {
    out_color = texture(color_texture, uv);
}
