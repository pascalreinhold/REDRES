#version 460

layout (constant_id = 0) const uint POINT_LIGHT_COUNT = 1;
layout (constant_id = 1) const uint MOUSE_BUCKET_COUNT = 1;

layout (location = 0) in vec3 inPosition;   // in World Space
layout (location = 1) in vec3 inNormal; // in World Space
layout (location = 2) in vec3 inColor;
layout (location = 3) in flat uint inID;
layout (location = 4) in flat uint batchID;

layout (location = 0) out vec4 outColor;




#include "buffers.vert"
#include "utils.frag"

void main()
{
    vec3 linear_out_color = BlinnPhongIso(
                                           scene_ubo.ambient_light, scene_ubo.point_lights,
                                           inPosition, inNormal, inColor,
                                           scene_ubo.params[batchID][1],
                                           scene_ubo.params[batchID][2],
                                           scene_ubo.params[batchID][3],
                                           vec3(cam_ubo.camera_positionW),
                                           vec3(cam_ubo.direction_of_light)
    );
    outColor = vec4(CorrectGamma(linear_out_color, scene_ubo.params[batchID][0]), 1.f);

    uint bucketIndex = uint((gl_FragCoord.z) * MOUSE_BUCKET_COUNT);
    if(length(scene_ubo.mouse_coords - gl_FragCoord.xy) < 1){
        buckets.arr[bucketIndex] = inID;
    }
}


