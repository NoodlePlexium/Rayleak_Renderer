#pragma once

// STANDARD LIBRARY
#include <vector>
#include <string>

// PROJECT HEADERS
#include "mesh.h"
#include "gpu_memory_manager.h"

struct Model
{
    uint32_t id;
    std::vector<Mesh*> submeshPtrs;
    std::vector<uint32_t> meshIDs;
    char name[32];
    char tempName[32];
    bool inScene = false;
};


class ModelManager
{
public:

    ModelManager(unsigned int _pathtraceShader) : 
        pathtraceShader(_pathtraceShader),
        VertexBuffer(DynamicPoolBuffer(2, 0)),
        IndexBuffer(DynamicPoolBuffer(3, 0)),
        BvhBuffer(DynamicPoolBuffer(5, 0)),
        PartitionBuffer(DynamicContiguousBuffer(6, 0)),
        meshCount(0)
    {

    }

    std::vector<Mesh*> meshes;
    std::vector<Model> models;
    std::vector<Model> modelInstances;

    void LoadModel(const char* filepath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath)) 
        {
            throw std::runtime_error(warn + err);
        }

        Model model;
        std::string name = ExtractName(filepath).substr(0, 32);
        strcpy_s(model.name, 32, name.c_str());
        strcpy_s(model.tempName, 32, name.c_str());

        // FOR EACH MESH IN THE FILE
        Debug::StartTimer();
        # pragma omp parallel for
        for (const auto &shape : shapes)
        {
            if (shape.mesh.indices.size() == 0) continue;

            Mesh* mesh = new Mesh();  
            mesh->Init();
            mesh->name = shape.name;
            mesh->vertices.reserve(shape.mesh.indices.size()); 
            mesh->indices.reserve(shape.mesh.indices.size());
            uint32_t indicesUsed = 0;

            for (const auto &index : shape.mesh.indices)
            {
                Vertex vertex{};

                if (index.vertex_index >= 0)
                {
                    vertex.pos = glm::vec3(
                        attrib.vertices[3 * index.vertex_index],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]);

                    mesh->aabbMin = glm::min(mesh->aabbMin, vertex.pos);
                    mesh->aabbMax = glm::max(mesh->aabbMax, vertex.pos);
                }

                if (index.normal_index >= 0)
                {
                    vertex.normal = glm::vec3(
                        attrib.normals[3 * index.normal_index],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]);
                }

                if (index.texcoord_index >= 0)
                {
                    vertex.u = attrib.texcoords[2 * index.texcoord_index];
                    vertex.v = attrib.texcoords[2 * index.texcoord_index + 1];
                }
    
                mesh->vertices.emplace_back(vertex);
                mesh->indices.emplace_back(indicesUsed);
                indicesUsed += 1;
            }
            
            mesh->vertices.resize(mesh->vertices.size());
            mesh->BuildBVH();
            meshes.push_back(mesh);  
            model.submeshPtrs.push_back(mesh);
        }
        models.push_back(model);
        Debug::EndTimer();
    }

    void DeleteInstanceMesh(int instanceIndex, int submeshIndex, int meshIndex)
    {   
        Model &modelInstance = modelInstances[instanceIndex];

        uint32_t meshId = modelInstance.meshIDs[submeshIndex];

        // DELETE MESH VERTEX DATA
        VertexBuffer.DeleteItem(meshId);
        
        // DELETE MESH INDEX DATA
        IndexBuffer.DeleteItem(meshId);

        // DELETE MESH BVH DATA
        BvhBuffer.DeleteItem(meshId);

        // DELETE MESH PARTITION DATA
        PartitionBuffer.DeleteShift(meshIndex * sizeof(MeshPartition), sizeof(MeshPartition));

        // DELETE SUBMESH 
        modelInstance.submeshPtrs.erase(modelInstance.submeshPtrs.begin() + submeshIndex);
        modelInstance.meshIDs.erase(modelInstance.meshIDs.begin() + submeshIndex);
        if (modelInstance.submeshPtrs.size() == 0) modelInstances.erase(modelInstances.begin() + instanceIndex);
        meshCount--;

        // UPDATE MESH COUNT UNIFORM
        glUseProgram(pathtraceShader);
        glUniform1i(glGetUniformLocation(pathtraceShader, "u_meshCount"), meshCount);
    }

    int CreateModelInstance(int modelIndex)
    {
        Model instance;
        instance.inScene = false;
        strcpy_s(instance.name, 32, models[modelIndex].name);
        strcpy_s(instance.tempName, 32, models[modelIndex].tempName);
        for (int i=0; i<models[modelIndex].submeshPtrs.size(); i++)
        {
            instance.submeshPtrs.push_back(models[modelIndex].submeshPtrs[i]);
            instance.meshIDs.push_back(meshCount++);
        }
        modelInstances.push_back(instance);
        return modelInstances.size() - 1;
    }

    void AddModelToScene(Model* model)
    {
        glUseProgram(pathtraceShader);
        model->inScene = true;
        glUniform1i(glGetUniformLocation(pathtraceShader, "u_meshCount"), meshCount);
        

        // CALCULATE BUFFER SIZES
        uint32_t appendVertexBufferSize = 0;
        uint32_t appendIndexBufferSize = 0;
        uint32_t appendBvhBufferSize = 0;
        uint32_t appendPartitionBufferSize = model->submeshPtrs.size() * sizeof(MeshPartition);
        for (Mesh* mesh : model->submeshPtrs) 
        {
            // ACCUMULATE BUFFER SPACE USED
            appendVertexBufferSize += mesh->vertices.size() * sizeof(Vertex);
            appendIndexBufferSize += mesh->indices.size() * sizeof(uint32_t);
            appendBvhBufferSize += mesh->nodesUsed * sizeof(BVH_Node);
        }

        // ENSURE THERE IS SPACE AVAILABLE
        if (VertexBuffer.FindAvailableSpace(appendVertexBufferSize) == -1) VertexBuffer.GrowBuffer(appendVertexBufferSize);
        if (IndexBuffer.FindAvailableSpace(appendIndexBufferSize) == -1) IndexBuffer.GrowBuffer(appendIndexBufferSize);
        if (BvhBuffer.FindAvailableSpace(appendBvhBufferSize) == -1) BvhBuffer.GrowBuffer(appendBvhBufferSize);
        
        // GET BUFFER OFFSETS
        Mesh* firstMesh = model->submeshPtrs[0];
        uint32_t firstID = model->meshIDs[0];
        int vertexBufferOffset = VertexBuffer.FindAvailableSpace(firstMesh->vertices.size() * sizeof(Vertex));
        int indexBufferOffset = IndexBuffer.FindAvailableSpace(firstMesh->indices.size() * sizeof(uint32_t));
        int bvhBufferOffset = BvhBuffer.FindAvailableSpace(firstMesh->nodesUsed * sizeof(BVH_Node));

        // CREATE BUFFER PARTITION ITEMS
        for (int i=0; i<model->meshIDs.size(); i++)
        {   
            Mesh* mesh = model->submeshPtrs[i];
            uint32_t id = model->meshIDs[i];
            VertexBuffer.OccupyRegion(mesh->vertices.size() * sizeof(Vertex), id);
            IndexBuffer.OccupyRegion(mesh->indices.size() * sizeof(uint32_t), id);
            BvhBuffer.OccupyRegion(mesh->nodesUsed * sizeof(BVH_Node), id);
        }

        // BUFFER MAPPINGS
        void* mappedVertexBuffer;
        void* mappedIndexBuffer;
        void* mappedBvhBuffer;
        void* mappedPartitionBuffer;
        mappedVertexBuffer = VertexBuffer.GetMappedBuffer(vertexBufferOffset, appendVertexBufferSize);
        mappedIndexBuffer = IndexBuffer.GetMappedBuffer(indexBufferOffset, appendIndexBufferSize);
        mappedBvhBuffer = BvhBuffer.GetMappedBuffer(bvhBufferOffset, appendBvhBufferSize);

        // GET PARTITION BUFFER MAPPING
        PartitionBuffer.GrowBuffer(appendPartitionBufferSize);
        mappedPartitionBuffer = PartitionBuffer.GetMappedBuffer(PartitionBuffer.UsedCapacity() - appendPartitionBufferSize, appendPartitionBufferSize);

        // INIT MESH PARTITIONS
        uint32_t vertexStart = static_cast<uint32_t>(vertexBufferOffset / sizeof(Vertex));
        uint32_t indexStart = static_cast<uint32_t>(indexBufferOffset / sizeof(uint32_t));
        uint32_t bvhStart = static_cast<uint32_t>(bvhBufferOffset / sizeof(BVH_Node));
        std::vector<MeshPartition> meshPartitions;
        for (Mesh* mesh : model->submeshPtrs) 
        {
            // CREATE NEW MESH PARTITION
            MeshPartition mPart;
            mPart.verticesStart = vertexStart;
            mPart.indicesStart = indexStart;
            mPart.materialIndex = 0;
            mPart.bvhNodeStart = bvhStart;
            mesh->UpdateInverseTransformMat();
            mPart.inverseTransform = mesh->inverseTransform;
            vertexStart += mesh->vertices.size();
            indexStart += mesh->indices.size();
            bvhStart += mesh->nodesUsed;
            meshPartitions.push_back(mPart);
        }

        // COPY BUFFER DATA TO GPU
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        uint32_t bvhOffset = 0;
        uint32_t partitionOffset = 0;
        for (const Mesh* mesh : model->submeshPtrs) 
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
    
    void UpdateMeshMaterial(uint32_t meshIndex, uint32_t materialIndex)
    {       
        // CALCULATE BUFFER OFFSET
        uint32_t bufferOffset = meshIndex * sizeof(MeshPartition) + 2 * sizeof(uint32_t);

        // GET MAPPED BUFFER
        void* mappedPartitionBuffer = PartitionBuffer.GetMappedBuffer(bufferOffset, sizeof(uint32_t));

        // COPY NEW PARTITION BUFFER DATA
        memcpy((char*)mappedPartitionBuffer, &materialIndex, sizeof(uint32_t));

        // UNMAP BUFFER
        PartitionBuffer.UnmapBuffer();
    }

    void UpdateMeshTransform(Mesh* mesh, uint32_t meshIndex)
    {
        // CALCULATE BUFFER OFFSET
        uint32_t bufferOffset = meshIndex * sizeof(MeshPartition) + 4 * sizeof(uint32_t);

        // GET MAPPED BUFFER
        void* mappedPartitionBuffer = PartitionBuffer.GetMappedBuffer(bufferOffset, sizeof(glm::mat4));

        // COPY NEW PARTITION BUFFER DATA
        memcpy((char*)mappedPartitionBuffer, glm::value_ptr(mesh->inverseTransform), sizeof(glm::mat4));

        // UNMAP BUFFER
        PartitionBuffer.UnmapBuffer();
    }

    int meshCount;

private:
    // DYNAMIC SHADER STORAGE BUFFERS
    DynamicPoolBuffer VertexBuffer;
    DynamicPoolBuffer IndexBuffer;
    DynamicPoolBuffer BvhBuffer;
    DynamicContiguousBuffer PartitionBuffer;

    // PATH TRACING SHADER ID
    unsigned int pathtraceShader;
};