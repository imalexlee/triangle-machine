#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D color_texture;

void main() {
    //out_color = vec4(fragColor, 1.0);
    out_color = texture(color_texture, uv);
}
