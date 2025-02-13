#pragma once 

// EXTERNAL LIBRARIES
#include <GL/glew.h>

// STANDARD LIBRARY
#include <vector>
#include <string>

// PROJECT HEADERS
#include "luminite_mesh.h"
#include "luminite_light.h"


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

    void CopyMeshDataToGPU(std::vector<Mesh*> &meshes, const std::vector<Material> &materials, const unsigned int &pathtraceShader)
    {   
        // BUILD SCENE BVH
        // BuildSceneBVH(meshes);


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
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, vertexBuffer);
        void* mappedVertexBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, vertexBufferSize, GL_MAP_WRITE_BIT);

        // INDEX BUFFER
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, indexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indexBuffer);
        void* mappedIndexBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, indexBufferSize, GL_MAP_WRITE_BIT);

        // MATERIAL BUFFER
        glGenBuffers(1, &materialBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, materialBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, materialBuffer);
        void* mappedMaterialBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, materialBufferSize, GL_MAP_WRITE_BIT);

        // MESH BVH BUFFER
        glGenBuffers(1, &bvhBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bvhBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, bvhBuffer);
        void* mappedBVHBuffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bvhBufferSize, GL_MAP_WRITE_BIT);
       
        // SCENE BVH BUFFER
        glGenBuffers(1, &sceneBvhBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sceneBvhBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, nodesUsed * sizeof(BVH_Node), sceneBvhNodes, GL_STATIC_READ);  
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, sceneBvhBuffer);
        
        // PARTITION BUFFER
        glGenBuffers(1, &partitionBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, partitionBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MeshPartition) * meshPartitions.size(), meshPartitions.data(), GL_STATIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, partitionBuffer);

        // EMISSION INDEX BUFFER
        glGenBuffers(1, &emissiveBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, emissiveBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(EmissiveTriangle) * emissiveTriangleCount, emissiveTriangleBuffer.data(), GL_STATIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, emissiveBuffer);
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
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, directionalLightBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLights.size());

        // POINT LIGHTS
        glGenBuffers(1, &pointLightBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointLightBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLight) * pointLights.size(), pointLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, pointLightBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLights.size());

        // SPOTLIGHTS
        glGenBuffers(1, &spotlightBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, spotlightBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Spotlight) * spotlights.size(), spotlights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, spotlightBuffer);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlights.size());
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

    void BuildSceneBVH(std::vector<Mesh*> &meshes)
    {
        sceneBvhNodes = new BVH_Node[meshes.size()];
        BVH_Node& root = sceneBvhNodes[0];
        root.indexCount = meshes.size();
        UpdateNodeBounds(0, meshes);
        SubdivideNode(0, 0, meshes);

        // RESIZE bvhNodes TO DISCARD UNUSED NODES
        BVH_Node* resizedNodes = new BVH_Node[nodesUsed];  
        std::memcpy(resizedNodes, sceneBvhNodes, nodesUsed * sizeof(BVH_Node));
        delete[] sceneBvhNodes;
        sceneBvhNodes = resizedNodes;
    }

    void UpdateNodeBounds(uint32_t nodeIndex, std::vector<Mesh*> &meshes)
    {
        BVH_Node& node = sceneBvhNodes[nodeIndex];
        node.aabbMin = glm::vec3(1e30f);
        node.aabbMax = glm::vec3(-1e30f);
        for (uint32_t i=0; i<node.indexCount; ++i)
        {
            node.aabbMin = glm::min(node.aabbMin, meshes[node.firstIndex + i]->aabbMin);
            node.aabbMax = glm::max(node.aabbMax, meshes[node.firstIndex + i]->aabbMax);
        }
    }

    void SubdivideNode(uint32_t nodeIndex, uint16_t recurse, std::vector<Mesh*> &meshes)
    {
        BVH_Node& node = sceneBvhNodes[nodeIndex];
        if (node.indexCount == 1 || recurse > 16) return;

        glm::vec3 nodeBoxDimensions = node.aabbMax - node.aabbMin;
        uint32_t axis = 0;
        if (nodeBoxDimensions.y > nodeBoxDimensions.x) axis = 1;
        if (nodeBoxDimensions.z > nodeBoxDimensions[axis]) axis = 2;
        float splitPos = (node.aabbMax.x + node.aabbMin.x) * 0.5f;

        // ARRANGE INDICES ABOUT THE SPLIT POS
        int i = node.firstIndex;
        int j = i + node.indexCount - 1;
        while (i <= j)
        {
            glm::vec3 centroid = (meshes[i]->aabbMax + meshes[i]->aabbMin) * 0.5f;

            if (centroid[axis] < splitPos) i++;
            else
            {
                std::swap(meshes[i], meshes[j]);
                j--;
            }
        }

        // IF A SPLIT HAS NO VERTICES
        uint32_t leftIndexCount = i - node.firstIndex;
        if (leftIndexCount == 0 || leftIndexCount == node.indexCount){
            return;
        }

        // SEY NODE ATTRIBUTES
        uint32_t leftChildIndex = nodesUsed++;
        uint32_t rightChildIndex = nodesUsed++;
        sceneBvhNodes[leftChildIndex].firstIndex = node.firstIndex;
        sceneBvhNodes[leftChildIndex].indexCount = leftIndexCount;
        sceneBvhNodes[rightChildIndex].firstIndex = i; 
        sceneBvhNodes[rightChildIndex].indexCount = node.indexCount - leftIndexCount; 
        node.leftChild = leftChildIndex;
        node.rightChild = rightChildIndex;
        node.indexCount = 0;

        // RECURSIVE CALL FOT LEFT AND RIGHT SUB NODES
        UpdateNodeBounds(leftChildIndex, meshes);
        UpdateNodeBounds(rightChildIndex, meshes);
        SubdivideNode(leftChildIndex, recurse+1, meshes);
        SubdivideNode(rightChildIndex, recurse+1, meshes);
    }

    BVH_Node* sceneBvhNodes;
    uint32_t nodesUsed;

    unsigned int sceneBvhBuffer;
    unsigned int vertexBuffer;
    unsigned int indexBuffer;
    unsigned int materialBuffer;
    unsigned int bvhBuffer;
    unsigned int partitionBuffer;
    unsigned int emissiveBuffer;
    unsigned int directionalLightBuffer;
    unsigned int pointLightBuffer;
    unsigned int spotlightBuffer;
};