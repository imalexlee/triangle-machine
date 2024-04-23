
#version 450
layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 surface_normal;
layout(location = 3) in vec3 eye_pos;
layout(location = 4) in vec3 vert_pos;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D color_texture;

void main() {
    vec4 c_surface = texture(color_texture, uv);
    vec3 c_cool = vec3(0, 0, 0.55) + 0.20 * c_surface.xyz;
    vec3 c_warm = vec3(0.3, 0.3, 0) + 0.6 * c_surface.xyz;
    vec3 c_highlight = vec3(1, 1, 1);
    vec3 light_dir = normalize(vec3(0, 1, 0.4));
    float nl = dot(surface_normal, light_dir);
    float t = (nl + 1) / 2;
    vec3 reflected = 2 * nl * surface_normal - light_dir;
    //reflect(light_dir, surface_normal);
    vec3 view_dir = normalize(eye_pos - vert_pos);
    float s = clamp(100 * dot(reflected, view_dir) - 97, 0, 1);

    out_color = vec4(s * c_highlight + (1 - s) * (t * c_warm + (1 - t) * c_cool), 1);
}
