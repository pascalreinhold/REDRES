#ifndef FRAGMENT_SHADER_UTILS
#define FRAGMENT_SHADER_UTILS

#define PI 3.14159265358979323846
#include "structs.vert"

vec3 BlinnPhong(
    const in vec4 ambient_light, const in PointLight lights[POINT_LIGHT_COUNT],
    const in vec3 positionW, in vec3 normalW, const in vec3 color,
    const in float shininess, const in float diffuse_coeff, const in float specular_coeff,
    const in vec3 camera_positionW){

        vec3 diffuse_light = ambient_light.xyz * ambient_light.w;
        vec3 specular_light = vec3(0.0f);
        normalW = normalize(normalW);
        vec3 view_direction = normalize(vec3(camera_positionW) - positionW);

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



// We will try to mimic the notation in googles filament PBR documentation
// so we have an easy and consistent standard

// D = Normal Distribution Function
// G = Geometric Shadowing
// F = Fresnel Factor

// AoB = dot(A, B)
// AxB = cross(A, B)

// N is the surface normal unit vector
// V is the viewing vector unit vector
// fXX is the specular for angle XX between surface normal and incident vector

float D_GGX(float NoH, float roughness){
    float a = NoH * roughness;
    float b = roughness / (1.0 - NoH * NoH + a * a);
    return b * b * (1.0 / PI);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float roughness){
    float r2 = roughness * roughness;
    float GGXL = NoV * sqrt((-NoL * r2 + NoL) * NoL + r2);
    float GGXV = NoL * sqrt((-NoV * r2 + NoV) * NoV + r2);
    return 0.5 / (GGXV + GGXL);
}

float V_Kelemen(float LoH) {
    return 0.25 / (LoH * LoH);
}

vec3 F_Schlick(float u, vec3 f0, float f90){
    return f0 + (vec3(f90) - f0) * pow(1.0 - u, 5.0);
}

float F_SchlickScalar(float u, float f0, float f90){
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}


float Fd_Lambert(){
    return 1.0 / PI;
}
// Disney Diffuse
float Fd_Burley(float NoV, float NoL, float LoH, float roughness){
    float f90 = 0.5 * 2.0 * roughness * LoH * LoH;
    float lightScatter = F_SchlickScalar(NoL, 1.0, f90);
    float viewScatter = F_SchlickScalar(NoV, 1.0, f90);
    return lightScatter * viewScatter * (1.0 / PI);
}

vec3 BRDF1(in vec3 l, in vec3 n, in vec3 v, in vec3 baseColor){
/*

    // material params
    // BaseColor RGB
    // Metallic [0 - 1]
    float metallic = 0;
    // Roughness [0 - 1]
    float roughness = 0.25 * 0.25;
    // Reflectance [0 - 1]
    float reflectance = 0.2;

    // Emissive RGB + exposure compensation
    // ambient occlusion [0 - 1]

    vec3 diffuseColor = (1.0 - metallic) * baseColor;

    vec3 h = normalize(v+l);

    // copper index of reflection

    vec3 f0 = vec3(0.16 * reflectance * reflectance * (1.0 - metallic)) + baseColor * metallic;

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    float D = D_GGX(NoH, roughness);
    vec3 F = F_Schlick(LoH, f0, 1.0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    float clearCoatRoughness = 0.5;

    float Dc = D_GGX(NoH, clearCoatRoughness);
    vec3 Fc = F_Schlick(LoH, 0.04, 1.0);
    float Vc = V_Kelemen(clearCoatRoughness, LoH);

    vec3 f_rc = (Dc * Vc) * Fc;

    // specular
    vec3 f_r = (D * V) * F;
    // diffuse
    vec3 f_d = diffuseColor * Fd_Burley(NoV, NoL, LoH, roughness);

    // clear coat

    return f_r + f_d;
*/
    return vec3(0.0);
}

vec3 CorrectGamma(const in vec3 linear_color, const float reciprocal_gamma){
    return pow(linear_color, vec3(reciprocal_gamma));
}
#endif