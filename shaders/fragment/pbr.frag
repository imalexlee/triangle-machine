#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require
#include "../input_structures.glsl"

layout (location = 0) out vec4 out_color;
//layout (location = 1) out int entity_id;

layout (location = 1) in vec2 color_uv;
layout (location = 2) in vec2 metal_rough_uv;
layout (location = 3) in vec2 normal_uv;
layout (location = 4) in vec3 surface_normal;
layout (location = 5) in vec4 vert_pos;

float PI = 3.1415926535897932384626433832795;

float distribution_GGX(vec3 normal, vec3 Halfway, float roughness)
{
    float alpha = roughness * roughness;
    float alpha_2 = alpha * alpha;
    float n_dot_h = max(dot(normal, Halfway), 0.1);
    float n_dot_h_2 = n_dot_h * n_dot_h;

    float denom = n_dot_h_2 * (alpha_2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return alpha_2 / denom;
}

float geometry_schlick_GGX(float n_dot_v, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = n_dot_v * (1.0 - k) + k;

    return n_dot_v / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float n_dot_v = max(dot(N, V), 0.0);
    float n_dot_l = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_GGX(n_dot_v, roughness);
    float ggx1 = geometry_schlick_GGX(n_dot_l, roughness);

    return ggx1 * ggx2;
}

vec3 fresnel_schlick(float v_dot_h, vec3 albedo, float metallic)
{
    vec3 f_0 = mix(vec3(0.04), albedo, metallic);

    return f_0 + (1.0 - f_0) * pow(clamp(1.0 - v_dot_h, 0.0, 1.0), 5.0);
}

mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv)
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    // solve the linear system
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));

    // calculate handedness of the resulting cotangent frame
    float w = (dot(cross(N, T), B) < 0.0) ? -1.0 : 1.0;

    // adjust tangent if needed
    T = T * w;

    return mat3(T * invmax, B * invmax, N);
}

vec3 perturbNormal(vec3 n, vec3 v, vec3 normalSample, vec2 uv)
{
    vec3 map = normalize(2.0 * normalSample - vec3(1.0));
    mat3 TBN = cotangentFrame(n, v, uv);
    return normalize(TBN * map);
}

void main() {
    PBR_Material mat = material_buf.materials[nonuniformEXT(constants.material_i)];

    vec3 view_dir = normalize(scene_data.eye_pos.xyz - vert_pos.xyz);

    vec4 bump_tex_val = texture(tex_samplers[nonuniformEXT(mat.normal_tex_i)], normal_uv);
    vec3 normal = normalize(surface_normal);
    normal = perturbNormal(normal, view_dir, bump_tex_val.xyz, normal_uv);


    vec4 tex_color = mat.color_factors * texture(tex_samplers[nonuniformEXT(mat.color_tex_i)], color_uv);
    //tex_color = pow(tex_color, vec4(1 / 2.2));
    vec3 color = tex_color.rgb;


    vec4 metallic_roughness = texture(tex_samplers[nonuniformEXT (mat.metal_rough_tex_i)], metal_rough_uv);
    float metallic = mat.metal_factor * metallic_roughness.b;
    float roughness = mat.rough_factor * metallic_roughness.g;

    vec3 light_dir = normalize(vec3(1, 1, 0.5));
    vec3 light_color = vec3(23.47, 21.31, 20.79);
    vec3 light_diffuse_intensity = vec3(0.8);

    vec3 light_intensity = light_color * light_diffuse_intensity;

    vec3 l = light_dir;
    vec3 halfway = normalize(view_dir + l);

    float v_dot_h = max(dot(view_dir, halfway), 0.0);
    float n_dot_v = max(dot(normal, view_dir), 0.0);
    float n_dot_l = max(dot(normal, l), 0.3);

    vec3 f = fresnel_schlick(v_dot_h, color.rgb, metallic);
    vec3 k_s = f;
    vec3 k_d = vec3(1.0) - k_s;

    float d = distribution_GGX(normal, halfway, roughness);
    float g = geometry_smith(normal, view_dir, l, roughness);

    vec3 specular_nom = g * f * d;
    float specular_denom = 4.0 * n_dot_v * n_dot_l + 0.0001;
    vec3 specular_brdf = specular_nom / specular_denom;

    vec3 lambert = mix(color.rgb, color.rgb * vec3(0.0001), metallic);

    vec3 diffuse_brdf = k_d * color.rgb / PI;

    vec3 final_color = (diffuse_brdf + specular_brdf) * light_intensity * n_dot_l;

    out_color = vec4(final_color, tex_color.a);

    rayQueryEXT rq;
    float infinity = 1.0 / 0;
    rayQueryInitializeEXT(rq, accel_struct, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vert_pos.xyz, 0.13, light_dir, infinity);

    rayQueryProceedEXT(rq);

    // 1.0 for occluded (in shadow) and 0.0 for not occluded
    float occlued = 0.0;
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        // in shadow
        occlued = 1.0;
        out_color = vec4(final_color.rgb * vec3(0.2), tex_color.a);

    }

    ivec2 coord = ivec2(gl_FragCoord.xy);
    uvec4 img_elem = imageLoad(entity_id_img, coord);
    uint z_int = uint(round(gl_FragCoord.z * 65535.0));

    if (z_int > img_elem.y) {
        // store new value in id buffer if the current fragments distance to camera is closer than the last
        imageStore(entity_id_img, coord, ivec4(constants.entity_id, z_int, 0, 0));
    }
}
