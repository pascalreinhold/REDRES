#version 460

layout (constant_id = 0) const uint POINT_LIGHT_COUNT = 1;

layout (location = 0) in vec3 inPosition;   // in World Space
layout (location = 1) in vec3 inNormal; // in World Space
layout (location = 2) in flat vec3 inColor1;
layout (location = 3) in flat vec3 inColor2;
layout (location = 4) in flat vec3 inCenter;
layout (location = 5) in flat vec3 inBondNormal;
layout (location = 6) in flat uint batchID;

layout (location = 0) out vec4 outColor;


#include "buffers.vert"
#include "utils.frag"



void main()
{
    bool isLeft = (dot((inPosition - inCenter), inBondNormal)) > 0;
    vec3 bondColor = isLeft ? inColor1 : inColor2; // 36,24ms

    vec3 linear_out_color = BlinnPhongIso(
                                           scene_ubo.ambient_light,
                                           scene_ubo.point_lights,
                                           inPosition, inNormal, bondColor,
                                           scene_ubo.params[batchID][1],
                                           scene_ubo.params[batchID][2],
                                           scene_ubo.params[batchID][3],
                                           vec3(cam_ubo.camera_positionW),
                                           vec3(cam_ubo.direction_of_light));
    outColor = vec4(CorrectGamma(linear_out_color, scene_ubo.params[batchID][0]), 1.f);
}


