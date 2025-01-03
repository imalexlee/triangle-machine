#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_GOOGLE_include_directive: require

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec4 eye_pos;
} scene_data;


layout (location = 0) out vec3 nearPoint;
layout (location = 1) out vec3 farPoint;
layout (location = 2) out mat4 view;
layout (location = 6) out mat4 projection;


vec3 unprojectPoint(float x, float y, float z, mat4 viewProjectionInverse) {
    vec4 clipSpacePos = vec4(x, y, z, 1.0);
    vec4 eyeSpacePos = viewProjectionInverse * clipSpacePos;
    return eyeSpacePos.xyz / eyeSpacePos.w;
}

vec3 gridPlane[6] = vec3[](

vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

void main() {
    vec3 pos = gridPlane[gl_VertexIndex].xyz;

    mat4 view_proj_inv = inverse(scene_data.proj * scene_data.view);

    nearPoint = unprojectPoint(pos.x, pos.y, 0.0, view_proj_inv);
    farPoint = unprojectPoint(pos.x, pos.y, 1.0, view_proj_inv);

    view = scene_data.view;
    projection = scene_data.proj;

    gl_Position = vec4(pos, 1.0);
}