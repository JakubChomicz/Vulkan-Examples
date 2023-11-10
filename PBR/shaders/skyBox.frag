#version 460 core

layout(location = 0) out vec4 OutputColor;

layout (set = 0, binding = 1) uniform samplerCube u_EnviornmentRadianceTexture;

layout (set = 0, binding = 2) uniform samplerCube u_EnviornmentIrradianceTexture;

layout (set = 0, binding = 3) uniform sampler2D u_BRDFLUTTexture;

layout (location = 0) in vec3 v_Pos;

void main()
{
    // For some reason v_Pos is incorrect on y axis so just negate it
    float EnvironmentIntensity = 1.0;
	OutputColor = textureLod(u_EnviornmentRadianceTexture, normalize(vec3(v_Pos.x, -v_Pos.y, v_Pos.z)), 0.0) * EnvironmentIntensity;    //0.5 is Environment Intensity
    OutputColor.a = 1.0;
}
