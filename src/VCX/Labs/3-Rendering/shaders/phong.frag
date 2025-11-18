#version 410 core

layout(location = 0) in  vec3 v_Position;
layout(location = 1) in  vec3 v_Normal;
layout(location = 2) in  vec2 v_TexCoord;

layout(location = 0) out vec4 f_Color;

struct Light {
    vec3  Intensity;
    vec3  Direction;   // For spot and directional lights.
    vec3  Position;    // For point and spot lights.
    float CutOff;      // For spot lights.
    float OuterCutOff; // For spot lights.
};

layout(std140) uniform PassConstants {
    mat4  u_Projection;
    mat4  u_View;
    vec3  u_ViewPosition;
    vec3  u_AmbientIntensity;
    Light u_Lights[4];
    int   u_CntPointLights;
    int   u_CntSpotLights;
    int   u_CntDirectionalLights;
};

uniform float u_AmbientScale;
uniform bool  u_UseBlinn;
uniform float u_Shininess;
uniform bool  u_UseGammaCorrection;
uniform int   u_AttenuationOrder;
uniform float u_BumpMappingBlend;

uniform sampler2D u_DiffuseMap;
uniform sampler2D u_SpecularMap;
uniform sampler2D u_HeightMap;

vec3 Shade(vec3 lightIntensity, vec3 lightDir, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor, float shininess) {
    // your code here:
    float diff_dir=max(dot(normal,lightDir),0);
    if(u_UseBlinn){
        vec3 h=normalize(lightDir+viewDir);
        float spec=pow(max(dot(normal,h),0),shininess);
        return lightIntensity*(diffuseColor*diff_dir+specularColor*spec);
    }
    else{
    vec3 spe_res=reflect(-lightDir,normal);
    float  cos=dot(spe_res,viewDir);
    return lightIntensity*(diffuseColor*diff_dir+specularColor*pow(max(cos,0),shininess));}
}

vec3 GetNormal() {
    // Bump mapping from paper: Bump Mapping Unparametrized Surfaces on the GPU
    vec3 vn = normalize(v_Normal);

    // your code here:
    vec3 bumpNormal = vn;
    vec2 texCoord = v_TexCoord;
    ivec2 heightMapSize = textureSize(u_HeightMap, 0); 
    vec2 step = 1.0 / vec2(heightMapSize); 
    float heightScale = 15.0; 
    // 当前点的高度
    float hCenter = texture(u_HeightMap, texCoord).r;
    // 右邻点（x方向+step.x）的高度
    float hRight = texture(u_HeightMap, texCoord + vec2(step.x, 0.0)).r;
    // 上邻点（y方向+step.y）的高度
    float hUp = texture(u_HeightMap, texCoord + vec2(0.0, step.y)).r;
    // 计算x方向的高度变化
    float dHdx = (hRight - hCenter) * heightScale;
    // 计算y方向的高度变化
    float dHdy = (hUp - hCenter) * heightScale;

    // 切线空间的法线：垂直于高度变化的方向（梯度）
    // 高度变化的梯度是(dHdx, dHdy, 1)
    vec3 tangentNormal = vec3(-dHdx, -dHdy, 1.0);
    tangentNormal = normalize(tangentNormal); // 归一化

    // 用屏幕空间偏导数计算切线T和副切线B
    vec3 dPdx = dFdx(v_Position);
    vec3 dPdy = dFdy(v_Position); 
    // 切线T：与原始法线垂直
    vec3 T = normalize(dPdx - vn * dot(vn, dPdx));
    // 副切线B：由法线和切线叉乘得到
    vec3 B = normalize(cross(vn, T));
    // TBN矩阵：将切线空间法线转换到世界空间
    bumpNormal = T * tangentNormal.x + B * tangentNormal.y + vn * tangentNormal.z;
    bumpNormal = normalize(bumpNormal); // 归一化
    return bumpNormal != bumpNormal ? vn : normalize(vn * (1. - u_BumpMappingBlend) + bumpNormal * u_BumpMappingBlend);
}

void main() {
    float gamma          = 2.2;
    vec4  diffuseFactor  = texture(u_DiffuseMap , v_TexCoord).rgba;
    vec4  specularFactor = texture(u_SpecularMap, v_TexCoord).rgba;
    if (diffuseFactor.a < .2) discard;
    vec3  diffuseColor   = u_UseGammaCorrection ? pow(diffuseFactor.rgb, vec3(gamma)) : diffuseFactor.rgb;
    vec3  specularColor  = specularFactor.rgb;
    float shininess      = u_Shininess < 0 ? specularFactor.a * 256 : u_Shininess;
    vec3  normal         = GetNormal();
    vec3  viewDir        = normalize(u_ViewPosition - v_Position);
    // Ambient component.
    vec3  total = u_AmbientIntensity * u_AmbientScale * diffuseColor;
    // Iterate lights.
    for (int i = 0; i < u_CntPointLights; i++) {
        vec3  lightDir     = normalize(u_Lights[i].Position - v_Position);
        float dist         = length(u_Lights[i].Position - v_Position);
        float attenuation  = 1. / (u_AttenuationOrder == 2 ? dist * dist : (u_AttenuationOrder == 1  ? dist : 1.));
        total             += Shade(u_Lights[i].Intensity, lightDir, normal, viewDir, diffuseColor, specularColor, shininess) * attenuation;
    }
    for (int i = u_CntPointLights + u_CntSpotLights; i < u_CntPointLights + u_CntSpotLights + u_CntDirectionalLights; i++) {
        total += Shade(u_Lights[i].Intensity, u_Lights[i].Direction, normal, viewDir, diffuseColor, specularColor, shininess);
    }
    // Gamma correction.
    f_Color = vec4(u_UseGammaCorrection ? pow(total, vec3(1. / gamma)) : total, 1.);
}
