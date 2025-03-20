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

// SIMPLIFIED ACES TONE MAPPING
vec3 ACES(vec3 colour)
{
    vec3 numerator = colour * (2.51f * colour + 0.03f);
    vec3 denominator = colour * (2.43f * colour + 0.59f) + 0.14f;
    vec3 result = numerator / denominator;
    return clamp(result, 0.0f, 1.0f);
}


void main()
{
    // DIRECTIONAL LIGHT
    vec3 directLightPos = vec3(4, -3, -3);
    vec3 directLightColour = vec3(2, 2, 2);
    vec3 dirToLight = normalize(directLightPos);
    vec3 directLight = directLightColour * max(dot(FragNormal, dirToLight), 0);

    // AMBIENT LIGHT
    vec3 ambientLight = vec3(0.5, 0.7, 0.95) * 0.5;

    // SURFACE COLOUR
    vec3 surfaceColour = material.colour; 
    if ((material.textureFlags & (1 << 0)) != 0)
    {
        surfaceColour = material.colour * texture(sampler2D(material.albedoHandle), FragUV).xyz;
    }
    
    // EMITTED LIGHT
    vec3 emittedLight = surfaceColour * material.emission;

    // CALCULATE FRAGMENT LIGHTING
    vec3 lighting = (directLight + ambientLight) * surfaceColour + emittedLight;

    // SET FRAG COLOUR
    FragColour = vec4(ACES(lighting), 1); 
}