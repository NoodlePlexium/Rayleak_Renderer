#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"
#include "./glm/glm.hpp"

struct Vertex
{
    float x, y, z;
    float nx, ny, nz;
    float u, v;

    bool operator==(const Vertex& other) const
    {
        return x == other.x &&
            y == other.y &&
            z == other.z &&
            nx == other.nx &&
            ny == other.ny &&
            nz == other.nz && 
            u == other.u &&
            v == other.v;
    }
};

template <typename T, typename... Rest> // from: https://stackoverflow.com/a/57595105
void HashCombine(std::size_t& seed, const T& v, const Rest&... rest) 
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (HashCombine(seed, rest), ...);
};

struct VertexHasher 
{
    std::size_t operator()(const Vertex& v) const 
    {
        std::size_t seed = 0;
        HashCombine(seed, v.x, v.y, v.z, v.nx, v.ny, v.nz, v.u, v.v);
        return seed;
    }
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    void LoadOBJ(const std::string& filepath)
    {
        if (vertices.size() > 0 || indices.size() > 0){
            throw std::runtime_error("[LoabODJ] Error! Cannot call <LoadOBJ> on mesh with vertices!");
        }

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices{};
        for (const auto &shape : shapes){
            for (const auto &index : shape.mesh.indices){
                Vertex vertex{};

                if (index.vertex_index >= 0)
                {
                    vertex.x = attrib.vertices[3 * index.vertex_index];
                    vertex.y = attrib.vertices[3 * index.vertex_index + 1];
                    vertex.z = attrib.vertices[3 * index.vertex_index + 2];
                }

                if (index.normal_index >= 0)
                {
                    vertex.nx = attrib.normals[3 * index.normal_index];
                    vertex.ny = attrib.normals[3 * index.normal_index + 1];
                    vertex.nz = attrib.normals[3 * index.normal_index + 2];
                }

                if (index.texcoord_index >= 0)
                {
                    vertex.u = attrib.texcoords[2 * index.texcoord_index];
                    vertex.v = attrib.texcoords[2 * index.texcoord_index + 1];
                }

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }
};
