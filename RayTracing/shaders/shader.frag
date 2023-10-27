#version 460

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

layout(binding = 1) uniform accelerationStructureEXT topLevelAS;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec4 worldSpace;
layout(location = 2) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

void main() {

	vec3 normalizedNormal = normalize(normal);

	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	vec3 LightPos = vec3(15.0, 6.0, 8.0);
	vec3 LightDir   = normalize(LightPos - worldSpace.xyz);
	vec3 ViewDir    = normalize(viewPos - worldSpace.xyz);
	vec3 HalfwayDir = normalize(LightDir + ViewDir);

	float diff = max(dot(normalizedNormal, LightDir), 0.0);
	vec3 diffuse = lightColor * diff;

	float spec = pow(max(dot(normalizedNormal, HalfwayDir), 0.0), 0.2);
	vec3 specular = lightColor * spec;

	outColor = vec4((diffuse + specular) * vec3(0.5, 0.5, 0.5), 1.0);

	// Ray Query for shadow
	vec3  origin    = worldSpace.xyz;
	vec3  direction = normalize(LightPos - worldSpace.xyz);  // vector to light
	float tMin      = 0.01;
	float tMax      = 10000;

	// Initializes a ray query object but does not start traversal
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tMin, direction, tMax);

	// Start traversal: return false if traversal is complete
	while(rayQueryProceedEXT(rayQuery))
	{
	}

	// Returns type of committed (true) intersection
	if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT)
	{
		// Got an intersection == Shadow
		outColor *= 0.1;
	}
}