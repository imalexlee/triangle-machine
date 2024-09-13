#version 450

#extension GL_EXT_buffer_reference: require
#extension GL_GOOGLE_include_directive: require

#include "../input_structures.glsl"

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
    mat4 viewInv = inverse(scene_data.view);
    mat4 projInv = inverse(scene_data.proj);
    nearPoint = unprojectPoint(pos.x, pos.y, 0.0, viewInv * projInv);
    farPoint = unprojectPoint(pos.x, pos.y, 1.0, viewInv * projInv);

    view = scene_data.view;
    projection = scene_data.proj;

    gl_Position = vec4(pos, 1.0);


}
