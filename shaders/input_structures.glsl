layout(set = 0, binding = 0) uniform SceneData {
    mat4 view_proj;
    vec3 eye_pos;
} scene_data;

layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 color_factors;
} material_data;

layout(set = 1, binding = 1) uniform sampler2D color_tex;
