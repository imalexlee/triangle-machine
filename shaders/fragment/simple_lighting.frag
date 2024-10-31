#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"

layout (location = 0) out vec4 out_color;

layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec2 metal_rough_uv;
layout (location = 3) in vec3 surface_normal;
layout (location = 4) in vec4 vert_pos;



void main() {
    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];
    vec4 color = mat.color_factors * texture(tex_samplers[nonuniformEXT(mat.color_tex_i)], color_uv);
    vec3 light_color = vec3(1.0);
    // ambient
    vec3 ambient = 0.15 * light_color;
    // diffuse
    vec3 normal = normalize(surface_normal);
    vec3 light_dir = normalize(vec3(0.5, 1, 0.5));
    float diffuse_factor = max(dot(normal, light_dir), 0.1);
    vec3 diffuse = diffuse_factor * light_color;
    // specular
    vec3 view_dir = normalize(scene_data.eye_pos.xyz - vert_pos.xyz);
    float specular_factor = 0.0;
    vec3 halfway_dir = normalize(light_dir + view_dir);
    specular_factor = pow(max(dot(normal, halfway_dir), 0.0), 64.0);
    vec3 specular = specular_factor * light_color;

    rayQueryEXT rq;
    float infinity = 1.0 / 0;
    rayQueryInitializeEXT(rq, accel_struct, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vert_pos.xyz, 0.1, light_dir, infinity);
    rayQueryProceedEXT(rq);

    // 1.0 for occluded (in shadow) and 0.0 for not occluded
    float occlued = 0.0;
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        // in shadow
        occlued = 1.0;
    }

    out_color = vec4(ambient + (1.0 - occlued) * (diffuse + specular), 1.0) * color;
}
