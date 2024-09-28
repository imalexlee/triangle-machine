#version 450

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"

/**
layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec4 eye_pos;
} scene_data;

layout (set = 1, binding = 0) uniform samplerCube sky_box_tex;
*/

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 out_color;

void main() {
    // out_color = vec4(dir, 1.f) * vec4(1.f, 0.f, 1.f, 1.f);
    out_color = texture(sky_box_tex, dir);
    // out_color = vec4(0, 0, 1, 1);
}