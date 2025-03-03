#extension GL_EXT_buffer_reference: require
#extension GL_EXT_ray_tracing: enable
#extension GL_EXT_ray_query: enable

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec4 eye_pos;
} scene_data;

layout (set = 0, binding = 1, rg16ui) uniform uimage2D entity_id_img;

struct PBR_Material {
    vec4 color_factors;
    vec4 emissive_factors;

    vec4 specular_color_factors;

    float metal_factor;
    float rough_factor;
    float occlusion_strength;

    float clearcoat_factor;
    float clearcoat_rough_factor;
    float specular_strength;

    uint color_tex_i;
    uint color_tex_coord;
    uint metal_rough_tex_i;
    uint metal_rough_tex_coord;
    uint normal_tex_i;
    uint normal_tex_coord;
    uint occlusion_tex_i;
    uint occlusion_tex_coord;
    uint emissive_tex_i;
    uint emissive_tex_coord;

    uint clearcoat_tex_i;
    uint clearcoat_tex_coord;
    uint clearcoat_rough_tex_i;
    uint clearcoat_rough_tex_coord;
    uint clearcoat_normal_tex_i;
    uint clearcoat_normal_tex_coord;

    uint specular_strength_tex_i;
    uint specular_strength_tex_coord;
    uint specular_color_tex_i;
    uint specular_color_tex_coord;

    uint padding[2];
};

layout (std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    PBR_Material materials[];
} material_buf;

layout (set = 1, binding = 1) uniform accelerationStructureEXT accel_struct;

layout (set = 1, binding = 2) uniform samplerCube sky_box_tex;

layout (set = 1, binding = 3) uniform sampler2D tex_samplers[];

struct Vertex {
    vec4 pos;
    vec4 norm;
    vec4 tangent;
    vec2 tex_coord[2];
};

layout (std430, buffer_reference) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout (push_constant) uniform PushConstants {
    mat4 global_transform;
    mat4 local_transform;
    VertexBuffer vertex_buffer;
    uint material_i;
    uint entity_id;
} constants;



