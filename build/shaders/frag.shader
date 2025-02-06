#version 440 core

out vec4 FragColour;
in vec2 TextureCoord;
uniform sampler2D DisplayTexture;

void main()
{
    FragColour = texture(DisplayTexture, TextureCoord);
}