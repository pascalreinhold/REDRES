#version 450

layout (set = 0, binding = 0) uniform sampler2D position_sampler;
layout (set = 0, binding = 1) uniform sampler2D normal_sampler;
layout (set = 0, binding = 2) uniform sampler2D albedo_sampler;

layout (location = 0) in vec2 uv_in;
layout (location = 0) out vec4 frag_color_out;

void main()
{
    vec3 position = texture(position_sampler, uv_in).xyz;
    vec3 normal = texture(normal_sampler, uv_in).xyz;
    vec3 albedo = texture(albedo_sampler, uv_in).xyz;

    frag_color_out = vec4(0.5);
}