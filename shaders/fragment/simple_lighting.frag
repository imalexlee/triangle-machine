#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"


layout (location = 0) out vec4 out_color;
layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec3 surface_normal;

layout (location = 4) in vec3 vert_pos;


void main() {

    vec3 light_dir = normalize(vec3(1, 1, 0));

    float light_value = max(dot(surface_normal, light_dir), 0.1f);

    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];

    vec4 color = mat.color_factors * texture(tex_samplers[mat.color_tex_i], color_uv);
    //vec4 color = mat_data.color_factors * texture(color_tex, color_uv);
    vec4 ambient = color * vec4(0.3, 0.3, 0.3, 1.0);


    float infinity = 1.0 / 0.0;

    rayQueryEXT rq;

    vec3 ray_dir = vert_pos - scene_data.eye_pos.xyz;
    rayQueryInitializeEXT(rq, accel_struct, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vert_pos, 0.1, light_dir, 100000000.0);
    rayQueryProceedEXT(rq);

    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        // not in shadow
        out_color = color + ambient;
    } else {
        // in shadow
        //        out_color = color * vec4(0.1, 0.1, 0.1, 1.0);
        out_color = vec4(0.0, 0, 1.0, 1.0);
    }

    out_color = color + ambient;

}