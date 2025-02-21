#pragma once 

// EXTERNAL LIBRARIES
#include <GL/glew.h>

// STANDARD LIBRARY
#include <vector>
#include <string>

// PROJECT HEADERS
#include "mesh.h"
#include "light.h"
#include "debug.h"


class DynamicStorageBuffer
{
public:

    DynamicStorageBuffer(int binding = 0, uint32_t allocatedSpace = 0) : _binding(binding)
    {
        bufferSize = allocatedSpace;
        glGenBuffers(1, &bufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, bufferID);
    }

    void GrowBuffer(uint32_t addSize)
    {
        bufferSize += addSize;
        unsigned int newBufferID;
        glGenBuffers(1, &newBufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, newBufferID);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, newBufferID);

        glBindBuffer(GL_COPY_READ_BUFFER, bufferID);
        glBindBuffer(GL_COPY_WRITE_BUFFER, newBufferID);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, bufferSize - addSize);
        glDeleteBuffers(1, &bufferID);

        bufferID = newBufferID;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, _binding, bufferID);
    }

    void* GetMappedBuffer(int offset, int size)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        return glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_WRITE_BIT);
    }

    void UnmapBuffer()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); 
    }

private:
    int _binding;
    unsigned int bufferID;
    uint32_t bufferSize;
};

class ModelManager
{
public:

    ModelManager(const unsigned int &pathtraceShader)
    {
        glUseProgram(pathtraceShader);
        VertexBuffer           = DynamicStorageBuffer(2, 0);
        IndexBuffer            = DynamicStorageBuffer(3, 0);
        MaterialBuffer         = DynamicStorageBuffer(4, 0);
        BvhBuffer              = DynamicStorageBuffer(5, 0);
        PartitionBuffer        = DynamicStorageBuffer(6, 0);
        // DirectionalLightBuffer = DynamicStorageBuffer(8, 0);
        // PointLightBuffer       = DynamicStorageBuffer(9, 0);
        // SpotlightBuffer        = DynamicStorageBuffer(10, 0);
    }

    void CopyLightDataToGPU(
        const std::vector<DirectionalLight> &directionalLights,
        const std::vector<PointLight> &pointLights,
        const std::vector<Spotlight> &spotlights,
        const unsigned int &pathtraceShader)
    {
        glUseProgram(pathtraceShader);

        // DIRECTIONAL LIGHTS
        glGenBuffers(1, &directionalLightBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, directionalLightBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DirectionalLight) * directionalLights.size(), directionalLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, directionalLightBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLights.size());

        // POINT LIGHTS
        glGenBuffers(1, &pointLightBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointLightBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLight) * pointLights.size(), pointLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, pointLightBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLights.size());

        // SPOTLIGHTS
        glGenBuffers(1, &spotlightBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, spotlightBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Spotlight) * spotlights.size(), spotlights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, spotlightBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlights.size());
    }


    void AddModelToScene(Model& model, const unsigned int &pathtraceShader)
    {
        glUseProgram(pathtraceShader);

        meshCount += model.submeshPtrs.size();
        glUniform1i(glGetUniformLocation(pathtraceShader, "u_meshCount"), meshCount);

        // CREATE MESH PARTITIONS AND CALCULATE BUFFER SIZES
        uint32_t appendVertexBufferSize = 0;
        uint32_t appendIndexBufferSize = 0;
        uint32_t appendBvhBufferSize = 0;
        uint32_t appendPartitionBufferSize = model.submeshPtrs.size() * sizeof(MeshPartition);
        std::vector<MeshPartition> meshPartitions;
        for (Mesh* mesh : model.submeshPtrs) 
        {
            // CREATE NEW MESH PARTITION
            MeshPartition mPart;
            mPart.verticesStart = vertexStart;
            mPart.indicesStart = indexStart;
            mPart.materialIndex = mesh->materialIndex;
            mPart.bvhNodeStart = bvhStart;
            mPart.inverseTransform = mesh->GetInverseTransformMat();
            vertexStart += mesh->vertices.size();
            indexStart += mesh->indices.size();
            bvhStart += mesh->nodesUsed;
            meshPartitions.push_back(mPart);

            // INCREMENT BUFFER SIZES
            appendVertexBufferSize += mesh->vertices.size() * sizeof(Vertex);
            appendIndexBufferSize += mesh->indices.size() * sizeof(uint32_t);
            appendBvhBufferSize += mesh->nodesUsed * sizeof(BVH_Node);
        }

        // GROW BUFFERS TO ACCOMODATE NEW MESHES
        VertexBuffer.GrowBuffer(appendVertexBufferSize);
        IndexBuffer.GrowBuffer(appendIndexBufferSize);
        BvhBuffer.GrowBuffer(appendBvhBufferSize);
        PartitionBuffer.GrowBuffer(appendPartitionBufferSize);

        // GET BUFFER MAPPINGS
        void* mappedVertexBuffer = VertexBuffer.GetMappedBuffer(vertexBufferSize, appendVertexBufferSize);
        void* mappedIndexBuffer = IndexBuffer.GetMappedBuffer(indexBufferSize, appendIndexBufferSize);
        void* mappedBvhBuffer = BvhBuffer.GetMappedBuffer(bvhBufferSize, appendBvhBufferSize);
        void* mappedPartitionBuffer = PartitionBuffer.GetMappedBuffer(partitionBufferSize, appendPartitionBufferSize);

        // INCREASE USED BUFFER SIZES
        vertexBufferSize += appendVertexBufferSize;
        indexBufferSize += appendIndexBufferSize;
        bvhBufferSize += appendBvhBufferSize;
        partitionBufferSize += appendPartitionBufferSize;

        // COPY BUFFER DATA TO GPU
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        uint32_t bvhOffset = 0;
        uint32_t partitionOffset = 0;
        for (const Mesh* mesh : model.submeshPtrs) 
        {
            memcpy((char*)mappedVertexBuffer + vertexOffset, mesh->vertices.data(), mesh->vertices.size() * sizeof(Vertex));
            memcpy((char*)mappedIndexBuffer + indexOffset, mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t));
            memcpy((char*)mappedBvhBuffer + bvhOffset, mesh->bvhNodes, mesh->nodesUsed * sizeof(BVH_Node));
            vertexOffset += mesh->vertices.size() * sizeof(Vertex);
            indexOffset += mesh->indices.size() * sizeof(uint32_t);
            bvhOffset += mesh->nodesUsed * sizeof(BVH_Node);
        }
        for (const MeshPartition& partition : meshPartitions)
        {
            memcpy((char*)mappedPartitionBuffer + partitionOffset, &partition, sizeof(MeshPartition));
            partitionOffset += sizeof(MeshPartition);
        }

        // UNMAP BUFFERS
        VertexBuffer.UnmapBuffer();
        IndexBuffer.UnmapBuffer();
        BvhBuffer.UnmapBuffer();
        PartitionBuffer.UnmapBuffer();
    }

    void AddMaterialToScene(MaterialData& materialData, const unsigned int &pathtraceShader)
    {
        glUseProgram(pathtraceShader);
        
        // GET MATERIAL SIZE
        uint32_t materialDataSize = sizeof(MaterialData);

        // GROW BUFFER TO ACCOMODATE NEW MATERIAL
        MaterialBuffer.GrowBuffer(materialDataSize);

        // GET MAPPED MATERIAL BUFFER
        void* mappedMaterialBuffer = MaterialBuffer.GetMappedBuffer(materialBufferSize, materialDataSize);

        // INCREASE USED BUFFER SIZES
        materialBufferSize += materialDataSize;

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

    ~ModelManager()
    {
        glDeleteBuffers(1, &vertexBuffer);
        glDeleteBuffers(1, &indexBuffer);
        glDeleteBuffers(1, &materialBuffer);
        glDeleteBuffers(1, &bvhBuffer);
        glDeleteBuffers(1, &partitionBuffer);
        glDeleteBuffers(1, &emissiveBuffer);
        glDeleteBuffers(1, &directionalLightBuffer);
        glDeleteBuffers(1, &pointLightBuffer);
        glDeleteBuffers(1, &spotlightBuffer);
    }
private:

    unsigned int vertexBuffer;
    unsigned int indexBuffer;
    unsigned int materialBuffer;
    unsigned int bvhBuffer;
    unsigned int partitionBuffer;
    unsigned int emissiveBuffer;
    unsigned int directionalLightBuffer;
    unsigned int pointLightBuffer;
    unsigned int spotlightBuffer;

    DynamicStorageBuffer VertexBuffer;
    DynamicStorageBuffer IndexBuffer;
    DynamicStorageBuffer MaterialBuffer;
    DynamicStorageBuffer BvhBuffer;
    DynamicStorageBuffer PartitionBuffer;
    DynamicStorageBuffer DirectionalLightBuffer;
    DynamicStorageBuffer PointLightBuffer;
    DynamicStorageBuffer SpotlightBuffer;

    uint32_t vertexBufferSize = 0;
    uint32_t indexBufferSize = 0;
    uint32_t bvhBufferSize = 0;
    uint32_t partitionBufferSize = 0;
    uint32_t materialBufferSize = 0;

    uint32_t vertexStart = 0;
    uint32_t indexStart = 0;
    uint32_t bvhStart = 0;
    uint32_t materialStart = 0;
    int meshCount;
};