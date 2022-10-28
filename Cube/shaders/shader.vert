#version 450
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

layout( push_constant ) uniform constants
{
	mat4 ModelViewProj;
} PushConstants;

layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = PushConstants.ModelViewProj * vec4(pos, 1.0);
	fragColor = col;
}