#version 450

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec4 eye_pos;
} scene_data;


layout (set = 1, binding = 0) uniform samplerCube sky_box_tex;

layout (location = 0) in vec3 v_pos;

layout (location = 0) out vec3 dir;


void main() {
    dir = v_pos.xyz;
    mat4 view = mat4(mat3(scene_data.view));
    gl_Position = scene_data.proj * view * vec4(v_pos, 1.f);
}
