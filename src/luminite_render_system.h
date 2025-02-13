#pragma once

// EXTERNAL LIBRARIES
#include <GL/glew.h>

// STANDARD LIBRARY
#include <chrono>
#include <queue>
#include <iostream>

// PROJECT HEADERS
#include "luminite_camera.h"
#include "luminite_quad_renderer.h"

struct PathVertex
{
    alignas(16) glm::vec3 surfacePosition;
    alignas(16) glm::vec3 surfaceNormal;
    alignas(16) glm::vec3 surfaceColour;
    alignas(16) glm::vec3 reflectedDir;
    alignas(16) glm::vec3 outgoingLight;
    alignas(16) glm::vec3 directLight;
    float surfaceRoughness;
    float surfaceEmission;
    float IOR;
    int refractive;
    int hitSky;
    int inside;
    int refracted;
    int cachedDirectLight;
};

struct RenderTile
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

class RenderSystem
{
public:

    RenderSystem(int width, int height)
    {   
        VIEWPORT_WIDTH = width;
        VIEWPORT_HEIGHT = height;

        int SCA_W = static_cast<int>(static_cast<float>(VIEWPORT_WIDTH) * resolutionScale);
        int SCA_H = static_cast<int>(static_cast<float>(VIEWPORT_HEIGHT) * resolutionScale);

        qRenderer.PrepareQuadShader();
        qRenderer.CreateFrameBuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        
        // RENDER TEXTURE SETUP
        glGenTextures(1, &RenderTexture);
        glBindTexture(GL_TEXTURE_2D, RenderTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCA_W, SCA_H, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        // DISPLAY TEXTURE SETUP
        glGenTextures(1, &DisplayTexture);
        glBindTexture(GL_TEXTURE_2D, DisplayTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCA_W, SCA_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        // CAMERA PATH BUFFER
        uint32_t cameraPathVertexCount = SCA_W * SCA_H * (cameraBounces+1);
        glGenBuffers(1, &cameraPathVertexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * cameraPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, cameraPathVertexBuffer);

        // LIGHT PATH BUFFER
        uint32_t lightPathVertexCount = SCA_W * SCA_H * (lightBounces+2);
        glGenBuffers(1, &lightPathVertexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * lightPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, lightPathVertexBuffer);
    }

    ~RenderSystem()
    {
        glDeleteBuffers(1, &RenderTexture);
        glDeleteBuffers(1, &DisplayTexture);
        glDeleteBuffers(1, &cameraPathVertexBuffer);
        glDeleteBuffers(1, &lightPathVertexBuffer);
    }

    void ResizeFramebuffer(int width, int height)
    {
        VIEWPORT_WIDTH = width;
        VIEWPORT_HEIGHT = height;

        int SCA_W = static_cast<int>(static_cast<float>(VIEWPORT_WIDTH) * resolutionScale);
        int SCA_H = static_cast<int>(static_cast<float>(VIEWPORT_HEIGHT) * resolutionScale);

        qRenderer.ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

        // CLEAR TEXTURES
        glm::vec4 blackColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearTexImage(RenderTexture, 0, GL_RGBA, GL_FLOAT, &blackColor);
        glClearTexImage(DisplayTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blackColor);

        // RESIZE RENDER TEXTURE
        glBindTexture(GL_TEXTURE_2D, RenderTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCA_W, SCA_H, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        // RESIZE DISPLAY TEXTURE
        glBindTexture(GL_TEXTURE_2D, DisplayTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCA_W, SCA_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        // RESIZE CAMERA PATH VERTEX BUFFER
        uint32_t cameraPathVertexCount = SCA_W * SCA_H * (cameraBounces+1);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * cameraPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, cameraPathVertexBuffer);

        // RESIZE LIGHT PATH VERTEX BUFFER
        uint32_t lightPathVertexCount = SCA_W * SCA_H * (lightBounces+2);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightPathVertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * lightPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, lightPathVertexBuffer);

        // EMPTY TILE QUEUE
        while (!TileQueue.empty()) TileQueue.pop();
        accumulationFrame = 0;
        frameCount = 0;
    }

    void RestartRender()
    {
        while (!TileQueue.empty()) TileQueue.pop();
        accumulationFrame = 0;
        frameCount = 0;
    }

    void SetRendererDynamic()
    {
        if (dynamicScene == false)
        {
            dynamicScene = true;

            // SAVE CURRENT SETTINGS
            revert_resolutionScale = resolutionScale;
            revert_cameraBoucnes = cameraBounces;

            // OPTIMISE THE FRAMERATE
            resolutionScale = 0.2f;
            cameraBounces = 1;

            // RESIZE IMAGES TO LOWER RESOLUTION
            ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        }
    }

    void SetRendererStatic()
    {
        if (dynamicScene)
        {
            dynamicScene = false;

            // RESET RESOLUTION SCALE AND RENDER BUDGET
            resolutionScale = revert_resolutionScale;
            cameraBounces = revert_cameraBoucnes;

            // RESIZE IMAGES TO FULL RESOLUTION
            ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        }
    }

    void PathtraceFrame(unsigned int pathtraceShader, Camera &camera)
    {
        auto startTime = std::chrono::high_resolution_clock::now();

        int SCA_W = static_cast<int>(static_cast<float>(VIEWPORT_WIDTH) * resolutionScale);
        int SCA_H = static_cast<int>(static_cast<float>(VIEWPORT_HEIGHT) * resolutionScale);

        glUseProgram(pathtraceShader);
        camera.UpdatePathtracerUniforms(); // CAMERA UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_frameCount"), frameCount); // FRAME COUNT FOR PSEUDO RANDOMNESS
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_accumulationFrame"), accumulationFrame); // FRAME ACCUMULATION COUNT
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_debugMode"), 0); // DEBUG MODE
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_bounces"), cameraBounces); // CAMERA BOUNCES
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_light_bounces"), lightBounces); // LIGHT BOUCNES
        glUniform1f(glGetUniformLocation(pathtraceShader, "u_resolution_scale"), resolutionScale); // RESOLUTION SCALE
        glBindImageTexture(0, RenderTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F); // RENDER TEXTURE
        glBindImageTexture(1, DisplayTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); // DISPLAY TEXTURE

        uint32_t tilesX = static_cast<uint32_t>((static_cast<float>(SCA_W) + 32) / 32);
        uint32_t tilesY =  static_cast<uint32_t>((static_cast<float>(SCA_H) + 32) / 32);

        if (TileQueue.empty())
        {
            ScheduleRenderTiles(tilesX, tilesY, accumulationFrame);
        }

        while (!TileQueue.empty())
        {
            auto batchStartTime = std::chrono::high_resolution_clock::now();
            const RenderTile &tile = TileQueue.front();

            // UPDATE TILE OFFSET UNIFORM
            glUniform1ui(glGetUniformLocation(pathtraceShader, "u_tileX"), tile.x);
            glUniform1ui(glGetUniformLocation(pathtraceShader, "u_tileY"), tile.y);

            // RENDER TILE SEGMENT OF IMAGE
            glDispatchCompute(tile.width, tile.height, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            glFinish();

            // REMOVE TILE FROM QUEUE
            TileQueue.pop();

            auto batchEndTime = std::chrono::high_resolution_clock::now();
            float batchDuration = std::chrono::duration_cast<std::chrono::microseconds>(batchEndTime - batchStartTime).count() / 1000.0f;

            // Check elapsed time
            auto now = std::chrono::high_resolution_clock::now();
            float totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            lastFrameRenderTime += batchDuration;
            if (totalDuration + batchDuration >= renderBudget) break; // STOP RENDERING AFTER 16 MILLISECONDS
        }

        if (TileQueue.empty()) {
            accumulationFrame += 1;
            frameCount += 1;
        }
    }

    void RenderToViewport()
    {
        qRenderer.RenderToViewport(DisplayTexture);
    }

    unsigned int GetFrameBufferTextureID()
    {
        return qRenderer.GetFrameBufferTextureID();
    }

    uint32_t accumulationFrame = 0;
private:

    float renderBudget = 15;
    float resolutionScale = 1.0f;
    uint32_t cameraBounces = 3;
    uint32_t lightBounces = 2;
    uint32_t frameCount = 0;

    QuadRenderer qRenderer;
    int VIEWPORT_WIDTH;
    int VIEWPORT_HEIGHT;
    unsigned int DisplayTexture;
    unsigned int RenderTexture;
    unsigned int cameraPathVertexBuffer;
    unsigned int lightPathVertexBuffer;
    float lastFrameRenderTime;
    std::queue<RenderTile> TileQueue;

    // DYNAMIC SCENES
    bool dynamicScene = false;
    float revert_resolutionScale;
    float revert_cameraBoucnes;


    void ScheduleRenderTiles(uint32_t x_blocks, uint32_t y_blocks, uint32_t accumulationFrame)
    {
        uint32_t tileWidth = 0;
        if (dynamicScene) 
        {
            tileWidth = std::max(x_blocks, y_blocks);
        }
        else if (accumulationFrame == 0) // DEFAULT TILE WIDTH
        {
            tileWidth = 3;
        }
        else 
        { 
            float timePerBlock = lastFrameRenderTime / (x_blocks * y_blocks);
            float numBlocks = renderBudget / std::max(timePerBlock, 0.001f);
            tileWidth = static_cast<uint32_t>(std::sqrt(numBlocks)); 
            if (tileWidth < 3) tileWidth = 3;
        }
        lastFrameRenderTime = 0;


        // CREATE SCHEDULE QUEUE
        uint32_t x = 0;
        uint32_t y = 0;
        while (y < y_blocks)
        {
            // CREATE NEW RENDER TILE
            RenderTile tile;
            tile.width = std::min(tileWidth, x_blocks - x);
            tile.height = std::min(tileWidth, y_blocks - y);
            tile.x = x;
            tile.y = y; 

            // ADD RENDER TILE TO QUEUE
            TileQueue.push(tile);

            // INCREMENT X
            x += tile.width;

            // MOVE TO NEXT ROW
            if (x >= x_blocks)
            {
                x = 0;
                y += tile.height;
            }
        }
    }
};

