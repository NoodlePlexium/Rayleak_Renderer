#pragma once

#include "mesh.h"

struct Model
{
    uint32_t id;
    std::vector<Mesh*> submeshPtrs;
    char name[32];
    char tempName[32];
    bool inScene = false;
};


class ModelManager
{
public:

    std::vector<Mesh*> meshes;
    std::vector<Model> models;
    std::vector<uint32_t> modelInstances;

    void LoadModel(const char* filepath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath)) {
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
        model.id = models.size();
        Debug::EndTimer();
    }

    int CreateModelInstance(int modelIndex)
    {
        uint32_t instance = static_cast<uint32_t>(modelIndex);
        modelInstances.push_back(instance);
        return modelInstances.size() - 1;
    }
private:

};