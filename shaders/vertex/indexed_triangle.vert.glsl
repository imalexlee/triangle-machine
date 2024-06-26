#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "../input_structures.glsl"

layout(push_constant) uniform PushConstants {
    // will be used to transform vertex normal
    mat4 local_transform;
    VertexBuffer vertex_buffer;
} constants;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec3 surface_normal;
layout(location = 3) out vec3 eye_pos;
layout(location = 4) out vec3 vert_pos;

void main() {
    Vertex v = obj_data.vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = scene_data.view_proj * obj_data.local_transform * vec4(v.position, 1.0f);
    frag_color = (obj_data.local_transform * vec4(v.normal, 0)).xyz;
    surface_normal = v.normal;
    eye_pos = scene_data.eye_pos;
    vert_pos = v.position;
    uv = vec2(v.uv_x, v.uv_y);
}
