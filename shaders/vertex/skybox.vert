#version 460

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view_proj;
    vec3 eye_pos;
    vec3 dir;
} scene_data;

layout (set = 1, binding = 0) uniform samplerCube sky_box_tex;

layout (location = 0) in vec3 vertex;
layout (location = 0) out vec3 dir;

void main() {
    dir = scene_data.dir;
    gl_Position = scene_data.view_proj * vec4(vertex, 1.f);
    //* vec4(scene_data.eye_pos, 1.f);
}
