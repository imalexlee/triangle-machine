#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_GOOGLE_include_directive: require

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec4 eye_pos;
} scene_data;

const float CURSOR_SIZE = 0.01;

const vec2 positions[4] = vec2[4](
vec2(-0.5, 0.5), // top-left
vec2(0.5, 0.5), // top-right
vec2(-0.5, -0.5), // bottom-left
vec2(0.5, -0.5)  // bottom-right
);

void main() {
    float viewport_height = 2.0 / scene_data.proj[1][1];
    float viewport_width = 2.0 / scene_data.proj[0][0];

    vec2 pixel_size = vec2(CURSOR_SIZE / viewport_width, CURSOR_SIZE / viewport_height);

    vec2 scaled_pos = positions[gl_VertexIndex] * pixel_size;

    gl_Position = vec4(scaled_pos, 0.0, 1.0);
}
