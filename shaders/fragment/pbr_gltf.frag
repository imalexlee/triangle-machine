#version 460

// PBR based on gltf 2.0 spec

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"

layout (location = 0) out vec4 out_color;

layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec2 metal_rough_uv;
layout (location = 3) in vec2 normal_uv;
layout (location = 4) in vec2 clearcoat_uv;
layout (location = 5) in vec2 clearcoat_rough_uv;
layout (location = 6) in vec2 clearcoat_normal_uv;
layout (location = 7) in vec3 surface_normal;
layout (location = 8) in vec4 vert_pos;
layout (location = 9) in vec4 tangent;

float PI = 3.1415926535897932384626433832795;

float g1_schlick(vec3 normal, vec3 incident, float roughness) {
    float alignment = abs(dot(normal, incident));
    float roughness_2 = pow(roughness, 2);
    float denom = alignment + sqrt(roughness_2 + (1 - roughness_2) * pow(alignment, 2));
    return (2.0 * alignment) / denom;
}

float g2_smith(vec3 normal, vec3 view_dir, vec3 light_dir, float roughness) {
    // Disney roughness remap to reduce "hotness"
    //roughness = (roughness + 1.0) / 2;
    //float k = pow(roughness + 1.0, 2.0) * 0.125; // divide by 8
    float g1_light = g1_schlick(normal, light_dir, roughness);
    float g1_view = g1_schlick(normal, view_dir, roughness);
    return g1_light * g1_view;
}

float ggx_distribution(vec3 macro_normal, vec3 micro_normal, float roughness) {
    float roughness_2 = pow(roughness, 2);

    float alignment = abs(dot(macro_normal, micro_normal));
    float numerator = roughness_2;
    float denominator = PI * pow(pow(alignment, 2) * (roughness_2 - 1) + 1, 2);
    return numerator / denominator;
}

vec3 fresnel_schlick(vec3 normal, vec3 incident, vec3 albedo, float metallic)
{
    float alignment = max(dot(normal, incident), 0.000);
    vec3 f_0 = mix(vec3(0.04), albedo, metallic);
    return f_0 + (1.0 - f_0) * pow(clamp(1.0 - alignment, 0.0, 1.0), 5.0);
}

mat3 generate_cotangent_frame(vec3 normal, vec3 p, vec2 uv)
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    // solve the linear system
    vec3 dp2perp = cross(dp2, normal);
    vec3 dp1perp = cross(normal, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    // construct a scale-invariant frame
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    // calculate handedness of the resulting cotangent frame
    float w = (dot(cross(normal, T), B) < -1.0) ? -1.0 : 1.0;
    // adjust tangent if needed
    T = T * w;
    return mat3(T * invmax, B * invmax, normal);
}

vec3 perturb_normal(vec3 normal, vec3 view, vec3 normal_sample, vec2 uv)
{
    vec3 map = normalize(2.0 * normal_sample - vec3(1.0));
    mat3 TBN = generate_cotangent_frame(normal, view, uv);
    return normalize(TBN * map);
}

void main() {
    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];

    vec3 view_dir = normalize(scene_data.eye_pos.xyz - vert_pos.xyz);
    vec3 normal = normalize(surface_normal);

    vec4 loaded_tex_color = texture(tex_samplers[nonuniformEXT (mat.color_tex_i)], color_uv);
    vec4 tex_color = mat.color_factors * loaded_tex_color;
    vec3 color = tex_color.rgb;

    // Bump Mapping
    vec3 bump_tex_val = texture(tex_samplers[nonuniformEXT(mat.normal_tex_i)], normal_uv).xyz;

    // if not the default texture (aka there is a bump value associated with this vertex)
    if (bump_tex_val != vec3(1, 1, 1)) {
        // convert to [-1, 1] range
        vec3 bi_tangent = normalize(cross(normal, tangent.xyz) * tangent.w);

        mat3 TBN = mat3(tangent.xyz, bi_tangent, normal);
        bump_tex_val = bump_tex_val * 2.0 - vec3(1.0);
        normal = normalize(TBN * bump_tex_val);

    }


    vec3 light_color = normalize(vec3(23.47, 21.31, 20.79));
    vec3 light_pos = vec3(10, 10, 5);
    vec3 light_dir = normalize(light_pos);
    float light_distance = length(light_pos - vert_pos.xyz);
    float attenuation = 1.0 / pow(light_distance, 1.0);
    float light_intensity = 15;

    vec3 halfway = normalize(view_dir + light_dir);

    float n_dot_l = abs(dot(normal, light_dir));
    float n_dot_v = abs(dot(normal, view_dir));
    float n_dot_h = abs(dot(normal, halfway));
    float l_dot_v = abs(dot(light_dir, view_dir));

    vec3 in_radiance = light_color * light_intensity;



    vec4 metallic_roughness = texture(tex_samplers[nonuniformEXT (mat.metal_rough_tex_i)], metal_rough_uv);
    float metallic = mat.metal_factor * metallic_roughness.b;
    float roughness = pow(mat.rough_factor * metallic_roughness.g, 2);

    vec3 albedo = mix(color, vec3(0, 0, 0), metallic);

    // METAL BRDF

    // add offset to avoid divide by 0
    float metal_visibility_denominator = 4 * n_dot_l * n_dot_v + 0.0001;
    float metal_visibility_numerator = g2_smith(normal, view_dir, light_dir, roughness);
    float metal_visibility = metal_visibility_numerator / metal_visibility_denominator;
    float metal_distribution = ggx_distribution(normal, halfway, roughness);
    float metal_specular_brdf = metal_visibility * metal_distribution;
    vec3 metal_fresnel = fresnel_schlick(halfway, view_dir, albedo, metallic);

    vec3 metal_brdf = metal_specular_brdf * metal_fresnel;

    // DIFFUSE BRDF

    // lambertian
    vec3 diffuse_brdf = albedo * (1 / PI);


    //vec3 out_radiance = PI * mix(diffuse_brdf, specular_brdf, metallic) * radiance * n_dot_l;
    vec3 material = mix(diffuse_brdf, metal_brdf, metallic);
    //vec3 material = metal_brdf + diffuse_brdf;

    // clearcoat
    float clearcoat_val = texture(tex_samplers[nonuniformEXT (mat.clearcoat_tex_i)], clearcoat_uv).r;
    float clearcoat = mat.clearcoat_factor * clearcoat_val;

    float clearcoat_rough_val = texture(tex_samplers[nonuniformEXT (mat.clearcoat_rough_tex_i)], clearcoat_rough_uv).g;
    float clearcoat_roughness = pow(mat.clearcoat_rough_factor * clearcoat_rough_val, 2);

    vec3 clearcoat_normal_val = texture(tex_samplers[nonuniformEXT (mat.clearcoat_normal_tex_i)], clearcoat_normal_uv).xyz;
    vec3 clearcoat_normal = normal;


    // check that the surface has a clearcoat
    if (clearcoat != 0.0) {
        // normal value of [1,1,1] would mean the default  texture
        if (clearcoat_normal_val != vec3(1, 1, 1)) {
            vec4 tangent_computed = tangent;
            if (tangent_computed.xyz == vec3(0)) {
                // gltf says that authors SHOULD include tangent vectors when using a clearcoat material...
                // not everyone listens. You have to derive it from pixel deltas instead.
                clearcoat_normal = perturb_normal(normal, view_dir, clearcoat_normal_val, clearcoat_normal_uv);
            } else {
                vec3 bi_tangent = normalize(cross(normalize(surface_normal), tangent.xyz) * tangent.w);
                mat3 TBN = mat3(tangent.xyz, bi_tangent, normalize(surface_normal));
                // convert to [-1, 1] range
                clearcoat_normal_val = clearcoat_normal_val * 2.0 - vec3(1.0);
                clearcoat_normal = normalize(TBN * clearcoat_normal_val);
            }
        }

        //        float nc_dot_l = max(dot(clearcoat_normal, light_dir), 0.0);
        //        float nc_dot_v = max(dot(clearcoat_normal, view_dir), 0.0);
        //        float clearcoat_brdf_denom = 4 * nc_dot_l * nc_dot_v + 0.00001;
        //
        //
        //        float clearcoat_distribution = ggx_distribution(clearcoat_normal, halfway, clearcoat_roughness);
        //        float clearcoat_masking = g2_smith_correlated(clearcoat_normal, view_dir, light_dir, clearcoat_roughness);
        //        vec3 clearcoat_reflection = fresnel_schlick(clearcoat_normal, view_dir, albedo, 0.0);
        //        float clearcoat_specular_brdf = (clearcoat_distribution * clearcoat_masking) / clearcoat_brdf_denom;
        //
        //
        //        material = mix(material, vec3(clearcoat_specular_brdf), clearcoat * clearcoat_reflection.r);
    } else {
        //  out_color = vec4(0);
        //        return;
    }
    material = (PI * material) * in_radiance * n_dot_l;


    vec3 ambient = vec3(0.03) * color;
    color = material + ambient;

    // hard shadow calculation
    rayQueryEXT rq;
    float infinity = 1.0 / 0;
    rayQueryInitializeEXT(rq, accel_struct, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vert_pos.xyz, 0.13, light_dir, infinity);
    rayQueryProceedEXT(rq);
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        // in shadow
        // color = color * vec3(0.5);

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
