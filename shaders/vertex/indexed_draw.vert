#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_GOOGLE_include_directive: require

#include "../input_structures.glsl"

layout (location = 1) out vec2 color_uv;
layout (location = 2) out vec2 metal_rough_uv;
layout (location = 3) out vec2 normal_uv;
layout (location = 4) out vec2 occlusion_uv;
layout (location = 5) out vec2 emissive_uv;
layout (location = 6) out vec2 clearcoat_uv;
layout (location = 7) out vec2 clearcoat_rough_uv;
layout (location = 8) out vec2 clearcoat_normal_uv;
layout (location = 9) out vec2 specular_strength_uv;
layout (location = 10) out vec2 specular_color_uv;
layout (location = 11) out vec3 surface_normal;
layout (location = 12) out vec4 vert_pos;
layout (location = 13) out vec4 tangent;

void main() {
    Vertex v = constants.vertex_buffer.vertices[gl_VertexIndex];
    mat4 model = constants.global_transform * constants.local_transform;

    vert_pos = model * vec4(v.pos.xyz, 1.f);
    gl_Position = scene_data.proj * scene_data.view * vert_pos;

    surface_normal = mat3(model) * v.norm.xyz;

    PBR_Material mat = material_buf.materials[nonuniformEXT (constants.material_i)];

    color_uv = v.tex_coord[mat.color_tex_coord];
    metal_rough_uv = v.tex_coord[mat.metal_rough_tex_coord];
    normal_uv = v.tex_coord[mat.normal_tex_coord];
    occlusion_uv = v.tex_coord[mat.occlusion_tex_coord];
    emissive_uv = v.tex_coord[mat.emissive_tex_coord];

    // extensions
    clearcoat_uv = v.tex_coord[mat.clearcoat_tex_coord];
    clearcoat_rough_uv = v.tex_coord[mat.clearcoat_rough_tex_coord];
    clearcoat_normal_uv = v.tex_coord[mat.clearcoat_normal_tex_coord];
    specular_strength_uv = v.tex_coord[mat.specular_strength_tex_coord];
    specular_color_uv = v.tex_coord[mat.specular_color_tex_coord];

    tangent = model * v.tangent;
}