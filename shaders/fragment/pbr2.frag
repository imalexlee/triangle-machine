#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"

layout (location = 0) out vec4 out_color;

layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec2 metal_rough_uv;
layout (location = 3) in vec2 normal_uv;
layout (location = 4) in vec3 surface_normal;
layout (location = 5) in vec4 vert_pos;
layout (location = 6) in vec4 tangent;

float PI = 3.1415926535897932384626433832795;

float ggx_distribution(vec3 normal, vec3 halfway, float roughness) {
    float alignment = max(dot(normal, halfway), 0.0);
    // a = r^2 adopted by Epic Games and Disney.
    float a_2 = pow(roughness, 2.0);
    float recip_denom = 1.0 / (PI * pow(pow(alignment, 2.0) * (a_2 - 1) + 1, 2.0));
    return a_2 * recip_denom;
}

float g1_schlick(vec3 normal, vec3 incident, float roughness) {
    float alignment = max(dot(normal, incident), 0.0);
    float recip_denom = 1.0 / (alignment * (1 - roughness) + roughness);
    return alignment * recip_denom;
}

float g2_smith(vec3 normal, vec3 view_dir, vec3 light_dir, float roughness) {
    // Disney roughness remap to reduce "hotness"
    //roughness = (roughness + 1.0) / 2;
    float k = pow(roughness + 1.0, 2.0) * 0.125; // divide by 8
    float g1_light = g1_schlick(normal, light_dir, k);
    float g1_view = g1_schlick(normal, view_dir, k);
    return g1_light * g1_view;
}

vec3 fresnel_schlick(vec3 normal, vec3 incident, vec3 albedo, float metallic)
{
    float alignment = max(dot(normal, incident), 0.000);
    vec3 f_0 = mix(vec3(0.04), albedo, metallic);
    return f_0 + (1.0 - f_0) * pow(clamp(1.0 - alignment, 0.0, 1.0), 5.0);
}

void main() {
    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];

    vec3 view_dir = normalize(scene_data.eye_pos.xyz - vert_pos.xyz);
    vec3 normal = normalize(surface_normal);

    // Bump Mapping
    vec3 bump_tex_val = texture(tex_samplers[nonuniformEXT(mat.normal_tex_i)], normal_uv).xyz;

    // if not the default texture (aka there is a bump value associated with this vertex)
    if (bump_tex_val != vec3(1, 1, 1)) {
        // convert to [-1, 1] range
        bump_tex_val = bump_tex_val * 2.0 - vec3(1.0);
        vec3 bi_tangent = normalize(cross(normal, tangent.xyz) * tangent.w);
        mat3 TBN = mat3(tangent.xyz, bi_tangent, normal);
        normal = normalize(TBN * bump_tex_val);
    }

    vec3 light_color = vec3(23.47, 21.31, 20.79);
    vec3 light_pos = vec3(10, 10, 5);
    vec3 light_dir = normalize(light_pos);
    float light_distance = length(light_pos - vert_pos.xyz);
    float attenuation = 1.0 / pow(light_distance, 2.0);
    float light_intensity = 0.5;


    float n_dot_l = max(dot(normal, light_dir), 0.0);
    float n_dot_v = max(dot(normal, view_dir), 0.0);

    vec3 radiance = light_color * light_intensity;

    vec4 loaded_tex_color = texture(tex_samplers[nonuniformEXT (mat.color_tex_i)], color_uv);
    vec4 tex_color = mat.color_factors * loaded_tex_color;
    vec3 color = tex_color.rgb;


    vec4 metallic_roughness = texture(tex_samplers[nonuniformEXT (mat.metal_rough_tex_i)], metal_rough_uv);
    float metallic = mat.metal_factor * metallic_roughness.b;
    float roughness = mat.rough_factor * metallic_roughness.g;

    //    out_color = vec4(metallic, metallic, metallic, 1);
    //    return;

    vec3 halfway = normalize(view_dir + light_dir);

    float specular_brdf_denom = 4 * n_dot_l * n_dot_v + 0.0001;

    float specular_distribution = ggx_distribution(normal, halfway, roughness);
    float specular_masking = g2_smith(normal, view_dir, light_dir, roughness);
    vec3 specular_reflection = fresnel_schlick(halfway, view_dir, color, metallic);
    vec3 specular_brdf = (specular_distribution * specular_reflection * specular_masking) / specular_brdf_denom;

    // fractional specular and diffuse components
    vec3 k_s = specular_reflection;
    vec3 k_d = vec3(1.0) - k_s;

    vec3 diffuse_brdf = mix(color, vec3(0.0), metallic);
    diffuse_brdf = k_d * diffuse_brdf * (1.0 / PI);






    vec3 out_radiance = (diffuse_brdf + (specular_brdf * metallic)) * radiance * n_dot_l;
    vec3 ambient = vec3(0.2) * color;
    color = out_radiance + ambient;

    // hard shadow calculation
    rayQueryEXT rq;
    float infinity = 1.0 / 0;
    rayQueryInitializeEXT(rq, accel_struct, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vert_pos.xyz, 0.13, light_dir, infinity);
    rayQueryProceedEXT(rq);
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        // in shadow
        color = color * vec3(0.5);

    }
    out_color = vec4(color, pow(tex_color.a, 1));

    ivec2 coord = ivec2(gl_FragCoord.xy);
    uvec4 img_elem = imageLoad(entity_id_img, coord);
    uint z_int = uint(round(gl_FragCoord.z * 65535.0));

    if (z_int > img_elem.y) {
        // store new value in id buffer if the current fragments distance to camera is closer than the last
        imageStore(entity_id_img, coord, ivec4(constants.entity_id, z_int, 0, 0));
    }
}
