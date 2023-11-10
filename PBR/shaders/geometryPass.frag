#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec4 worldSpace;
layout(location = 2) in vec3 viewPos;
layout(location = 3) in vec2 uvs;
layout(location = 4) in mat3 TBN;

layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform samplerCube u_EnviornmentRadianceTexture;

layout (set = 0, binding = 2) uniform samplerCube u_EnviornmentIrradianceTexture;

layout (set = 0, binding = 3) uniform sampler2D u_BRDFLUTTexture;

layout(set = 1, binding = 0) uniform Material
{
	vec4 Albedo;
	float Emisivity;
	float Roughness;
	float Metalic;
	int UseNormalMap;
} u_MaterialData;

layout(set = 1, binding = 1) uniform sampler2D AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D NormalMap;
layout(set = 1, binding = 3) uniform sampler2D MetalicRoughnessMap;

struct PBRData
{
	vec3 Albedo;
	float Roughness;
	float Metalic;
	vec3 Normal;
	vec3 CameraPos;
	float NdotV;
} m_Data;

const vec3 Fdielectric = vec3(0.04);

#define M_PI 3.1415926
#define M_EPSILON 0.00001

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float NormalDistributionGGXTR(float cosH, float roughness)	//Trowbridge-Reitz GGX
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = (cosH * cosH) * (a2 - 1.0) + 1.0;
	return a2 / (M_PI * denom * denom);
}

float GeometrySchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

float GeometryAttenuationSchlickGGX(float cosL, float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float denom = NdotV * ( 1.0 - k ) + k;
	return GeometrySchlickG1(cosL, k) * GeometrySchlickG1(NdotV, k);
}

vec3 CalculateLight(vec3 F0, vec3 Position, vec3 Color, float Intensity)
{
	vec3 result = vec3(0.0);
	if(Intensity <= 0.0)
		return vec3(0.0, 0.0, 0.0);
	vec3 L = normalize(Position - worldSpace.xyz);
	vec3 H = normalize(m_Data.CameraPos + L);
	float lightDistance = length(Position - worldSpace.xyz);
	float Falloff = 0.1;
	float Radius = 8.0;
	
	float Attenuation = clamp(1.0 - (lightDistance * lightDistance) / (Radius * Radius), 0.0, 1.0);
	Attenuation *= mix(Attenuation, 1.0, Falloff);
	vec3 Radinace = Color * Intensity * Attenuation;

	float cosL = max(0.0, dot(m_Data.Normal, L));
	float cosH = max(0.0, dot(m_Data.Normal, H));
	vec3 F = FresnelSchlickRoughness(F0, max(0.0, dot(H, m_Data.CameraPos)), m_Data.Roughness);
	float NDF = NormalDistributionGGXTR(cosH, m_Data.Roughness);
	float G = GeometryAttenuationSchlickGGX(cosL, m_Data.NdotV, m_Data.Roughness);

	vec3 KD = (1.0 - F) * (1.0 - m_Data.Metalic);
	vec3 diffuseBRDF = KD * m_Data.Albedo;
	vec3 specularBRDF = (F * NDF * G) / max(M_EPSILON, 4.0 * cosL * m_Data.NdotV);

	specularBRDF = clamp(specularBRDF, vec3(0.0), vec3(10.0));
	result += (diffuseBRDF + specularBRDF) * Radinace * cosL;
	return result;
}

vec3 RotateVectorAboutY(float angle, vec3 vec)
{
	angle = radians(angle);
	mat3x3 rotationMatrix = { vec3(cos(angle),0.0,sin(angle)),
							vec3(0.0,1.0,0.0),
							vec3(-sin(angle),0.0,cos(angle)) };
	return rotationMatrix * vec;
}

vec3 IBL(vec3 F0, vec3 Lr)
{
	vec3 irradiance = texture(u_EnviornmentIrradianceTexture, m_Data.Normal).rgb;
	vec3 F = FresnelSchlickRoughness(F0, m_Data.NdotV, m_Data.Roughness);
	vec3 kd = (1.0 - F) * (1.0 - m_Data.Metalic);
	vec3 diffuseIBL = m_Data.Albedo * irradiance;

	//TODO:(Jacob) calculate specularIBL as well
	int envRadianceTexLevels = textureQueryLevels(u_EnviornmentRadianceTexture);
	float NoV = clamp(m_Data.NdotV, 0.0, 1.0);
	vec3 R = 2.0 * dot(m_Data.CameraPos, m_Data.Normal) * m_Data.Normal - m_Data.CameraPos;
	vec3 specularIrradiance = textureLod(u_EnviornmentRadianceTexture, RotateVectorAboutY(0.0, Lr), (m_Data.Roughness) * envRadianceTexLevels).rgb;

	vec2 specularBRDF = texture(u_BRDFLUTTexture, vec2(m_Data.NdotV, 1.0 - m_Data.Roughness)).rg;

	vec3 specularIBL = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

	return kd * diffuseIBL + specularIBL;
}

void main() {

	if((texture(AlbedoMap, uvs) * u_MaterialData.Albedo).a < 0.05)
		discard;

	m_Data.Albedo = texture(AlbedoMap, uvs).rgb * u_MaterialData.Albedo.rgb;
	m_Data.Roughness = texture(MetalicRoughnessMap, uvs).g;
	m_Data.Roughness = max(m_Data.Roughness, 0.05);
	m_Data.Metalic = texture(MetalicRoughnessMap, uvs).b;
	m_Data.Normal = normalize(normal);
	if(u_MaterialData.UseNormalMap > 0)
	{
		m_Data.Normal = normalize(texture(NormalMap, uvs).rgb * 2.0 - 1.0);
		m_Data.Normal = normalize(TBN * m_Data.Normal);
	}
	m_Data.CameraPos = normalize(viewPos - worldSpace.xyz);
	m_Data.NdotV = max(dot(m_Data.Normal, m_Data.CameraPos), 0.0);

	vec3 F0 = mix(Fdielectric, m_Data.Albedo, m_Data.Metalic);

	vec3 Lr = 2.0 * m_Data.NdotV * m_Data.Normal - m_Data.CameraPos;

	vec3 LightPos[4] = {
		vec3(4.0, 0.0, -4.0),
		vec3(-4.0, 0.0, -4.0),
		vec3(-4.0, 0.0, 4.0),
		vec3(4.0, 0.0, 4.0)
	};
	vec3 LightColor = vec3(1.0, 1.0, 1.0);
	float LightIntensity = 2.0;

	vec3 lightContribution = vec3(0.0, 0.0, 0.0);

	for(int i = 0; i < 4; i++)
	{
		lightContribution += CalculateLight(F0, LightPos[i], LightColor, LightIntensity);
	}
	//lightContribution += m_Data.Albedo * u_MaterialData.Emisivity;

	float EnvironmentIntensity = 1.0;
	vec3 iblContribution = IBL(F0, Lr) * EnvironmentIntensity;

	vec3 color = iblContribution + lightContribution;
	outColor = vec4(color, 1.0);
}