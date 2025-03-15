#pragma once

// STANDARD LIBRARY
#include <vector>
#include <string>

// PROJECT HEADERS
#include "light.h"
#include "gpu_memory_manager.h"

class LightManager
{
public:
    // LIST OF LIGHTS
    std::vector<DirectionalLight> directionalLights;
    std::vector<PointLight> pointLights;
    std::vector<Spotlight> spotlights;

    // LIST OF LIGHT NAMES
    std::vector<std::string> directionalLightNames;
    std::vector<std::string> pointLightNames;
    std::vector<std::string> spotlightNames;

    LightManager(unsigned int _pathtraceShader) : 
    pathtraceShader(_pathtraceShader),
    DirectionalLightBuffer(DynamicContiguousBuffer(7, 0)),
    PointLightBuffer(DynamicContiguousBuffer(8, 0)),
    SpotlightBuffer(DynamicContiguousBuffer(9, 0)) 
    {
        
    }

    void AddDirectionalLight()
    {
        DirectionalLight light;
        directionalLights.push_back(light);
        directionalLightNames.push_back(DefaultDirectionalName());

        // SEND LIGHT DATA TO GPU
        AddDirectionalLightToScene(light);
    }

    void AddPointLight()
    {
        PointLight light;
        pointLights.push_back(light);
        pointLightNames.push_back(DefaultPointName());

        // SEND LIGHT DATA TO GPU
        AddPointLightToScene(light);
    }

    void AddSpotlight()
    {
        Spotlight light;
        spotlights.push_back(light);
        spotlightNames.push_back(DefaultSpotName());

        // SEND LIGHT DATA TO GPU
        AddSpotlightToScene(light);
    }

    void DeleteDirectionalLight(int index)
    {
        directionalLights.erase(directionalLights.begin() + index);
        directionalLightNames.erase(directionalLightNames.begin() + index);

        // DELETE FROM GPU MEMORY
        DirectionalLightBuffer.DeleteShift(index * sizeof(DirectionalLight), sizeof(DirectionalLight));

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLights.size());
    }

    void DeletePointLight(int index)
    {
        pointLights.erase(pointLights.begin() + index);
        pointLightNames.erase(pointLightNames.begin() + index);

        // DELETE FROM GPU MEMORY
        PointLightBuffer.DeleteShift(index * sizeof(PointLight), sizeof(PointLight));

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLights.size());
    }

    void DeleteSpotlight(int index)
    {
        spotlights.erase(spotlights.begin() + index);
        spotlightNames.erase(spotlightNames.begin() + index);

        // DELETE FROM GPU MEMORY
        SpotlightBuffer.DeleteShift(index * sizeof(Spotlight), sizeof(Spotlight));

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlights.size());
    }

    void UpdateDirectionalLight(int lightIndex)
    {
        // GET DIRECTIONAL LIGHT POINTER
        DirectionalLight* light = &directionalLights[lightIndex];

        // GET LIGHT SIZE
        uint32_t lightDataSize = sizeof(DirectionalLight);

        // GET LIGHT BUFFER OFFSET
        uint32_t bufferOffset = lightIndex * lightDataSize;

        // GET MAPPED LIGHT BUFFER
        void* mappedLightBuffer = DirectionalLightBuffer.GetMappedBuffer(bufferOffset, lightDataSize);

        // COPY UPDATED LIGHT DATA TO THE GPU
        memcpy((char*)mappedLightBuffer, light, lightDataSize);

        // UNMAP BUFFER
        DirectionalLightBuffer.UnmapBuffer();
    }

    void UpdatePointLight(int lightIndex)
    {
        // GET POINT LIGHT POINTER
        PointLight* light = &pointLights[lightIndex];

        // GET LIGHT SIZE
        uint32_t lightDataSize = sizeof(PointLight);

        // GET LIGHT BUFFER OFFSET
        uint32_t bufferOffset = lightIndex * lightDataSize;

        // GET MAPPED LIGHT BUFFER
        void* mappedLightBuffer = PointLightBuffer.GetMappedBuffer(bufferOffset, lightDataSize);

        // COPY UPDATED LIGHT DATA TO THE GPU
        memcpy((char*)mappedLightBuffer, light, lightDataSize);

        // UNMAP BUFFER
        PointLightBuffer.UnmapBuffer();
    }

    void UpdateSpotlight(int lightIndex)
    {
        // GET SPOTLIGHT POINTER
        Spotlight* light = &spotlights[lightIndex];

        // GET LIGHT SIZE
        uint32_t lightDataSize = sizeof(Spotlight);

        // GET LIGHT BUFFER OFFSET
        uint32_t bufferOffset = lightIndex * lightDataSize;

        // GET MAPPED LIGHT BUFFER
        void* mappedLightBuffer = SpotlightBuffer.GetMappedBuffer(bufferOffset, lightDataSize);

        // COPY UPDATED LIGHT DATA TO THE GPU
        memcpy((char*)mappedLightBuffer, light, lightDataSize);

        // UNMAP BUFFER
        SpotlightBuffer.UnmapBuffer();
    }

private:
    DynamicContiguousBuffer DirectionalLightBuffer;
    DynamicContiguousBuffer PointLightBuffer;
    DynamicContiguousBuffer SpotlightBuffer;

    // PATH TRACING SHADER ID
    unsigned int pathtraceShader;

    void AddDirectionalLightToScene(DirectionalLight& directionalLight)
    {
        glUseProgram(pathtraceShader);

        // GET DIRECTIONAL LIGHT SIZE
        uint32_t directionalLightSize = sizeof(DirectionalLight);

        // GROW BUFFER TO ACCOMODATE NEW DIRECTIONAL LIGHT
        DirectionalLightBuffer.GrowBuffer(directionalLightSize);

        // GET MAPPED BUFFER
        void* mappedDirectionalLightBuffer = DirectionalLightBuffer.GetMappedBuffer(DirectionalLightBuffer.UsedCapacity() - directionalLightSize, directionalLightSize);

        // COPY LIGHT TO THE GPU
        memcpy((char*)mappedDirectionalLightBuffer, &directionalLight, directionalLightSize);

        // UNMAP BUFFER
        DirectionalLightBuffer.UnmapBuffer();

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLights.size());
    }

    void AddPointLightToScene(PointLight& pointLight)
    {
        glUseProgram(pathtraceShader);

        // GET DIRECTIONAL LIGHT SIZE
        uint32_t pointLightSize = sizeof(PointLight);

        // GROW BUFFER TO ACCOMODATE NEW DIRECTIONAL LIGHT
        PointLightBuffer.GrowBuffer(pointLightSize);

        // GET MAPPED BUFFER
        void* mappedPointLightBuffer = PointLightBuffer.GetMappedBuffer(PointLightBuffer.UsedCapacity() - pointLightSize, pointLightSize);

        // COPY LIGHT TO THE GPU
        memcpy((char*)mappedPointLightBuffer, &pointLight, pointLightSize);

        // UNMAP BUFFER
        PointLightBuffer.UnmapBuffer();

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLights.size());
    }

    void AddSpotlightToScene(Spotlight& spotlight)
    {
        glUseProgram(pathtraceShader);
        
        // GET DIRECTIONAL LIGHT SIZE
        uint32_t spotlightSize = sizeof(Spotlight);

        // GROW BUFFER TO ACCOMODATE NEW DIRECTIONAL LIGHT
        SpotlightBuffer.GrowBuffer(spotlightSize);

        // GET MAPPED BUFFER
        void* mappedSpotlightBuffer = SpotlightBuffer.GetMappedBuffer(SpotlightBuffer.UsedCapacity() - spotlightSize, spotlightSize);

        // COPY LIGHT TO THE GPU
        memcpy((char*)mappedSpotlightBuffer, &spotlight, spotlightSize);

        // UNMAP BUFFER
        SpotlightBuffer.UnmapBuffer();

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlights.size());
    }

    // GENERATE A DEFAULT DIRECTIONAL LIGHT NAME
    std::string DefaultDirectionalName()
    {
        int count = 1;
        for (int i=0; i<directionalLightNames.size(); i++)
        {
            std::string checkName = "directional light " + std::to_string(count); 
            bool nameEqual = strcmp(directionalLightNames[i].c_str(), checkName.c_str()) == 0;
            count += nameEqual;
        }
        return "directional light " + std::to_string(count);
    }

    // GENERATE A DEFAULT POINT LIGHT NAME
    std::string DefaultPointName()
    {
        int count = 1;
        for (int i=0; i<pointLightNames.size(); i++)
        {
            std::string checkName = "point light " + std::to_string(count); 
            bool nameEqual = strcmp(pointLightNames[i].c_str(), checkName.c_str()) == 0;
            count += nameEqual;
        }
        return "point light " + std::to_string(count);
    }

    // GENERATE A DEFAULT SPOTLIGHT NAME
    std::string DefaultSpotName()
    {
        int count = 1;
        for (int i=0; i<spotlightNames.size(); i++)
        {
            std::string checkName = "spotlight " + std::to_string(count); 
            bool nameEqual = strcmp(spotlightNames[i].c_str(), checkName.c_str()) == 0;
            count += nameEqual;
        }
        return "spotlight " + std::to_string(count);
    }
};