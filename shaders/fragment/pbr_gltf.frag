#version 460

// PBR based on gltf 2.0 spec

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"

layout (location = 0) out vec4 out_color;

layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec2 metal_rough_uv;
layout (location = 3) in vec2 normal_uv;
layout (location = 4) in vec2 occlusion_uv;
layout (location = 5) in vec2 emissive_uv;
layout (location = 6) in vec2 clearcoat_uv;
layout (location = 7) in vec2 clearcoat_rough_uv;
layout (location = 8) in vec2 clearcoat_normal_uv;
layout (location = 9) in vec2 specular_strength_uv;
layout (location = 10) in vec2 specular_color_uv;
layout (location = 11) in vec3 surface_normal;
layout (location = 12) in vec4 vert_pos;
layout (location = 13) in vec4 tangent;


float PI = 3.1415926535897932384626433832795;

float g1_schlick(vec3 normal, vec3 incident, float roughness) {
    float alignment = max(dot(normal, incident), 0.000001);
    float roughness_2 = pow(roughness, 2);
    float denom = alignment + sqrt(roughness_2 + (1.0 - roughness_2) * pow(alignment, 2));
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
    roughness_2 = max(roughness_2, 0.00002);

    float alignment = max(dot(macro_normal, micro_normal), 0.01);
    float numerator = roughness_2;
    float denominator = PI * pow(pow(alignment, 2) * (roughness_2 - 1) + 1, 2);
    return numerator / denominator;
}

float max_value(vec3 color) {
    float rg_max = max(color.r, color.g);
    return max(rg_max, color.b);
}

vec3 fresnel_mix(vec3 normal, vec3 incident, vec3 f_0, vec3 base, vec3 layer, float weight) {
    // f_0 = vec3(0.04);

    f_0 = min(0.04 * f_0, vec3(1.0));
    float alignment = max(dot(normal, incident), 0.0);
    vec3 fr = f_0 + (1 - f_0) * pow(1 - alignment, 5);
    // return mix(base, layer, fr);
    return (1 - weight * max_value(fr)) * base + weight * fr * layer;
}

vec3 fresnel_conductor(vec3 normal, vec3 incident, vec3 f_0, vec3 bsdf) {
    float alignment = max(dot(normal, incident), 0.0);
    return bsdf * (f_0 + (1 - f_0) * pow(1 - alignment, 5));
}

vec3 fresnel_coat(vec3 normal, vec3 incident, vec3 f_0, vec3 base, vec3 layer, float weight) {
    float alignment = max(dot(normal, incident), 0.0);
    vec3 fr = f_0 + (1 - f_0) * pow(1 - alignment, 5);
    return mix(base, layer, weight * fr);
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

vec3 specular_brdf(vec3 normal, vec3 halfway, vec3 view, vec3 light, float roughness) {
    float n_dot_l = max(dot(normal, light), 0.001);
    float n_dot_v = max(dot(normal, view), 0.0);

    // add offset to avoid divide by 0
    float visibility_denominator = 4 * n_dot_l * n_dot_v + 0.0001;
    float visibility_numerator = g2_smith(normal, view, light, roughness);
    float visibility = visibility_numerator / visibility_denominator;

    // faster impl
    // https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/63b7c128266cfd86bbd3f25caf8b3db3fe854015/source/Renderer/shaders/brdf.glsl
/**
    float alpharoughnesssq = roughness * roughness;

    float ggxv = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - alpharoughnesssq) + alpharoughnesssq);
    float ggxl = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - alpharoughnesssq) + alpharoughnesssq);

    float visibility;
    float ggx = ggxv + ggxl;
    if (ggx > 0.0)
    {
        visibility = 0.5 / ggx;
    } else {

        visibility = 0.0;
    }

*/
    float distribution = ggx_distribution(normal, halfway, roughness);
    return vec3(visibility * distribution);
}

// TONE MAPPING ALGORITHMS
vec3 toneMap_KhronosPbrNeutral(vec3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1. - startCompression;
    float newPeak = 1. - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
    return mix(color, newPeak * vec3(1, 1, 1), g);
}

vec3 RRTAndODTFit(vec3 color)
{
    vec3 a = color * (color + 0.0245786) - 0.000090537;
    vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    return a / b;
}
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.f, 1.f);
}
void main() {
    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];

    vec3 view_dir = normalize(scene_data.eye_pos.xyz - vert_pos.xyz);
    vec3 normal = normalize(surface_normal);

    float loaded_occlusion_val = texture(tex_samplers[nonuniformEXT (mat.occlusion_tex_i)], occlusion_uv).r;
    float occlusion = 1.0 + mat.occlusion_strength * (loaded_occlusion_val - 1.0);

    vec3 loaded_emissive_val = texture(tex_samplers[nonuniformEXT (mat.emissive_tex_i)], emissive_uv).rgb;
    vec3 emissive = loaded_emissive_val * mat.emissive_factors.rgb;

    float loaded_specular_strength_val = texture(tex_samplers[nonuniformEXT (mat.specular_strength_tex_i)], specular_strength_uv).a;
    vec3 loaded_specular_color_val = texture(tex_samplers[nonuniformEXT (mat.specular_color_tex_i)], specular_color_uv).rgb;
    //    loaded_specular_color_val = pow(loaded_specular_color_val, vec3(2.2));

    float specular_strength = loaded_specular_strength_val * mat.specular_strength;
    vec3 specular_color = loaded_specular_color_val * mat.specular_color_factors.rgb;

    //    vec4 loaded_tex_color = pow(texture(tex_samplers[nonuniformEXT (mat.color_tex_i)], color_uv), vec4(2.2, 2.2, 2.2, 1));
    vec4 loaded_tex_color = texture(tex_samplers[nonuniformEXT (mat.color_tex_i)], color_uv);
    vec4 tex_color = vec4(mat.color_factors.rgba) * loaded_tex_color;
    vec3 color = tex_color.rgb * occlusion;

    //    out_color = vec4(emissive, tex_color.a);
    //    return;
    //    out_color = vec4(mat.color_factors.a, mat.color_factors.a, mat.color_factors.a, 1);
    //    return;

    // Bump Mapping
    vec3 bump_tex_val = texture(tex_samplers[nonuniformEXT(mat.normal_tex_i)], normal_uv).xyz;

    // if not the default texture (aka there is a bump value associated with this vertex)
    if (bump_tex_val != vec3(1, 1, 1)) {
        // convert to [-1, 1] range
        vec3 bi_tangent = normalize(cross(normal, tangent.xyz) * tangent.w);
        if (tangent.xyz == vec3(0)) {
            normal = perturb_normal(normal, view_dir, bump_tex_val, normal_uv);
        } else {
            mat3 TBN = mat3(tangent.xyz, bi_tangent, normal);
            bump_tex_val = bump_tex_val * 2.0 - vec3(1.0);
            normal = normalize(TBN * bump_tex_val);
        }
    }

    //    out_color = vec4(normal, tex_color.a);
    //    return;

    vec3 light_color = vec3(1);
    mat4 model = constants.global_transform * constants.local_transform;
    vec3 light_pos = vec3(2, 3, 0);
    //vec3 light_pos = (model * vec4(10, 10, 0, 0)).xyz;
    vec3 light_dir = normalize(light_pos);
    float light_distance = length(light_pos - vert_pos.xyz);
    float attenuation = 1.0 / pow(light_distance, 2.0);
    float light_intensity = 5;

    vec3 halfway = normalize(view_dir + light_dir);

    float n_dot_l = max(dot(normal, light_dir), 0.0);
    float n_dot_v = max(dot(normal, view_dir), 0.0);
    float n_dot_h = max(dot(normal, halfway), 0.0);
    float l_dot_v = max(dot(light_dir, view_dir), 0.0);

    vec3 in_radiance = light_color * light_intensity;

    vec4 metallic_roughness = texture(tex_samplers[nonuniformEXT (mat.metal_rough_tex_i)], metal_rough_uv);
    float metallic = mat.metal_factor * metallic_roughness.b;
    float roughness = pow(mat.rough_factor * metallic_roughness.g, 2);

    vec3 albedo = mix(color, vec3(0, 0, 0), metallic);

    vec3 base_specular_brdf = specular_brdf(normal, halfway, view_dir, light_dir, roughness);

    // METAL BRDF
    vec3 metal_brdf = fresnel_conductor(halfway, view_dir, color, base_specular_brdf);

    // DIELECTRIC BRDF

    // lambertian diffuse
    vec3 diffuse_brdf = color * (1 / PI);

    vec3 dielectric_brdf = fresnel_mix(halfway, view_dir, specular_color.rgb, diffuse_brdf, base_specular_brdf, specular_strength);


    vec3 material = mix(dielectric_brdf, metal_brdf, metallic);


    // clearcoat
    float clearcoat_val = texture(tex_samplers[nonuniformEXT (mat.clearcoat_tex_i)], clearcoat_uv).r;
    float clearcoat = mat.clearcoat_factor * clearcoat_val;

    float clearcoat_rough_val = texture(tex_samplers[nonuniformEXT (mat.clearcoat_rough_tex_i)], clearcoat_rough_uv).g;
    float clearcoat_roughness = pow(mat.clearcoat_rough_factor * clearcoat_rough_val, 2);

    vec3 clearcoat_normal_val = texture(tex_samplers[nonuniformEXT (mat.clearcoat_normal_tex_i)], clearcoat_normal_uv).xyz;
    vec3 clearcoat_normal = normal;


    // check that the surface has a clearcoat
    if (clearcoat != 0.0) {
        //        out_color = vec4(0);
        //        return;
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

        vec3 clearcoat_brdf = specular_brdf(clearcoat_normal, halfway, view_dir, light_dir, clearcoat_roughness);


        material = fresnel_coat(clearcoat_normal, view_dir, vec3(0.04), material, clearcoat_brdf, clearcoat);
    }
    material = (PI * material) * in_radiance * n_dot_l;


    vec3 ambient = vec3(0.03) * color;
    color = material + ambient + emissive;

    // hard shadow calculation
    rayQueryEXT rq;
    float infinity = 1.0 / 0;
    rayQueryInitializeEXT(rq, accel_struct, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vert_pos.xyz, 0.13, light_dir, infinity);
    rayQueryProceedEXT(rq);
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        // in shadow
        //        color = color * vec3(0.5);

    }

    //    color = toneMap_KhronosPbrNeutral(color);
    //    color = RRTAndODTFit(color);


    color = ACESFilm(color);
    //    tone mapping
    //    color = color / (color + vec3(1.0));
    //            color = pow(color, vec3(1 / 2.2));
    out_color = vec4(color, tex_color.a);

    ivec2 coord = ivec2(gl_FragCoord.xy);
    uvec4 img_elem = imageLoad(entity_id_img, coord);
    uint z_int = uint(round(gl_FragCoord.z * 65535.0));

    if (z_int > img_elem.y) {
        // store new value in id buffer if the current fragments distance to camera is closer than the last
        imageStore(entity_id_img, coord, ivec4(constants.entity_id, z_int, 0, 0));
    }
}
