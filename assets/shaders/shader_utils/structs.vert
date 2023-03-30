#ifndef STRUCTS
#define STRUCTS

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


#endif