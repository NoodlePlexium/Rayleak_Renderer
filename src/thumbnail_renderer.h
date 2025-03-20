#pragma once
#include <GL/glew.h>
#include "../lib/glm/gtc/matrix_transform.hpp"
#include <string>
#include "shader.h"
#include "mesh.h"
#include "material.h"


struct ThumbnailMesh
{
    std::vector<float> vertices;
    std::vector<uint32_t>indices;
};

class ThumbnailRenderer
{
public:
    ThumbnailRenderer()
    {
        // LOAD SHADER
        std::string vertexShaderSource = LoadShaderFromFile("./shaders/thumbnail_vert.shader");
        std::string fragmentShaderSource = LoadShaderFromFile("./shaders/thumbnail_frag.shader");
        thumbnailShader = CreateRasterShader(vertexShaderSource, fragmentShaderSource);

        // LOAD SPHERE MODEL
        LoadThumbnailMesh("./models/ThumbnailSphere.obj", thumbnailMesh);

        // MODEL VIEW PROJECTION MATRIX
        MVP = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 1.0f, -1.0f);
        MVP = glm::rotate(MVP, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // GENERATE BUFFERS
        glGenVertexArrays(1, &vertexAttributeObject);
        glGenBuffers(1, &vertexBufferObject);
        glGenBuffers(1, &indexBufferObject);

        // BIND VERTEX ATTRIBUTE BUFFER
        glBindVertexArray(vertexAttributeObject);

        // VERTEX BUFFER
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
        glBufferData(GL_ARRAY_BUFFER, thumbnailMesh.vertices.size() * sizeof(float), thumbnailMesh.vertices.data(), GL_STATIC_DRAW);
        
        // INDEX BUFFER
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, thumbnailMesh.indices.size() * sizeof(uint32_t), thumbnailMesh.indices.data(), GL_STATIC_DRAW);
        
        // VERTEX ATTRIBUTE BUFFER  
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // UNBIND VERTEX ATTRIBUTE BUFFER
        glBindVertexArray(0);
    }

    void RenderThumbnail(Material& material, int width, int height)
    {
        // BIND THUMBNAIL FRAME BUFFER
        glBindFramebuffer(GL_FRAMEBUFFER, material.FBO);

        // THUMBNAIL VIEWPORT
        glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 1); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // BIND MATERIAL TEXTURES
        glUseProgram(thumbnailShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.albedoID);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, material.normalID);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.roughnessID);

        // TEXTURE BINDING LOCATIONS
        glUniform1i(glGetUniformLocation(thumbnailShader, "albedoTexture"), 0);
        glUniform1i(glGetUniformLocation(thumbnailShader, "normalTexture"), 1);
        glUniform1i(glGetUniformLocation(thumbnailShader, "roughnessTexture"), 2);

        // MATERIAL UNIFORMS
        glUniform3f(glGetUniformLocation(thumbnailShader, "material.colour"), material.data.colour.x, material.data.colour.y, material.data.colour.z);
        glUniform1f(glGetUniformLocation(thumbnailShader, "material.roughness"), material.data.roughness);
        glUniform1f(glGetUniformLocation(thumbnailShader, "material.emission"), material.data.emission);
        glUniform1f(glGetUniformLocation(thumbnailShader, "material.IOR"), material.data.IOR);
        glUniform1i(glGetUniformLocation(thumbnailShader, "material.refractive"), material.data.refractive);
        glUniform1ui64ARB(glGetUniformLocation(thumbnailShader, "material.albedoHandle"), material.data.albedoHandle);
        glUniform1ui64ARB(glGetUniformLocation(thumbnailShader, "material.normalHandle"), material.data.normalHandle);
        glUniform1ui64ARB(glGetUniformLocation(thumbnailShader, "material.roughnessHandle"), material.data.roughnessHandle);
        glUniform1ui(glGetUniformLocation(thumbnailShader, "material.textureFlags"), material.data.textureFlags);

        // MODEL VIEW PROJECTION UNIFORM
        glUniformMatrix4fv(glGetUniformLocation(thumbnailShader, "u_MVP"), 1, GL_FALSE, &MVP[0][0]);

        // BACKFACE CULLING
        glFrontFace(GL_CW);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // RENDER TO FRAME BUFFER
        glBindVertexArray(vertexAttributeObject);
        glDrawElements(GL_TRIANGLES, thumbnailMesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
private:
    unsigned int thumbnailShader, vertexAttributeObject, vertexBufferObject, indexBufferObject;
    ThumbnailMesh thumbnailMesh;
    glm::mat4 MVP;

    void LoadThumbnailMesh(const char* filepath, ThumbnailMesh& mesh)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath)) {
            throw std::runtime_error(warn + err);
        }
        
        mesh.vertices.reserve(shapes[0].mesh.indices.size() * 8); 
        mesh.indices.reserve(shapes[0].mesh.indices.size());
        uint32_t indicesUsed = 0;

        for (const auto &index : shapes[0].mesh.indices)
        {
            if (index.vertex_index >= 0)
            {
                mesh.vertices.emplace_back(attrib.vertices[3 * index.vertex_index]);
                mesh.vertices.emplace_back(attrib.vertices[3 * index.vertex_index + 1]);
                mesh.vertices.emplace_back(attrib.vertices[3 * index.vertex_index + 2]);
            }

            if (index.normal_index >= 0)
            {
                mesh.vertices.emplace_back(attrib.normals[3 * index.normal_index]);
                mesh.vertices.emplace_back(attrib.normals[3 * index.normal_index + 1]);
                mesh.vertices.emplace_back(attrib.normals[3 * index.normal_index + 2]);
            }

            if (index.texcoord_index >= 0)
            {
                mesh.vertices.emplace_back(attrib.texcoords[2 * index.texcoord_index]);
                mesh.vertices.emplace_back(attrib.texcoords[2 * index.texcoord_index + 1]);
            }
            mesh.indices.emplace_back(indicesUsed);
            indicesUsed += 1;
        }
        mesh.vertices.resize(mesh.vertices.size());
    }
};