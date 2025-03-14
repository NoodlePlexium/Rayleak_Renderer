#pragma once

// STANDARD LIBRARY
#include <vector>

// PROJECT HEADERS
#include "gpu_memory_manager.h"
#include "material.h"


class MaterialManager
{
public:

    MaterialManager(unsigned int _pathtraceShader) : pathtraceShader(_pathtraceShader)
    {
        // CREATE DEFAULT MATERIAL
        Material defaultMat;
        materials.push_back(defaultMat);

        // CREATE DYNAMIC GPU BUFFER
        MaterialBuffer(DynamicStorageBuffer(4, 0));
    }

    void CreateMaterial()
    {

    }

    void AddMaterialToScene(MaterialData& materialData)
    {        
        // GET MATERIAL SIZE
        uint32_t materialDataSize = sizeof(MaterialData);

        // GROW BUFFER TO ACCOMODATE NEW MATERIAL
        MaterialBuffer.GrowBuffer(materialDataSize);

        // GET MAPPED MATERIAL BUFFER
        void* mappedMaterialBuffer = MaterialBuffer.GetMappedBuffer(MaterialBuffer.BufferSize() - materialDataSize, materialDataSize);

        // COPY MATERIAL TO THE GPU
        memcpy((char*)mappedMaterialBuffer, &materialData, materialDataSize);

        // UNMAP BUFFER
        MaterialBuffer.UnmapBuffer();
    }

    void UpdateMaterial(MaterialData& materialData, int materialIndex)
    {
        // GET MATERIAL SIZE
        uint32_t materialDataSize = sizeof(MaterialData);

        // GET MATERIAL BUFFER OFFSET
        uint32_t bufferOffset = materialIndex * materialDataSize;

        // GET MAPPED MATERIAL BUFFER
        void* mappedMaterialBuffer = MaterialBuffer.GetMappedBuffer(bufferOffset, materialDataSize);

        // COPY UPDATED MATERIAL DATA TO THE GPU
        memcpy((char*)mappedMaterialBuffer, &materialData, materialDataSize);

        // UNMAP BUFFER
        MaterialBuffer.UnmapBuffer();
    }

private:
    DynamicStorageBuffer MaterialBuffer;    
    std::vector<Material> materials;

    // PATH TRACING SHADER ID
    unsigned int pathtraceShader;
};