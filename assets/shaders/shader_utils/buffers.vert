#ifndef BUFFERS
#define BUFFERS

#include "structs.vert"
#include "constants.vert"

layout(set = 0, binding = 0) uniform camBuffer{
    mat4 projViewMat;
    mat4 viewMat;
    vec4 camera_positionW;
    vec4 direction_of_light;
} cam_ubo;

layout(set = 0, binding = 1) uniform sceneBuffer{
    vec4 ambient_light;
    vec4 params[RCC_MESH_COUNT];
    vec2 mouse_coords;
    PointLight point_lights[POINT_LIGHT_COUNT];
} scene_ubo;

layout(std430, set = 0, binding = 2) readonly buffer ObjectBuffer{
    ObjectData objects[];
}object_buffer;

layout (set = 0, binding = 3) buffer writeonly mouse_bucket_buffer
{
    uint arr[];
}buckets;

layout(std430, set = 0, binding = 4) readonly buffer FinalInstanceBuffer{
    FinalInstance data[];
}final_instances;

layout(std430, set = 0, binding = 5) readonly buffer OffsetBuffer{
    OffsetData data;
}offsets;


#endif