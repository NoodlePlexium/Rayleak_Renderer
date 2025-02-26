#pragma once
#include <GL/glew.h>
#include "glm/gtc/matrix_transform.hpp"
#include <string>
#include "shader.h"
#include "mesh.h"
#include "material.h"

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
        LoadSphere("./models/ThumbnailSphere.obj", sphereMesh);

        MVP = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f);
        MVP = glm::rotate(MVP, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // GENERATE BUFFERS
        glGenVertexArrays(1, &thumbnailVAO);
        glGenBuffers(1, &thumbnailVBO);
        glGenBuffers(1, &indexBufferObject);

        // BIND VERTEX ATTRIBUTE BUFFER
        glBindVertexArray(thumbnailVAO);

        // VERTEX BUFFER
        glBindBuffer(GL_ARRAY_BUFFER, thumbnailVBO);
        glBufferData(GL_ARRAY_BUFFER, sphereMesh.vertices.size() * sizeof(float), sphereMesh.vertices.data(), GL_STATIC_DRAW);
        
        // INDEX BUFFER
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereMesh.indices.size() * sizeof(uint32_t), sphereMesh.indices.data(), GL_STATIC_DRAW);
        
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
        glBindFramebuffer(GL_FRAMEBUFFER, material.FBO);
        glViewport(0, 0, width, height);
        glClearColor(0.5f, 0.7f, 0.95f, 1); // SKY COLOUR
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(thumbnailShader);

        // BIND MATERIAL TEXTURES
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

        // RENDER TO FRAME BUFFER
        glBindVertexArray(thumbnailVAO);
        glDrawElements(GL_TRIANGLES, sphereMesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
private:
    unsigned int thumbnailShader, thumbnailVAO, thumbnailVBO, indexBufferObject;
    SimpleMesh sphereMesh;
    glm::mat4 MVP;
};