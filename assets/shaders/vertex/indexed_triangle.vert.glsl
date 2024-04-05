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
    mat4 local_transform;
    VertexBuffer vertex_buffer;
} constants;

layout(location = 0) out vec3 fragColor;

void main() {
    Vertex v = constants.vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = constants.local_transform * vec4(v.position, 1.0f);
    fragColor = v.color.xyz;
}
