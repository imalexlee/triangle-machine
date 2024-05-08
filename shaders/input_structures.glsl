#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view_proj;
    vec3 eye_pos;
} scene_data;

layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 color_factors;
    float metallic_factor;
    float roughnes_factor;
} material_data;

layout(set = 1, binding = 1) uniform sampler2D color_tex;

layout(set = 1, binding = 2) uniform sampler2D metal_rough_tex;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 2, binding = 0) uniform DrawObjData {
    mat4 local_transform;
    VertexBuffer vertex_buffer;
} obj_data;
