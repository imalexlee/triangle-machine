#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_GOOGLE_include_directive: require

#include "../input_structures.glsl"

layout (location = 1) out vec2 color_uv;
layout (location = 2) out vec2 metal_rough_uv;
layout (location = 3) out vec2 normal_uv;
layout (location = 4) out vec3 surface_normal;
layout (location = 5) out vec4 vert_pos;

void main() {
    Vertex v = constants.vertex_buffer.vertices[gl_VertexIndex];

    vert_pos = constants.global_transform * constants.local_transform * vec4(v.pos.xyz, 1.f);
    gl_Position = scene_data.proj * scene_data.view * vert_pos;

    surface_normal = mat3(constants.local_transform) * v.norm.xyz;


    PBR_Material mat = material_buf.materials[nonuniformEXT (constants.material_i)];

    color_uv = v.tex_coord[mat.color_tex_coord];

    metal_rough_uv = v.tex_coord[mat.metal_rough_tex_coord];

    normal_uv = v.tex_coord[mat.normal_tex_coord];
}