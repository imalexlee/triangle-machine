#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view_proj;
    vec3 eye_pos;
} scene_data;

layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 color_factors;
    float metallic_factor;
    float roughness_factor;
    int color_tex_i;
    int metal_rough_tex_i;
} mat_data;

layout(set = 1, binding = 1) uniform sampler2D color_tex;

layout(set = 1, binding = 2) uniform sampler2D metal_rough_tex;

struct  Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

struct Vertex2 {
    vec4 pos;
    vec4 norm;
    vec2[2] tex_coord;
};

layout(std430, buffer_reference) readonly buffer VertexBuffer2{
    Vertex2 vertices[];
};

layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 2, binding = 0) uniform DrawObjData2 {
    mat4 local_transform;
    VertexBuffer2 vertex_buffer;
} obj_data_2;


layout(set = 2, binding = 0) uniform DrawObjData {
    mat4 local_transform;
    VertexBuffer vertex_buffer;
} obj_data;
