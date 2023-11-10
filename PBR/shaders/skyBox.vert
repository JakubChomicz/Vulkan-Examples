#version 460 core
layout(location = 0) in vec4 m_Pos;
layout(location = 1) in vec2 m_Uv;

layout(std140, set = 0, binding = 0) uniform Camera
{
	mat4 View;
	mat4 Projection;
} cam;

layout(location = 0) out vec3 v_Pos;

void main()
{
	vec4 position = vec4(m_Pos.x, m_Pos.y, 0.0, 1.0);
	gl_Position = position;
	v_Pos = mat3(inverse(cam.View)) * vec3(inverse(cam.Projection) * position);
}