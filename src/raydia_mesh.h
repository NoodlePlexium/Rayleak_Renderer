#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"
#include "glm/glm.hpp"
#include "raydia_debug.h"

struct MeshPartition
{
    uint32_t verticesStart;
    uint32_t indicesStart;
    uint32_t materialIndex;
    uint32_t bvhNodeStart;
};

struct BVH_Node
{
    alignas(16) glm::vec3 aabbMin;
    alignas(16) glm::vec3 aabbMax;
    uint32_t leftChild, rightChild;
    uint32_t firstIndex, indexCount;
    BVH_Node() : leftChild(0), rightChild(0), firstIndex(0), indexCount(0) {}
};

struct Material
{
    alignas(16) glm::vec3 colour;
    float roughness;
    float emission;

    Material()
    {
        colour = glm::vec3(0.8f, 0.8f, 0.8f);
        roughness = 0.5f;
        emission = 0.0f;
    }
};

struct Vertex
{
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec3 normal;
    float u, v;

    bool operator==(const Vertex& other) const
    {
        return pos.x == other.pos.x &&
            pos.y == other.pos.y &&
            pos.z == other.pos.z &&
            normal.x == other.normal.x &&
            normal.y == other.normal.y && 
            normal.z == other.normal.z && 
            u == other.u &&
            v == other.v;
    }
};

// adapted from: https://stackoverflow.com/a/57595105
template <typename T, typename... Rest> 
void HashCombine(std::size_t& seed, const T& v, const Rest&... rest) 
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    ((seed ^= std::hash<Rest>{}(rest) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ... );
}

struct VertexHasher 
{
    std::size_t operator()(const Vertex& v) const 
    {
        std::size_t seed = 0;
        HashCombine(seed, v.pos.x, v.pos.y, v.pos.z, v.normal.x, v.normal.y, v.normal.z, v.u, v.v);
        return seed;
    }
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Material material;
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    BVH_Node* bvhNodes;
    uint32_t nodesUsed = 1;

    void BuildBVH()
    {
        const uint32_t nodeCount = (indices.size() / 3) * 2 - 1;
        bvhNodes = new BVH_Node[nodeCount];
        BVH_Node& root = bvhNodes[0];
        root.indexCount = indices.size();
        UpdateNodeBounds(0);
        SubdivideNode(0, 0);

        // RESIZE bvhNodes TO DISCARD UNUSED NODES
        BVH_Node* resizedNodes = new BVH_Node[nodesUsed];  
        std::memcpy(resizedNodes, bvhNodes, nodesUsed * sizeof(BVH_Node));
        delete[] bvhNodes;
        bvhNodes = resizedNodes;
    }

    void UpdateNodeBounds(uint32_t nodeIndex)
    {
        BVH_Node& node = bvhNodes[nodeIndex];
        node.aabbMin = glm::vec3(1e30f);
        node.aabbMax = glm::vec3(-1e30f);
        for (uint32_t i=0; i<node.indexCount; i+=3)
        {
            Vertex& v1 = vertices[indices[node.firstIndex + i]];
            Vertex& v2 = vertices[indices[node.firstIndex + i+1]];
            Vertex& v3 = vertices[indices[node.firstIndex + i+2]];
            node.aabbMin = glm::min(node.aabbMin, v1.pos);
            node.aabbMin = glm::min(node.aabbMin, v2.pos);
            node.aabbMin = glm::min(node.aabbMin, v3.pos);
            node.aabbMax = glm::max(node.aabbMax, v1.pos);
            node.aabbMax = glm::max(node.aabbMax, v2.pos);
            node.aabbMax = glm::max(node.aabbMax, v3.pos);
        }
    }

    void SubdivideNode(uint32_t nodeIndex, uint16_t recurse)
    {
        BVH_Node& node = bvhNodes[nodeIndex];
        if (node.indexCount <= 24 || recurse > 32) return;

        // CALCULATE SPLIT AXIS AND POS
        glm::vec3 nodeBoxDimensions = node.aabbMax - node.aabbMin;
        uint32_t axis = 0;
        if (nodeBoxDimensions.y > nodeBoxDimensions.x) axis = 1;
        if (nodeBoxDimensions.z > nodeBoxDimensions[axis]) axis = 2;
        float splitPos = node.aabbMin[axis] + nodeBoxDimensions[axis] * 0.5f;


        // ARRANGE INDICES ABOUT THE SPLIT POS
        int i = node.firstIndex;
        int j = i + node.indexCount - 3;
        while (i <= j)
        {   
            glm::vec3 centroid = (vertices[indices[i]].pos + vertices[indices[i + 1]].pos + vertices[indices[i + 2]].pos) * 0.33333f;

            if (centroid[axis] < splitPos) i+=3;
            else
            {
                std::swap(indices[i], indices[j]);
                std::swap(indices[i+1], indices[j+1]);
                std::swap(indices[i+2], indices[j+2]);
                j-=3;
            }
        }

        // IF A SPLIT CHILD HAS NO VERTICES
        uint32_t leftIndexCount = i - node.firstIndex;
        if (leftIndexCount == 0 || leftIndexCount == node.indexCount){
            return;
        }

        // SET NODE ATTRIBUTES
        uint32_t leftChildIndex = nodesUsed++;
        uint32_t rightChildIndex = nodesUsed++;
        bvhNodes[leftChildIndex].firstIndex = node.firstIndex;
        bvhNodes[leftChildIndex].indexCount = leftIndexCount;
        bvhNodes[rightChildIndex].firstIndex = i;
        bvhNodes[rightChildIndex].indexCount = node.indexCount - leftIndexCount;
        node.leftChild = leftChildIndex;
        node.rightChild = rightChildIndex;
        node.indexCount = 0;

        // RECURSIVE CALL FOT LEFT AND RIGHT SUB NODES
        UpdateNodeBounds(leftChildIndex);
        UpdateNodeBounds(rightChildIndex);
        SubdivideNode(leftChildIndex, recurse+1);
        SubdivideNode(rightChildIndex, recurse+1);
    }

    size_t GetSize() const 
    {
        return vertices.size() * sizeof(Vertex) + indices.size() * sizeof(indices);
    }
};


void LoadOBJ(const std::string& filepath, std::vector<Mesh*>& meshes)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error(warn + err);
    }

    // FOR EACH MESH IN THE FILE
    for (const auto &shape : shapes)
    {
        if (shape.mesh.indices.size() == 0) continue;

        Mesh* mesh = new Mesh();  // Allocate mesh on the heap
        mesh->name = shape.name;
        mesh->vertices.reserve(shape.mesh.indices.size()); 
        mesh->indices.reserve(shape.mesh.indices.size());
        uint32_t verticesUsed = 0;
        uint32_t indicesUsed = 0;
        std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices{};

        for (const auto &index : shape.mesh.indices)
        {
            Vertex vertex{};

            if (index.vertex_index >= 0)
            {
                vertex.pos = glm::vec3(
                    attrib.vertices[3 * index.vertex_index],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]);
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

            Debug::ResumeAccum();
            if (uniqueVertices.count(vertex) == 0) 
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(mesh->vertices.size());
                mesh->vertices.emplace_back(vertex);
                verticesUsed += 1;
            }

            mesh->indices.emplace_back(uniqueVertices[vertex]);
        }
        mesh->vertices.resize(verticesUsed);
        mesh->BuildBVH();
        meshes.push_back(mesh);  
    }
}


