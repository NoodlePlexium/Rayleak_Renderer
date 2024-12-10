#version 440 core

out vec4 FragColour;
in vec2 TextureCoord;
uniform sampler2D RenderTexture;

void main()
{
    FragColour = texture(RenderTexture, TextureCoord);
}