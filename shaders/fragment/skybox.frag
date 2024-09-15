#version 450

layout (set = 1, binding = 0) uniform samplerCube sky_box_tex;

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 out_color;


void main() {

    out_color = vec4(1.f, 0.f, 0.f, 1.f);
    // = texture(sky_box_tex, dir).xyz;
}
