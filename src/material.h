#pragma once

// EXTERNAL LIBRARIES
#define STB_IMAGE_IMPLEMENTATION
#include <GL/glew.h>
#include "glm/glm.hpp"
#include "stb_image.h"
#include "debug.h"

// STANDARD LIBRARY
#include <iostream>
#include <functional>
#include <string>
#include <string.h>
#include <array>

// PROJECT HEADERS
#include "utils.h"

struct Texture
{
    char name[32];
    char tempName[32];
    uint64_t textureHandle;
    unsigned int textureID;

    void CleanUp() 
    {
        if (textureID) glDeleteTextures(1, &textureID);
    }

    void LoadImage(const std::string filepath)
    {
        int width, height, bpp;
        unsigned char* localbuffer = stbi_load(filepath.c_str(), &width, &height, &bpp, 3);
        if (!localbuffer)
        {
            std::cerr << "[LoadImage] Failed! Could not find image at: " << filepath << std::endl;
            return;
        }

        std::string filename = ExtractName(filepath).substr(0, 32);
        strcpy_s(name, 32, filename.c_str());
        strcpy_s(tempName, 32, filename.c_str());

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, localbuffer);
        glGenerateMipmap(GL_TEXTURE_2D); 
        stbi_image_free(localbuffer);

        textureHandle = glGetTextureHandleARB(textureID);
    }
};


struct MaterialData
{
    alignas(16) glm::vec3 colour;
    float roughness;
    float emission;
    float IOR;
    int refractive;
    uint64_t albedoHandle;
    uint64_t normalHandle;
    uint64_t roughnessHandle;
    uint32_t textureFlags;

    MaterialData()
    {
        colour          = glm::vec3(0.8f, 0.8f, 0.8f);
        roughness       = 1.0f;
        emission        = 0.0f;
        IOR             = 1.45;
        refractive      = 0;
        albedoHandle    = -1;
        normalHandle    = -1;
        roughnessHandle = -1;
        textureFlags    = 0;
    }
};


struct Material
{
    MaterialData data;
    unsigned int albedoID;
    unsigned int normalID;
    unsigned int roughnessID;
    unsigned int FBO, RBO, FrameBufferTextureID;
    char name[32];
    char tempName[32];

    Material()
    {
        strcpy_s(name, 32, "material");
        strcpy_s(tempName, 32, "material");
    }
    
    void CreateThumbnailFrameBuffer(int width, int height)
    {
        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glGenTextures(1, &FrameBufferTextureID);
        glBindTexture(GL_TEXTURE_2D, FrameBufferTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width,  height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FrameBufferTextureID, 0);
        glGenRenderbuffers(1, &RBO);
        glBindRenderbuffer(GL_RENDERBUFFER, RBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
        
    void CleanUp()
    {
        if (albedoID) glDeleteTextures(1, &albedoID);
        if (normalID) glDeleteTextures(1, &normalID);
        if (roughnessID) glDeleteTextures(1, &roughnessID);
    }


    void LoadAlbedo(std::string filepath)
    {
        // UNLOAD EXISTING TEXTURE IF IT EXISTS
        if (data.albedoHandle != -1) glMakeTextureHandleNonResidentARB(data.albedoHandle);

        int width, height, bpp;
        unsigned char* localbuffer = stbi_load(filepath.c_str(), &width, &height, &bpp, 3);
        if (!localbuffer)
        {
            std::cerr << "[LoadAlbedo] Failed! Could not find image at: " << filepath << std::endl;
            return;
        }

        glGenTextures(1, &albedoID);
        glBindTexture(GL_TEXTURE_2D, albedoID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, localbuffer);
        glGenerateMipmap(GL_TEXTURE_2D); 
        stbi_image_free(localbuffer);

        data.albedoHandle = glGetTextureHandleARB(albedoID);
        data.textureFlags |= (1 << 0);
        glMakeTextureHandleResidentARB(data.albedoHandle);
    }

    void LoadNormal(std::string filepath)
    {
        // UNLOAD EXISTING TEXTURE IF IT EXISTS
        if (data.normalHandle != -1) glMakeTextureHandleNonResidentARB(data.normalHandle);

        int width, height, bpp;
        unsigned char* localbuffer = stbi_load(filepath.c_str(), &width, &height, &bpp, 3);
        if (!localbuffer)
        {
            std::cerr << "[LoadNormal] Failed! Could not find image at: " << filepath << std::endl;
            return;
        }

        glGenTextures(1, &normalID);
        glBindTexture(GL_TEXTURE_2D, normalID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, localbuffer);
        glGenerateMipmap(GL_TEXTURE_2D); 
        stbi_image_free(localbuffer);

        data.normalHandle = glGetTextureHandleARB(normalID);
        data.textureFlags |= (1 << 1);
        glMakeTextureHandleResidentARB(data.normalHandle);
    }

    void LoadRoughness(std::string filepath)
    {
        // UNLOAD EXISTING TEXTURE IF IT EXISTS
        if (data.roughnessHandle != -1) glMakeTextureHandleNonResidentARB(data.roughnessHandle);

        int width, height, bpp;
        unsigned char* localbuffer = stbi_load(filepath.c_str(), &width, &height, &bpp, 1);
        if (!localbuffer)
        {
            std::cerr << "[LoadRoughness] Failed! Could not find image at: " << filepath << std::endl;
            return;
        }

        glGenTextures(1, &roughnessID);
        glBindTexture(GL_TEXTURE_2D, roughnessID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // CONVERT TO GREYSCALE FORM USER datenwolf https://stackoverflow.com/questions/8518596/opengl-rgba-to-grayscale-from-1-component
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, localbuffer);
        glGenerateMipmap(GL_TEXTURE_2D); 
        stbi_image_free(localbuffer);

        data.roughnessHandle = glGetTextureHandleARB(roughnessID);
        data.textureFlags |= (1 << 2);
        glMakeTextureHandleResidentARB(data.roughnessHandle);
    }

    void UpdateAlbedo(unsigned int newTextureID, uint64_t newTextureHandle)
    {   
        // IF ALBEDO EXISTS
        if (data.albedoHandle != -1)
        {   
            // UNLOAD TEXTURE FROM GPU
            glMakeTextureHandleNonResidentARB(data.albedoHandle);
        }

        albedoID = newTextureID;
        data.albedoHandle = newTextureHandle;
        data.textureFlags |= (1 << 0);
        glMakeTextureHandleResidentARB(data.albedoHandle);
    }

    void UpdateNormal(unsigned int newTextureID, uint64_t newTextureHandle)
    {
        // IF NORMAL EXISTS
        if (data.normalHandle != -1)
        {   
            // UNLOAD TEXTURE FROM GPU
            glMakeTextureHandleNonResidentARB(data.normalHandle);
        }

        normalID = newTextureID;
        data.normalHandle = newTextureHandle;
        data.textureFlags |= (1 << 1);
        glMakeTextureHandleResidentARB(data.normalHandle);
    }
    
    void UpdateRoughness(unsigned int newTextureID, uint64_t newTextureHandle)
    {
        // IF ROUGHNESS EXISTS
        if (data.roughnessHandle != -1)
        {   
            // UNLOAD TEXTURE FROM GPU
            glMakeTextureHandleNonResidentARB(data.roughnessHandle);
        }

        roughnessID = newTextureID;
        data.roughnessHandle = newTextureHandle;
        data.textureFlags |= (1 << 2);
        glMakeTextureHandleResidentARB(data.roughnessHandle);
    }
};