#version 460 core
layout(location = 0) in vec4 m_Pos;
layout(location = 1) in vec2 m_Uv;

layout(location = 0) out vec2 o_Uv;

void main()
{
	vec4 position = vec4(m_Pos.xy, 0.0, 1.0);
    o_Uv = m_Uv;
	gl_Position = position;
}