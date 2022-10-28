#version 450
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec4 bitangent;
layout(location = 4) in vec4 color;
layout(location = 5) in vec2 uv;

layout( push_constant ) uniform constants
{
	mat4 LightMat;
	mat4 ModelViewProj;
} PushConstants;

layout(location = 0) out vec3 OutputNormalWorldSpace;
layout(location = 1) out vec3 OutputWorldSpace;
layout(location = 2) out vec3 OutputView;
layout(location = 3) out vec3 LightPositon;
layout(location = 4) out vec3 fragColor;

void main() {
	OutputNormalWorldSpace = normalize(mat3(transpose(inverse(mat4(1.0)))) * normal);
	OutputWorldSpace = pos;
	mat4 view = mat4(vec4(-1.0, 0.0, 0.0, 0.0), vec4(0.0, -1.0, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, -5.0, 1.0));	//Hard coded View matrix so I don't have to Use Uniform buffer for this
	OutputView = vec3(inverse(view)[3]);
	LightPositon = mat3(PushConstants.LightMat) * vec3(6.0, 0.0, 0.0);	//Light Rotation Matrix * Light HardCoded Position
	fragColor = color.xyz;
	gl_Position = PushConstants.ModelViewProj * vec4(pos, 1.0);
}