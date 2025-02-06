// EXTERNAL LIBRARIES
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "./glm/glm.hpp"

// STANDARD LIBRARY
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>

// PROJECT HEADERS
#include "raydia_quad_renderer.h"
#include "raydia_shader.h"
#include "raydia_mesh.h"
#include "raydia_light.h"
#include "raydia_debug.h"
#include "raydia_ui.h"
#include "raydia_model_manager.h"
#include "luminite_camera.h"

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

bool PathtraceFrame(unsigned int pathtraceShader, 
                 unsigned int RenderTexture, 
                 unsigned int DisplayTexture,
                 float VIEWPORT_WIDTH,
                 float VIEWPORT_HEIGHT,
                 uint32_t &renderTileX, 
                 uint32_t &renderTileY,
                 uint32_t &accumulationFrame,
                 uint32_t &frameCount,
                 uint32_t cameraBounces,
                 uint32_t lightBounces,
                 Camera &camera)
{
    glUseProgram(pathtraceShader);
    camera.UpdatePathtracerUniforms(); // CAMERA UNIFORM
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_frameCount"), frameCount); // FRAME COUNT FOR PSEUDO RANDOMNESS
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_accumulationFrame"), accumulationFrame); // FRAME ACCUMULATION COUNT
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pixelCount"), (uint32_t)VIEWPORT_WIDTH * (uint32_t)VIEWPORT_HEIGHT); // PIXEL COUNT
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_debugMode"), 0); // DEBUG MODE
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_bounces"), cameraBounces); // CAMERA BOUNCES
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_light_bounces"), lightBounces); // LIGHT BOUCNES
    glBindImageTexture(0, RenderTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F); // RENDER TEXTURE
    glBindImageTexture(1, DisplayTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); // DISPLAY TEXTURE

    uint32_t tilesX = (VIEWPORT_WIDTH + 32) / 32;
    uint32_t tilesY = (VIEWPORT_HEIGHT + 32) / 32;

    // NUMBER OF BATCH TILES
    uint32_t batchTilesX = 4;
    uint32_t batchTilesY = 4;

    auto startTime = std::chrono::high_resolution_clock::now();
    while (renderTileY < tilesY)
    {
        // UPDATE TILE OFFSET UNIFORM
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_tileX"), renderTileX);
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_tileY"), renderTileY);

        auto batchStartTime = std::chrono::high_resolution_clock::now();
        glDispatchCompute(batchTilesX, batchTilesX, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glFinish();
        auto batchEndTime = std::chrono::high_resolution_clock::now();
        auto batchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(batchEndTime - batchStartTime).count();

        // MOVE ONTO NEXT TILE ROW
        renderTileX+=batchTilesX;
        if (renderTileX >= tilesX) 
        {
            renderTileX = 0;
            renderTileY+=batchTilesY;
        }

        // Check elapsed time
        auto now = std::chrono::high_resolution_clock::now();
        float totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

        if (totalDuration + batchDuration >= 16.0f) break; // STOP RENDERING AFTER 16 MILLISECONDS
    }

    if (renderTileY >= tilesY)
    {
        renderTileX = 0;
        renderTileY = 0;
        accumulationFrame += 1;
        frameCount += 1;
        return true;
    }
    return false;
}

int main()
{
    float WIDTH = 1280;
    float HEIGHT = 720;
    float VIEWPORT_WIDTH = WIDTH * 0.8f;
    float VIEWPORT_HEIGHT = HEIGHT * 0.8f;
    uint32_t renderTileX = 0;
    uint32_t renderTileY = 0;
    uint32_t frameCount = 0;
    uint32_t accumulationFrame = 0;
    float frameTime = 0.0f;
    uint32_t cameraBounces = 2;
    uint32_t lightBounces = 2;

    // SETUP A GLFW WINDOW
    if (!glfwInit()) exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "RedFlare Renderer", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    
    // SETUP IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // GLEW INIT CHECK
    if (glewInit() != GLEW_OK) 
    {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return -1;
    }

    // OPENGL VIEWPORT
    glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // QUAD REDERING SHADER
    QuadRenderer qRenderer;
    qRenderer.PrepareQuadShader();
    qRenderer.CreateFrameBuffer((int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT);
    
    // PATH TRACING COMPUTE SHADER
    std::string pathtraceShaderSource = LoadShaderFromFile("./shaders/bidirectional.shader");
    unsigned int pathtraceShader = CreateComputeShader(pathtraceShaderSource);


    // RENDER TEXTURE SETUP
    unsigned int RenderTexture;
    glGenTextures(1, &RenderTexture);
    glBindTexture(GL_TEXTURE_2D, RenderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, int(VIEWPORT_WIDTH), int(VIEWPORT_HEIGHT), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // DISPLAY TEXTURE SETUP
    unsigned int DisplayTexture;
    glGenTextures(1, &DisplayTexture);
    glBindTexture(GL_TEXTURE_2D, DisplayTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, int(VIEWPORT_WIDTH), int(VIEWPORT_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // CREATE CAMERA
    Camera camera(pathtraceShader);



    // }----------{ LOAD 3D MESHES }----------{
    std::vector<Mesh*> meshes;
    LoadOBJ("./models/vw.obj", meshes);

    // meshes[4]->material.colour = glm::vec3(0.215f, 0.349f, 0.19f);
    // meshes[4]->material.roughness = 0.2f;

    // meshes[3]->material.refractive = 1;
    // meshes[3]->material.roughness = 0.0001f;
    // meshes[3]->material.IOR = 1.1f;

    // meshes[2]->material.colour = glm::vec3(0.1f, 0.1f, 0.1f);
    // meshes[2]->material.roughness = 0.9f;

    // // meshes[4]->material.emission = 2.0f;

    // meshes[0]->LoadAlbedo("textures/brick_colour.jpeg");
    // meshes[0]->LoadNormal("textures/brick_normal.jpg");
    // meshes[0]->LoadRoughness("textures/brick_roughness.jpg");


    // meshes[1]->material.emission = 2.0f;
    // meshes[1]->material.roughness = 0.001f;
    // meshes[1]->material.colour = glm::vec3(0.1f, 1.0f, 0.1f);
    // }----------{ LOAD 3D MESHES }----------{


    // }----------{ SEND MESH DATA TO THE GPU }----------{
    ModelManager modelManager;
    modelManager.CopyMeshDataToGPU(meshes, pathtraceShader);
    // }----------{ SEND MESH DATA TO THE GPU }----------{


    // }----------{ SEND LIGHT DATA TO THE GPU }----------{
    DirectionalLight sun;
    sun.brightness = 2.0f;
    std::vector<DirectionalLight> directionalLights;
    // directionalLights.push_back(sun);


    PointLight pLight;
    pLight.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight.position = glm::vec3(0.0f, 2.8f, 0.0f);
    pLight.brightness = 2.0f;

    PointLight pLight1;
    pLight1.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight1.position = glm::vec3(0.0f, 3.5f, 0.2f);
    pLight1.brightness = 1.0f;
    std::vector<PointLight> pointLights;
    // pointLights.push_back(pLight);
    // pointLights.push_back(pLight1);

    std::vector<Spotlight> spotlights;
    Spotlight spotlight;
    spotlight.brightness = 5.0f;
    spotlight.colour = glm::vec3(1.0f, 0.1f, 0.1f);
    spotlight.position = glm::vec3(1.0f, 3.5f, 0.0f);
    spotlight.angle = 90.0f;
    spotlight.falloff = 10.0f;
    // spotlights.push_back(spotlight);


    Spotlight spotlight2;
    spotlight2.brightness = 5.0f;
    spotlight2.colour = glm::vec3(0.0f, 1.0f, 0.0f);
    spotlight2.position = glm::vec3(1.0f, 3.5f, 0.0f);
    spotlight2.angle = 10.0f;
    spotlight2.falloff = 0.001f;
    // spotlights.push_back(spotlight2);

    glUseProgram(pathtraceShader);

    // DIRECTIONAL LIGHTS
    unsigned int directionalLightBuffer;
    glGenBuffers(1, &directionalLightBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, directionalLightBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DirectionalLight) * directionalLights.size(), directionalLights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, directionalLightBuffer);
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLights.size());

    // POINT LIGHTS
    unsigned int pointLightBuffer;
    glGenBuffers(1, &pointLightBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointLightBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLight) * pointLights.size(), pointLights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, pointLightBuffer);
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLights.size());

    // SPOTLIGHTS
    unsigned int spotlightBuffer;
    glGenBuffers(1, &spotlightBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, spotlightBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Spotlight) * spotlights.size(), spotlights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, spotlightBuffer);
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlights.size());
    // }----------{ SEND LIGHT DATA TO THE GPU }----------{



    // CAMERA PATH BUFFER
    unsigned int cameraPathVertexBuffer;
    uint32_t cameraPathVertexCount = (int)VIEWPORT_WIDTH * (int)VIEWPORT_HEIGHT * (cameraBounces+1);
    glGenBuffers(1, &cameraPathVertexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraPathVertexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * cameraPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, cameraPathVertexBuffer);

    // LIGHT PATH BUFFER
    unsigned int lightPathVertexBuffer;
    uint32_t lightPathVertexCount = (int)VIEWPORT_WIDTH * (int)VIEWPORT_HEIGHT * (lightBounces+2);
    glGenBuffers(1, &lightPathVertexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightPathVertexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * lightPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, lightPathVertexBuffer);

    // }----------{ APPLICATION LOOP }----------{
    while (!glfwWindowShouldClose(window))
    {
        auto start = std::chrono::high_resolution_clock::now();
        glfwPollEvents();

        // }----------{ HANDLE WINDOW RESIZING }----------{
        int windowbufferWidth, windowbufferHeight;
        glfwGetFramebufferSize(window, &windowbufferWidth, &windowbufferHeight);
        const float newWIDTH = float(windowbufferWidth);
        const float newHEIGHT = float(windowbufferHeight);

        // WINDOW RESIZED
        if (newWIDTH != WIDTH || newHEIGHT != HEIGHT)
        {
            HEIGHT = newHEIGHT;
            WIDTH = newWIDTH;
            VIEWPORT_WIDTH = WIDTH * 0.8f;
            VIEWPORT_HEIGHT = HEIGHT * 0.8f;

            // RESIZE FRAME BUFFER, OPENGL VIEWPORT
            qRenderer.ResizeFramebuffer((int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT);
            glViewport(0, 0, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT); 

            // RESIZE RENDER TEXTURE
            glBindTexture(GL_TEXTURE_2D, RenderTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, int(VIEWPORT_WIDTH), int(VIEWPORT_HEIGHT), 0, GL_RGBA, GL_FLOAT, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            // RESIZE DISPLAY TEXTURE
            glBindTexture(GL_TEXTURE_2D, DisplayTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, int(VIEWPORT_WIDTH), int(VIEWPORT_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            // RESIZE CAMERA PATH VERTEX BUFFER
            cameraPathVertexCount = (int)VIEWPORT_WIDTH * (int)VIEWPORT_HEIGHT * (cameraBounces+1);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, cameraPathVertexBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * cameraPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, cameraPathVertexBuffer);

            // RESIZE LIGHT PATH VERTEX BUFFER
            lightPathVertexCount = (int)VIEWPORT_WIDTH * (int)VIEWPORT_HEIGHT * (lightBounces+2);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightPathVertexBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PathVertex) * lightPathVertexCount, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, lightPathVertexBuffer);

            // RESET FRAME ACCUMULATION
            accumulationFrame = 0;

            // RESET RENDER TILE COORDINATES
            renderTileX = 0;
            renderTileY = 0;
        }
        // }----------{ HANDLE WINDOW RESIZING }----------{


        UpdateUI();
        glClearColor(0.047f, 0.082f, 0.122f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        

        // }----------{ INVOKE PATH TRACER }----------{
        bool completedFrame = PathtraceFrame(pathtraceShader, 
                                             RenderTexture, 
                                             DisplayTexture, 
                                             VIEWPORT_WIDTH, 
                                             VIEWPORT_HEIGHT,
                                             renderTileX,
                                             renderTileY, 
                                             accumulationFrame, 
                                             frameCount, 
                                             cameraBounces, 
                                             lightBounces, 
                                             camera);
        // }----------{ PATH TRACER ENDS }----------{


    

        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{
        qRenderer.RenderToViewport(DisplayTexture);
        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{


        // }----------{ APP LAYOUT }----------{
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin(
            "App Layout", nullptr, 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoCollapse
        );
        

        // }----------{ VIEWPORT WINDOW   }----------{
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("Viewport", ImVec2(VIEWPORT_WIDTH, VIEWPORT_HEIGHT), true);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)qRenderer.GetFrameBufferTextureID(),
            cursorPos,
            ImVec2(cursorPos.x + VIEWPORT_WIDTH, cursorPos.y + VIEWPORT_HEIGHT),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
        int frameRate = int((1.0f / frameTime) + 0.5f);
        std::string frameTimeString = std::to_string(frameTime * 1000) + "ms";
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
        ImGui::Text("%s", frameTimeString.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        // }----------{ VIEWPORT WINDOW   }----------{
        

        // }----------{ OBJECTS PANEL     }----------{
        RenderObjectsPanel(meshes, VIEWPORT_HEIGHT);
        // }----------{ OBJECTS PANEL     }----------{


        // }----------{ MODEL EXPLORER    }----------{
        RenderModelExplorer();
        // }----------{ MODEL EXPLORER    }----------{


        // }----------{ MATERIAL EXPLORER }----------{
        RenderMaterialExplorer();
        // }----------{ MATERIAL EXPLORER }----------{


        // }----------{ TEXTURE PANEL     }----------{
        RenderTexturesPanel();
        // }----------{ TEXTURE PANEL     }----------{


        ImGui::End();
        ImGui::PopStyleVar();
        // }----------{ APP LAYOUT ENDS   }----------{


        RenderUI();
        glfwSwapBuffers(window);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);
        frameTime = duration.count();
    }

    glDeleteBuffers(1, &RenderTexture);
    glDeleteBuffers(1, &directionalLightBuffer);
    glDeleteBuffers(1, &pointLightBuffer);
    glDeleteBuffers(1, &spotlightBuffer);
    glDeleteBuffers(1, &cameraPathVertexBuffer);

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}