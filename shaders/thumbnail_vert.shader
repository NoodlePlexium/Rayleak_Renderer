#version 440 core

layout(location = 0) in vec3 VertexPosition; 
layout(location = 1) in vec3 VertexNormal;   
layout(location = 2) in vec2 UV; 
uniform mat4 u_MVP;

out vec3 FragPos;
out vec3 FragNormal;
out vec2 FragUV;

void main()
{
    gl_Position = u_MVP * vec4(VertexPosition, 1);
    FragPos = gl_Position.xyz;
    FragNormal = VertexNormal;
    FragUV = UV;
}