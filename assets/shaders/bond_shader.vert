#version 460

layout (constant_id = 0) const uint POINT_LIGHT_COUNT = 1;

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec3 vertexColor;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out flat vec3 outColor1;
layout (location = 3) out flat vec3 outColor2;
layout (location = 4) out flat vec3 outCenter;
layout (location = 5) out flat vec3 outBondNormal;
layout (location = 6) out flat uint batchID;

#include "buffers.vert"

void main()
{
    const FinalInstance instance = final_instances.data[gl_InstanceIndex];
    uint objectID = instance.objectID;
    uint offsetID = instance.offsetID;

    mat4 model_matrix = object_buffer.objects[objectID].model_matrix;
    vec4 vertexPositionWorld = model_matrix * vec4(vertexPosition, 1.f) + offsets.data.offsets[offsetID];

    gl_Position = cam_ubo.projViewMat * vertexPositionWorld;
    outPosition = vec3(vertexPositionWorld);
    outNormal = mat3(model_matrix) * vertexNormal;
    outColor1 = vec3(object_buffer.objects[objectID].color1);
    outColor2 = vec3(object_buffer.objects[objectID].color2);
    outCenter = vec3(model_matrix[3] + offsets.data.offsets[offsetID]);
    outBondNormal = vec3(object_buffer.objects[objectID].bond_normal);
    batchID = object_buffer.objects[objectID].batchID;
}