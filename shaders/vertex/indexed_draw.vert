#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_GOOGLE_include_directive: require

#include "../input_structures.glsl"



//layout (location = 0) out vec3 frag_color;
layout (location = 1) out vec2 color_uv;
layout (location = 2) out vec3 surface_normal;
layout (location = 3) out vec3 eye_pos;
layout (location = 4) out vec3 vert_pos;

void main() {
    Vertex v = constants.vertex_buffer.vertices[gl_VertexIndex];
    // TODO: see above. this is the old way
    //gl_Position = scene_data.proj * scene_data.view * (obj_data.local_transform * vec4(v.pos.xyz, 1.f) + vec4(constants.pos, 1.f));
    gl_Position = scene_data.proj * scene_data.view * (constants.local_transform * vec4(v.pos.xyz, 1.f));
    //   frag_color = (obj_data.local_transform * vec4(v.norm.xyz, 1.f)).xyz;
    surface_normal = v.norm.xyz;
    eye_pos = scene_data.eye_pos.xyz;
    vert_pos = v.pos.xyz;
    uint color_tex_coord = material_buf.materials[constants.material_i].color_tex_coord;
    color_uv = v.tex_coord[color_tex_coord];
}