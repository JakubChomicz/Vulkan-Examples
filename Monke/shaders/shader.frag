#version 450

layout(location = 0) in vec3 InputNormalWorldSpace;
layout(location = 1) in vec3 InputWorldSpace;
layout(location = 2) in vec3 InputView;
layout(location = 3) in vec3 LightPositon;
layout(location = 4) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

vec3 PointLight(vec3 Pos, vec3 normal, vec3 WorldSpacePos, vec3 View)
{
	float radius = 8.0;
	vec3 lightDirection = (Pos - WorldSpacePos) / radius;
	vec3 viewDiraction = normalize(View - WorldSpacePos);
	float attenuation = max(0.0, 1.0 - dot(lightDirection, lightDirection));
	lightDirection = normalize(lightDirection);
	vec3 halfwayDir = normalize(lightDirection + viewDiraction);
	vec3 intensity = vec3(1.0, 1.0, 1.0) * (0.5) * attenuation;	//FIX 10
	//diffuse
	float diff = max(dot(normal, lightDirection), 0.0);
	vec3 diffuse = intensity * diff;
	//Sepcular
	float blinnTerm = dot(normal, halfwayDir);
	blinnTerm = clamp(blinnTerm, 0, 1);
	blinnTerm = pow(blinnTerm, 32.0);
	vec3 specular = intensity * blinnTerm;
	
	return (diffuse + specular);
}

void main() {
	outColor = vec4(PointLight(LightPositon, normalize(InputNormalWorldSpace), InputWorldSpace, InputView) * fragColor, 1.0);
}