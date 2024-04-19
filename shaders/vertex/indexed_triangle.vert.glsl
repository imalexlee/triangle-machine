#version 450
#extension GL_EXT_buffer_reference : require

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

layout(push_constant) uniform PushConstants {
    // will be used to transform vertex normal
    mat4 local_transform;
    VertexBuffer vertex_buffer;
} constants;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view_proj;
} scene_data;

layout(set = 1, binding = 1) uniform sampler2D color_tex;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 uv;

void main() {
    Vertex v = constants.vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = scene_data.view_proj * constants.local_transform * vec4(v.position, 1.0f);
    frag_color = (constants.local_transform * vec4(v.normal, 0)).xyz;
    uv = vec2(v.uv_x, v.uv_y);
}