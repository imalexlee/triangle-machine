#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_GOOGLE_include_directive: require

#include "../input_structures.glsl"

layout (push_constant) uniform PushConstants {
    vec3 pos;
} constants;

layout (location = 0) out vec3 frag_color;
layout (location = 1) out vec2 color_uv;
layout (location = 2) out vec3 surface_normal;
layout (location = 3) out vec3 eye_pos;
layout (location = 4) out vec3 vert_pos;

void main() {
    Vertex v = obj_data.vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = scene_data.view_proj * (obj_data.local_transform * vec4(v.pos.xyz, 1.f) + vec4(constants.pos, 1.f));
    frag_color = (obj_data.local_transform * vec4(v.norm.xyz, 1.f)).xyz;
    surface_normal = v.norm.xyz;
    eye_pos = scene_data.eye_pos;
    vert_pos = v.pos.xyz;

    color_uv = v.tex_coord[mat_data.color_tex_i];
}
