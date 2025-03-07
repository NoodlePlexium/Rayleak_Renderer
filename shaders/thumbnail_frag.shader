#version 440 core
#extension GL_ARB_bindless_texture : enable
#extension GL_NV_gpu_shader5 : enable

out vec4 FragColour;
in vec3 FragPos;
in vec3 FragNormal;
in vec2 FragUV;

struct Material
{
    vec3 colour;
    float roughness;
    float emission;
    float IOR;
    int refractive;
    uint64_t albedoHandle;
    uint64_t normalHandle;
    uint64_t roughnessHandle;
    uint textureFlags;
};

uniform Material material;


void main()
{
    // DIRECTIONAL LIGHT
    vec3 lightPos = vec3(4, -3, -3);
    vec3 lightColour = vec3(1, 1, 1);
    vec3 dirToLight = normalize(lightPos);

    vec3 normal = normalize(FragPos);

    // AMBIENT LIGHT
    vec3 ambientLight = vec3(0.5, 0.7, 0.95) * 0.5;

    vec3 lighting = lightColour * max(dot(normal, dirToLight), 0) + ambientLight;



    vec4 albedoColour = vec4(material.colour, 1);
    if ((material.textureFlags & (1 << 0)) != 0)
    {
        albedoColour = vec4(material.colour, 1) * texture(sampler2D(material.albedoHandle), FragUV);
    }
    FragColour = albedoColour * vec4(lighting, 1);
}