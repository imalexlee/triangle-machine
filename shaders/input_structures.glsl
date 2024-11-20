#extension GL_EXT_buffer_reference: require
#extension GL_EXT_ray_tracing: enable
#extension GL_EXT_ray_query: enable

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec4 eye_pos;
} scene_data;

layout (set = 0, binding = 1, r32i) uniform writeonly iimage2D entity_id_img;

struct PBR_Material {
    vec4 color_factors;
    float metal_factor;
    float rough_factor;
    uint color_tex_i;
    uint color_tex_coord;
    uint metal_rough_tex_i;
    uint metal_rough_tex_coord;
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
} constants;



