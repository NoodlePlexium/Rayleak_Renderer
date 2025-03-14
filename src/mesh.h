#pragma once

// EXTERNAL LIBRARIES
#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

// STANDARD LIBRARY
#include <vector>
#include <functional>
#include <unordered_map>
#include <cmath>
#include <chrono>
#include <omp.h>

// PROJECT HEADERS
#include "debug.h"
#include "material.h"
#include "utils.h"

struct MeshPartition
{
    uint32_t verticesStart;
    uint32_t indicesStart;
    uint32_t materialIndex;
    uint32_t bvhNodeStart;
    glm::mat4 inverseTransform;
};

struct BVH_Node
{
    alignas(16) glm::vec3 aabbMin;
    alignas(16) glm::vec3 aabbMax;
    uint32_t leftChild, rightChild;
    uint32_t firstIndex, indexCount;
    BVH_Node() : leftChild(0), rightChild(0), firstIndex(0), indexCount(0) {}
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

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::mat4 inverseTransform;

    std::string name;

    BVH_Node* bvhNodes;
    uint32_t nodesUsed = 1;
    uint32_t materialIndex = 0;

    glm::vec3 aabbMin;
    glm::vec3 aabbMax;

    void Init()
    {
        position = glm::vec3(0.0f, 0.0f, 0.0f);
        rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        scale = glm::vec3(1.0f, 1.0f, 1.0f);
    }

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
            const Vertex &v1 = vertices[indices[node.firstIndex + i]];
            const Vertex &v2 = vertices[indices[node.firstIndex + i+1]];
            const Vertex &v3 = vertices[indices[node.firstIndex + i+2]];
            glm::vec3 vertexMin = glm::min(v1.pos, glm::min(v2.pos, v3.pos));
            glm::vec3 vertexMax = glm::max(v1.pos, glm::max(v2.pos, v3.pos));
            node.aabbMin = glm::min(node.aabbMin, vertexMin);
            node.aabbMax = glm::max(node.aabbMax, vertexMax);
        }
    }

    float HalfAreaAABB(const glm::vec3 &aabbMin, const glm::vec3 &aabbMax)
    {
        glm::vec3 dims = aabbMax - aabbMin;
        return dims.x * dims.y + dims.y * dims.z + dims.z * dims.x;
    }

    float EvaluateSAH(const BVH_Node& node, uint8_t axis, float pos)
    {
        glm::vec3 left_AABB_min(1e30f), left_AABB_max(-1e30f);
        glm::vec3 right_AABB_min(1e30f), right_AABB_max(-1e30f);
        int leftCount = 0;
        int rightCount = 0;
        for (uint32_t i=0; i<node.indexCount; i+=3)
        {
            const Vertex &v1 = vertices[indices[node.firstIndex + i]];
            const Vertex &v2 = vertices[indices[node.firstIndex + i + 1]];
            const Vertex &v3 = vertices[indices[node.firstIndex + i + 2]];
            glm::vec3 triangle_center = (v1.pos + v2.pos + v3.pos) * 0.333f;
            if (triangle_center[axis] < pos)
            {   
                left_AABB_min = glm::min(left_AABB_min, v1.pos);
                left_AABB_min = glm::min(left_AABB_min, v2.pos);
                left_AABB_min = glm::min(left_AABB_min, v3.pos);
                left_AABB_max = glm::max(left_AABB_max, v1.pos);
                left_AABB_max = glm::max(left_AABB_max, v2.pos);
                left_AABB_max = glm::max(left_AABB_max, v3.pos);
                leftCount++;
            }
            else
            {
                right_AABB_min = glm::min(right_AABB_min, v1.pos);
                right_AABB_min = glm::min(right_AABB_min, v2.pos);
                right_AABB_min = glm::min(right_AABB_min, v3.pos);
                right_AABB_max = glm::max(right_AABB_max, v1.pos);
                right_AABB_max = glm::max(right_AABB_max, v2.pos);
                right_AABB_max = glm::max(right_AABB_max, v3.pos);
                rightCount++;
            }
        }
        float cost = leftCount * HalfAreaAABB(left_AABB_min, left_AABB_max) + rightCount * HalfAreaAABB(right_AABB_min, right_AABB_max);
        return cost > 0.0f ? cost : 1e30f;
    }

    int CalculateSplitCount(int max, int min, uint16_t recurse)
    {   
        float recurseT = static_cast<float>(recurse) / 32.0f;
        float range = static_cast<float>(max - min);
        float splits = static_cast<float>(min) + range * recurseT;
        return static_cast<int>(splits);
    }   

    void SubdivideNode(uint32_t nodeIndex, uint16_t recurse)
    {
        BVH_Node& node = bvhNodes[nodeIndex];
        if (node.indexCount <= 12 || recurse > 32) return;

        // DETERMINE BEST SPLIT AXIS AND POS
        int axis = 0;
        float splitPos = 0.0f;
        float lowestCost = 1e30f;
        int splits = CalculateSplitCount(10, 3, recurse); // OPTIMISATION, FIRST BOUNDING BOXES NEED MORE SPLITS THAN LATER ONES
        for (uint8_t ax=0; ax<3; ++ax)
        {
            for (uint32_t i=0; i<=splits; ++i)
            {
                glm::vec3 nodeBoxDimensions = node.aabbMax - node.aabbMin;
                float testPos = node.aabbMin[ax] + nodeBoxDimensions[ax] * (i / static_cast<float>(splits));
                float cost = EvaluateSAH(node, ax, testPos);
                if (cost < lowestCost)
                {
                    lowestCost = cost;
                    axis = ax;
                    splitPos = testPos;
                }
            }
        }

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

    void UpdateInverseTransformMat()
    {
        glm::mat4 transform = glm::mat4(1.0f); // IDENTITY MATRIX
        transform = glm::translate(transform, position); // TRANSLATE
        transform = glm::rotate(transform, glm::radians(rotation.x), glm::vec3(1, 0, 0));  // ROTATE
        transform = glm::rotate(transform, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(rotation.z), glm::vec3(0, 0, 1)); 
        transform = glm::scale(transform, scale); // SCALE
        inverseTransform = glm::inverse(transform); // INVERT
    }
};
