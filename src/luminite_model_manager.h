#pragma once 

// EXTERNAL LIBRARIES
#include <GL/glew.h>

// STANDARD LIBRARY
#include <vector>
#include <string>

// PROJECT HEADERS
#include "luminite_mesh.h"


struct EmissiveTriangle
{
    uint32_t index;
    uint32_t materialIndex;
    float area;
    float weight;
};

// FROM STACK OVERFLOW USER Greg https://math.stackexchange.com/questions/128991/how-to-calculate-the-area-of-a-3d-triangle
float TriangleArea(const glm::vec3 &v1, const glm::vec3 &v2, const glm::vec3 &v3)
{
    glm::vec3 v1v2 = v2 - v1;
    glm::vec3 v1v3 = v3 - v1;
    glm::vec3 orthogonal = glm::cross(v1v2, v1v3);
    float area = glm::length(orthogonal) * 0.5f;
    return area;
}

class ModelManager
{
public:

    void CopyMeshDataToGPU(const std::vector<Mesh*> &meshes, const std::vector<Material> &materials, unsigned int &pathtraceShader)
    {

        // CREATE BUFFER OF MESH PARTITIONS
        std::vector<MeshPartition> meshPartitions;
        uint32_t vertexStart = 0;
        uint32_t indexStart = 0;
        uint32_t bvhStart = 0;
        for (Mesh* mesh : meshes) 
        {
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
        }

        // CREATE VERTEX, INDEX, MATERIAL, BVH BUFFERS
        size_t vertexBufferSize = 0;
        size_t indexBufferSize = 0;
        size_t materialBufferSize = 0;
        size_t bvhBufferSize = 0;
        size_t emissiveBufferSize = 0;
        uint32_t emissiveTriangleCount;
        for (const Mesh* mesh : meshes) {
            vertexBufferSize += mesh->vertices.size() * sizeof(Vertex);
            indexBufferSize += mesh->indices.size() * sizeof(uint32_t);
            materialBufferSize += sizeof(Material);
            bvhBufferSize += mesh->nodesUsed * sizeof(BVH_Node);
            if (materials[mesh->materialIndex].emission > 0.0f) emissiveTriangleCount += mesh->indices.size() / 3;
        }

        // CREATE BUFFER OF EMISSIVE TRIANGLE STRUCTS
        std::vector<EmissiveTriangle> emissiveTriangleBuffer;
        emissiveTriangleBuffer.reserve(emissiveTriangleCount);
        float totalEmissiveArea = 0.0f;
        indexStart = 0;
        for (const Mesh* mesh : meshes) {
            if (materials[mesh->materialIndex].emission > 0.0f)
            {
                for (int i=0; i<mesh->indices.size(); i+=3) {
                    EmissiveTriangle eTri;
                    const Vertex &v1 = mesh->vertices[mesh->indices[i]];
                    const Vertex &v2 = mesh->vertices[mesh->indices[i+1]];
                    const Vertex &v3 = mesh->vertices[mesh->indices[i+2]];
                    eTri.area = TriangleArea(v1.pos, v2.pos, v3.pos);
                    eTri.materialIndex = mesh->materialIndex; // MATERIAL INDEX
                    totalEmissiveArea += eTri.area;
                    eTri.index = indexStart + i;              // INDICE INDEX
                    emissiveTriangleBuffer.push_back(eTri);
                }
            }
            indexStart += mesh->indices.size();
        }
        for (int i=0; i<emissiveTriangleBuffer.size(); ++i) {
            emissiveTriangleBuffer[i].weight = (emissiveTriangleBuffer[i].area / totalEmissiveArea) * emissiveTriangleCount;
        }
        glUseProgram(pathtraceShader);
        glUniform1i(glGetUniformLocation(pathtraceShader, "u_meshCount"), meshes.size());

        // VERTEX BUFFER
        glGenBuffers(1, &vertexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, vertexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vertexBuffer);
        void* mappedVertexBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, vertexBufferSize, GL_MAP_WRITE_BIT);

        // INDEX BUFFER
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, indexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indexBuffer);
        void* mappedIndexBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, indexBufferSize, GL_MAP_WRITE_BIT);

        // MATERIAL BUFFER
        glGenBuffers(1, &materialBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, materialBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, materialBuffer);
        void* mappedMaterialBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, materialBufferSize, GL_MAP_WRITE_BIT);

        // BVH BUFFER
        glGenBuffers(1, &bvhBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bvhBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bvhBuffer);
        void* mappedBVHBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bvhBufferSize, GL_MAP_WRITE_BIT);

        // PARTITION BUFFER
        glGenBuffers(1, &partitionBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, partitionBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MeshPartition) * meshPartitions.size(), meshPartitions.data(), GL_STATIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, partitionBuffer);

        // EMISSION INDEX BUFFER
        glGenBuffers(1, &emissiveBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, emissiveBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(EmissiveTriangle) * emissiveTriangleCount, emissiveTriangleBuffer.data(), GL_STATIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, emissiveBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_emissiveTriangleCount"), emissiveTriangleCount);

        // COPY VERTEX INDEX AND MATERIAL DATA TO THE GPU
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        uint32_t bvhOffset = 0;
        for (const Mesh* mesh : meshes) 
        {
            memcpy((char*)mappedVertexBuffer + vertexOffset, mesh->vertices.data(),
                mesh->vertices.size() * sizeof(Vertex));
            memcpy((char*)mappedIndexBuffer + indexOffset, mesh->indices.data(),
                mesh->indices.size() * sizeof(uint32_t));
            memcpy((char*)mappedBVHBuffer + bvhOffset, mesh->bvhNodes,
                mesh->nodesUsed * sizeof(BVH_Node));

            vertexOffset += mesh->vertices.size() * sizeof(Vertex);
            indexOffset += mesh->indices.size() * sizeof(uint32_t);
            bvhOffset += mesh->nodesUsed * sizeof(BVH_Node);
        }

        // COPY MATERIALS TO THE GPU
        
        // COPY MATERIALS TO THE GPU
        uint32_t materialOffset = 0;
        for (const Material &material : materials)
        {
            memcpy((char*)mappedMaterialBuffer + materialOffset, &material,
                sizeof(Material));
            materialOffset += sizeof(Material);
        }
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);  
    }

    ~ModelManager()
    {
        glDeleteBuffers(1, &vertexBuffer);
        glDeleteBuffers(1, &indexBuffer);
        glDeleteBuffers(1, &materialBuffer);
        glDeleteBuffers(1, &bvhBuffer);
        glDeleteBuffers(1, &partitionBuffer);
        glDeleteBuffers(1, &emissiveBuffer);
    }
private:
    unsigned int vertexBuffer;
    unsigned int indexBuffer;
    unsigned int materialBuffer;
    unsigned int bvhBuffer;
    unsigned int partitionBuffer;
    unsigned int emissiveBuffer;
};