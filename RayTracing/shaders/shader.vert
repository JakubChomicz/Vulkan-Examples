#version 450
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec4 bitangent;
layout(location = 4) in vec4 color;
layout(location = 5) in vec2 uv;

layout( push_constant ) uniform constants
{
	mat4 ModelTransform;
} PushConstants;

layout(std140, set = 0, binding = 0) uniform Camera
{
	mat4 View;
	mat4 Projection;
} cam;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec4 out_worldSpace;
layout(location = 2) out vec3 out_viewPos;

void main() {
	out_viewPos = inverse(cam.View)[3].xyz;
	out_normal = normal;
	out_worldSpace = PushConstants.ModelTransform * vec4(pos, 1.0);
	gl_Position = cam.Projection * cam.View * PushConstants.ModelTransform * vec4(pos, 1.0);
}