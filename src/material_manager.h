#pragma once

// STANDARD LIBRARY
#include <vector>
#include <stdio.h>
#include <string.h>

// PROJECT HEADERS
#include "gpu_memory_manager.h"
#include "material.h"
#include "render_system.h"


class MaterialManager
{
public:

    std::vector<Material> materials;
    std::vector<Texture> textures;

    MaterialManager(unsigned int _pathtraceShader) : 
    pathtraceShader(_pathtraceShader),
    MaterialBuffer(DynamicContiguousBuffer(4, 0))
    {
        // CREATE DEFAULT MATERIAL
        Material defaultMat;
        SetDefaultMaterialName(defaultMat);
        materials.push_back(defaultMat);

        // SEND TO GPU
        AddMaterialToScene(materials[0].data);
    }

    void CreateMaterial(float thumbnailSize, RenderSystem& renderSystem)
    {
        // CREATE MATERIAL
        Material newMaterial;
        newMaterial.CreateThumbnailFrameBuffer(thumbnailSize, thumbnailSize);
        SetDefaultMaterialName(newMaterial);
        materials.push_back(newMaterial);

        // RENDER MATERIAL THUMBNAIL
        renderSystem.RenderThumbnail(newMaterial, thumbnailSize, thumbnailSize);

        // SEND TO GPU
        AddMaterialToScene(newMaterial.data);
    }

    void ImportTexture()
    {
        // ADAPTED FROM USER tinyfiledialogs https://stackoverflow.com/questions/6145910/cross-platform-native-open-save-file-dialogs
        const char *lFilterPatterns[2] = { "*.png", "*.jpg" };
        const char* selection = tinyfd_openFileDialog("Import Image", "C:\\", 2,lFilterPatterns, NULL, 0 );
        if (selection)
        {
            Texture newTexture;
            newTexture.LoadImage(selection);
            textures.push_back(newTexture);
        }
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

    // DYNAMIC SHADER STORAGE BUFFER
    DynamicContiguousBuffer MaterialBuffer;    

    // PATH TRACING SHADER ID
    unsigned int pathtraceShader;

    // SET DEFAULT MATERIAL NAME
    void SetDefaultMaterialName(Material& material)
    {
        int count = 0;
        for (int i=0; i<materials.size(); i++)
        {
            std::string checkName = "material " + std::to_string(count); 
            bool nameEqual = strcmp(materials[i].name, checkName.c_str()) == 0;
            count += nameEqual;
        }
        std::string matName = "material " + std::to_string(count);
        strcpy_s(material.name, 32, matName.c_str());
        strcpy_s(material.tempName, 32, matName.c_str());
    }
};

