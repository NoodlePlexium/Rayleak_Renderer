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

    ~DynamicStorageBuffer()
    {
        std::cout << "deleting buffer" << std::endl;
        glDeleteBuffers(1, &bufferID);
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

    void DeleteBuffer()
    {
        glDeleteBuffers(1, &bufferID);
    }

    uint32_t BufferSize()
    {
        return bufferSize;
    }

private:
    int _binding;
    unsigned int bufferID;
    uint32_t bufferSize;
};

class ModelManager
{
public:

    ModelManager(unsigned int _pathtraceShader) : 
        pathtraceShader(_pathtraceShader),
        VertexBuffer(DynamicStorageBuffer(2, 0)),
        IndexBuffer(DynamicStorageBuffer(3, 0)),
        MaterialBuffer(DynamicStorageBuffer(4, 0)),
        BvhBuffer(DynamicStorageBuffer(5, 0)),
        PartitionBuffer(DynamicStorageBuffer(6, 0)),
        DirectionalLightBuffer(DynamicStorageBuffer(8, 0)),
        PointLightBuffer(DynamicStorageBuffer(9, 0)),
        SpotlightBuffer(DynamicStorageBuffer(10, 0)),
        meshCount(0)
    {

    }

    void AddModelToScene(Model& model)
    {
        glUseProgram(pathtraceShader);

        model.inScene = true;
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
        void* mappedVertexBuffer = VertexBuffer.GetMappedBuffer(VertexBuffer.BufferSize() - appendVertexBufferSize, appendVertexBufferSize);
        void* mappedIndexBuffer = IndexBuffer.GetMappedBuffer(IndexBuffer.BufferSize() - appendIndexBufferSize, appendIndexBufferSize);
        void* mappedBvhBuffer = BvhBuffer.GetMappedBuffer(BvhBuffer.BufferSize() - appendBvhBufferSize, appendBvhBufferSize);
        void* mappedPartitionBuffer = PartitionBuffer.GetMappedBuffer(PartitionBuffer.BufferSize() - appendPartitionBufferSize, appendPartitionBufferSize);

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

    void AddDirectionalLightToScene(DirectionalLight& directionalLight, size_t directionalLightCount)
    {
        // GET DIRECTIONAL LIGHT SIZE
        uint32_t directionalLightSize = sizeof(DirectionalLight);

        // GROW BUFFER TO ACCOMODATE NEW DIRECTIONAL LIGHT
        DirectionalLightBuffer.GrowBuffer(directionalLightSize);

        // GET MAPPED BUFFER
        void* mappedDirectionalLightBuffer = DirectionalLightBuffer.GetMappedBuffer(DirectionalLightBuffer.BufferSize() - directionalLightSize, directionalLightSize);

        // COPY LIGHT TO THE GPU
        memcpy((char*)mappedDirectionalLightBuffer, &directionalLight, directionalLightSize);

        // UNMAP BUFFER
        DirectionalLightBuffer.UnmapBuffer();

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLightCount);
    }

    void AddPointLightToScene(PointLight& pointLight, size_t pointLightCount)
    {
        // GET DIRECTIONAL LIGHT SIZE
        uint32_t pointLightSize = sizeof(PointLight);

        // GROW BUFFER TO ACCOMODATE NEW DIRECTIONAL LIGHT
        PointLightBuffer.GrowBuffer(pointLightSize);

        // GET MAPPED BUFFER
        void* mappedPointLightBuffer = PointLightBuffer.GetMappedBuffer(PointLightBuffer.BufferSize() - pointLightSize, pointLightSize);

        // COPY LIGHT TO THE GPU
        memcpy((char*)mappedPointLightBuffer, &pointLight, pointLightSize);

        // UNMAP BUFFER
        PointLightBuffer.UnmapBuffer();

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLightCount);
    }

    void AddSpotlightToScene(Spotlight& spotlight, size_t spotlightCount)
    {
        // GET DIRECTIONAL LIGHT SIZE
        uint32_t spotlightSize = sizeof(Spotlight);

        // GROW BUFFER TO ACCOMODATE NEW DIRECTIONAL LIGHT
        SpotlightBuffer.GrowBuffer(spotlightSize);

        // GET MAPPED BUFFER
        void* mappedSpotlightBuffer = SpotlightBuffer.GetMappedBuffer(SpotlightBuffer.BufferSize() - spotlightSize, spotlightSize);

        // COPY LIGHT TO THE GPU
        memcpy((char*)mappedSpotlightBuffer, &spotlight, spotlightSize);

        // UNMAP BUFFER
        SpotlightBuffer.UnmapBuffer();

        // UPDATE UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlightCount);
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

    void UpdateMeshMaterial(Mesh* mesh, uint32_t meshIndex, uint32_t materialIndex)
    {       
        // UPDATE MESH MATERIAL INDEX
        mesh->materialIndex = materialIndex;

        // CALCULATE BUFFER OFFSET
        uint32_t bufferOffset = meshIndex * sizeof(MeshPartition) + 2 * sizeof(uint32_t);

        // GET MAPPED BUFFER
        void* mappedPartitionBuffer = PartitionBuffer.GetMappedBuffer(bufferOffset, sizeof(uint32_t));

        // COPY NEW PARTITION BUFFER DATA
        memcpy((char*)mappedPartitionBuffer, &mesh->materialIndex, sizeof(uint32_t));

        // UNMAP BUFFER
        PartitionBuffer.UnmapBuffer();
    }

private:

    DynamicStorageBuffer VertexBuffer;
    DynamicStorageBuffer IndexBuffer;
    DynamicStorageBuffer MaterialBuffer;
    DynamicStorageBuffer BvhBuffer;
    DynamicStorageBuffer PartitionBuffer;
    DynamicStorageBuffer DirectionalLightBuffer;
    DynamicStorageBuffer PointLightBuffer;
    DynamicStorageBuffer SpotlightBuffer;

    uint32_t vertexStart = 0;
    uint32_t indexStart = 0;
    uint32_t bvhStart = 0;
    uint32_t materialStart = 0;
    int meshCount;

    unsigned int pathtraceShader;
};