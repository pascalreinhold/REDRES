#define RCC_MESH_COUNT 5

struct PointLight{
    vec4 positionW;
    vec4 color;
};

struct ObjectData{
    mat4 model_matrix;
    vec4 color1;
    vec4 color2;
    vec4 bond_normal;
    float radius;
    uint batchID;
};

struct OffsetData{
    vec4 offsets[27];
};

struct DrawIndexedIndirectCommand{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct CullData{
    mat4 viewMatrix;
    vec4 frustumNormalEquations[6];
    vec4 cylinderCenter;
    vec4 cylinderNormal;
    float cylinderLength;
    float cylinderRadiusSquared;
    uint uniqueObjectCount;
    uint offsetCount;
    bool isCullingEnabled;
    bool cullCylinder;
};

struct Instance{
    uint objectID;
    uint batchID;
};
struct FinalInstance{
    uint objectID;
    uint offsetID;
};

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

#define PI 3.14159265358979323846

vec3 BlinnPhong(
const in vec4 ambient_light, const in PointLight lights[POINT_LIGHT_COUNT],
const in vec3 positionW, in vec3 normalW, const in vec3 color,
const in float shininess, const in float diffuse_coeff, const in float specular_coeff,
const in vec3 camera_positionW){

    vec3 diffuse_light = ambient_light.xyz * ambient_light.w;
    vec3 specular_light = vec3(0.0f);
    normalW = normalize(normalW);
    vec3 view_direction = normalize(camera_positionW.xyz - positionW);

    for (int i = 0; i < POINT_LIGHT_COUNT; i++){
        vec3 direction_to_light = lights[i].positionW.xyz - positionW;
        float attenuation = 1.0; // /(dot(direction_to_light, direction_to_light) + 1);
        //float attenuation = 1.0 / sqrt((dot(direction_to_light, direction_to_light) + 1));
        vec3 intensity = lights[i].color.xyz * (lights[i].color.w * attenuation);
        vec3 normalized_direction_to_light = normalize(direction_to_light);

        //diffusive
        float cosAngIncidence = max(dot(normalW, normalized_direction_to_light), 0);
        diffuse_light += intensity * cosAngIncidence;

        //specular
        vec3 halfAngle = normalize(normalized_direction_to_light + view_direction);
        float blinnTerm = dot(normalW, halfAngle);
        blinnTerm = clamp(blinnTerm, 0, 1);
        blinnTerm = pow(blinnTerm, shininess);
        specular_light += intensity * blinnTerm;
    }
    return vec3(specular_coeff * specular_light * color + diffuse_coeff * diffuse_light * color);
}

vec3 BlinnPhongIso(
const in vec4 ambient_light, const in PointLight lights[POINT_LIGHT_COUNT],
const in vec3 positionW, in vec3 normalW, const in vec3 color,
const in float shininess, const in float diffuse_coeff, const in float specular_coeff,
const in vec3 camera_positionW, in vec3 direction_to_light){
    vec3 diffuse_light = ambient_light.xyz * ambient_light.w;
    vec3 specular_light = vec3(0.0f);
    normalW = normalize(normalW);
    vec3 view_direction = normalize(vec3(camera_positionW) - positionW);

    float attenuation = 1.0; // /(dot(direction_to_light, direction_to_light) + 1);
    //float attenuation = 1.0 / sqrt((dot(direction_to_light, direction_to_light) + 1));
    vec3 intensity = lights[0].color.xyz * (lights[0].color.w * attenuation);
    vec3 normalized_direction_to_light = normalize(direction_to_light);

    //diffusive
    float cosAngIncidence = max(dot(normalW, normalized_direction_to_light), 0);
    diffuse_light += intensity * cosAngIncidence;

    //specular
    vec3 halfAngle = normalize(normalized_direction_to_light + view_direction);
    float blinnTerm = dot(normalW, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1);
    blinnTerm = pow(blinnTerm, shininess);
    specular_light += intensity * blinnTerm;

    return vec3(specular_coeff * specular_light * color + diffuse_coeff * diffuse_light * color);
}

vec3 CorrectGamma(const in vec3 linear_color, const float reciprocal_gamma){
    return pow(linear_color, vec3(reciprocal_gamma));
}