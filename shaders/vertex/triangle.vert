//we will be using glsl version 4.5 syntax
#version 450

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view_proj;
    vec4 eye_pos;
    vec4 dir;
} scene_data;

layout (location = 0) in vec3 vertex;

layout (location = 0) out vec3 dir;

//const array of positions for the triangle
const vec3 positions[3] = vec3[3](
vec3(1.f, 1.f, 0.0f),
vec3(-1.f, 1.f, 0.0f),
vec3(0.f, -1.f, 0.0f)
);

void main()
{
    //output the position of each vertex
    gl_Position = scene_data.view_proj * vec4(positions[gl_VertexIndex], 1.0f);
}
