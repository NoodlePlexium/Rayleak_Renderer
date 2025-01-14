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

int main()
{
    float WIDTH = 640;
    float HEIGHT = 480;
    float VIEWPORT_WIDTH = WIDTH * 0.8f;
    float VIEWPORT_HEIGHT = HEIGHT * 0.8f;
    uint32_t frameCount = 0;
    uint32_t accumulationFrame = 0;
    float frameTime = 0.0f;

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
    std::string pathtraceShaderSource = LoadShaderFromFile("./shaders/pathtrace.shader");
    unsigned int pathtraceShader = CreateComputeShader(pathtraceShaderSource);

    // RENDER TEXTURE SETUP
    unsigned int RenderTexture;
    glGenTextures(1, &RenderTexture);
    glBindTexture(GL_TEXTURE_2D, RenderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, int(VIEWPORT_WIDTH), int(VIEWPORT_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // CAMERA INFO UNIFORM
    unsigned int camInfoPosLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.pos");
    unsigned int camInfoForwardLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.forward");
    unsigned int camInfoRightLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.right");
    unsigned int camInfoUpLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.up");
    unsigned int camInfoFOVLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.FOV");




    // }----------{ LOAD 3D MESHES }----------{
    std::vector<Mesh*> meshes;
    LoadOBJ("./models/vw.obj", meshes);
    // }----------{ LOAD 3D MESHES }----------{


    // }----------{ SEND MESH DATA TO THE GPU }----------{
    ModelManager modelManager;
    modelManager.CopyMeshDataToGPU(meshes, pathtraceShader);
    // }----------{ SEND MESH DATA TO THE GPU }----------{


    // }----------{ SEND LIGHT DATA TO THE GPU }----------{
    DirectionalLight sun;
    std::vector<DirectionalLight> directionalLights;
    // directionalLights.push_back(sun);

    PointLight pLight;
    pLight.colour = glm::vec3(1.0f, 0.1f, 0.1f);
    pLight.position = glm::vec3(-2.0f, 3.5f, 0.0f);
    pLight.brightness = 2.0f;

    PointLight pLight1;
    pLight1.colour = glm::vec3(0.1f, 0.1f, 1.0f);
    pLight1.position = glm::vec3(-2.0f, 3.5f, 1.0f);
    pLight1.brightness = 2.0f;
    std::vector<PointLight> pointLights;
    pointLights.push_back(pLight);
    pointLights.push_back(pLight1);

    Spotlight spotlight;
    std::vector<Spotlight> spotlights;
    spotlight.brightness = 5.0f;
    spotlight.colour = glm::vec3(1.0f, 1.0f, 0.0f);
    spotlight.position = glm::vec3(0.0f, 3.5f, 0.0f);
    spotlight.angle = 30.0f;
    spotlights.push_back(spotlight);

    glUseProgram(pathtraceShader);

    // DIRECTIONAL LIGHTS
    unsigned int directionalLightBuffer;
    glGenBuffers(1, &directionalLightBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, directionalLightBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DirectionalLight) * directionalLights.size(), directionalLights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, directionalLightBuffer);
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_directionalLightCount"), directionalLights.size());

    // POINT LIGHTS
    unsigned int pointLightBuffer;
    glGenBuffers(1, &pointLightBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointLightBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointLight) * pointLights.size(), pointLights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, pointLightBuffer);
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_pointLightCount"), pointLights.size());

    // SPOTLIGHTS
    unsigned int spotlightBuffer;
    glGenBuffers(1, &spotlightBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, spotlightBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Spotlight) * spotlights.size(), spotlights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, spotlightBuffer);
    glUniform1ui(glGetUniformLocation(pathtraceShader, "u_spotlightCount"), spotlights.size());
    // }----------{ SEND LIGHT DATA TO THE GPU }----------{









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

            // RESIZE FRAME BUFFER, OPENGL VIEWPORT AND RENDER TEXTURE
            qRenderer.ResizeFramebuffer((int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT);
            glViewport(0, 0, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT); 
            glBindTexture(GL_TEXTURE_2D, RenderTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            // RESET FRAME ACCUMULATION
            accumulationFrame = 0;
        }
        // }----------{ HANDLE WINDOW RESIZING }----------{


        UpdateUI();
        glClearColor(0.047f, 0.082f, 0.122f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        

        // }----------{ INVOKE PATH TRACER }----------{
        glUseProgram(pathtraceShader);

        // CAMERA UNIFORM
        glm::vec3 camPos{0.0f, 0.9f, -4.9f};
        glm::vec3 camForward{0.0f, 0.0f, 1.0f};
        glm::vec3 camRight{1.0f, 0.0f, 0.0f};
        glm::vec3 camUp{0.0f, 1.0f, 0.0f};
        float camFOV = 75.0f;
        glUniform3f(camInfoPosLocation, camPos.x, camPos.y, camPos.z);
        glUniform3f(camInfoForwardLocation, camForward.x, camForward.y, camForward.z);
        glUniform3f(camInfoRightLocation, camRight.x, camRight.y, camRight.z);
        glUniform3f(camInfoUpLocation, camUp.x, camUp.y, camUp.z);
        glUniform1f(camInfoFOVLocation, camFOV);

        // FRAME COUNT FOR PSEUDO RANDOMNESS
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_frameCount"), frameCount);

        // FRAME ACCUMULATION COUNT
        glUniform1ui(glGetUniformLocation(pathtraceShader, "u_accumulationFrame"), accumulationFrame);

        // RENDER TEXTURE
        glBindImageTexture(0, RenderTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); 

        // RUN PATH TRACE SHADER
        GLuint numWorkGroupsX = (VIEWPORT_WIDTH + 32) / 32;
        GLuint numWorkGroupsY = (VIEWPORT_HEIGHT + 32) / 32;
        glDispatchCompute(numWorkGroupsX, numWorkGroupsY, 1);       
        // glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // }----------{ PATH TRACER ENDS }----------{
    

        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{
        qRenderer.RenderToViewport(RenderTexture);
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
        std::string frameTimeString = std::to_string(frameRate) + "fps";
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
        ImGui::Text("%s", frameTimeString.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        // }----------{ VIEWPORT WINDOW   }----------{
        

        // }----------{ OBJECTS PANEL     }----------{
        ImGui::SameLine();
        ImGui::BeginChild("Objects", ImVec2(0, VIEWPORT_HEIGHT), true);
        ImGui::Text("Objects");
        ImGui::EndChild();
        // }----------{ OBJECTS PANEL     }----------{


        // }----------{ MODEL EXPLORER    }----------{
        ImGui::BeginChild(
            "Model Explorer", 
            ImVec2(ImGui::GetContentRegionAvail().x * 0.333f, 
            ImGui::GetContentRegionAvail().y), true
        );
        ImGui::Text("Model Explorer");
        ImGui::EndChild();
        // }----------{ MODEL EXPLORER    }----------{


        // }----------{ MATERIAL EXPLORER }----------{
        ImGui::SameLine();
        ImGui::BeginChild(
            "Material Explorer", 
            ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 
            ImGui::GetContentRegionAvail().y), true
        );
        ImGui::Text("Material Explorer");
        ImGui::EndChild();
        // }----------{ MATERIAL EXPLORER }----------{


        // }----------{ TEXTURE PANEL     }----------{
        ImGui::SameLine();
        ImGui::BeginChild("Texture Panel", ImVec2(0, ImGui::GetContentRegionAvail().y), true);
        ImGui::Text("Texture Panel");
        ImGui::EndChild();
        // }----------{ TEXTURE PANEL     }----------{


        ImGui::End();
        ImGui::PopStyleVar();
        // }----------{ APP LAYOUT ENDS   }----------{


        RenderUI();
        glfwSwapBuffers(window);
        frameCount += 1;
        accumulationFrame += 1;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);
        frameTime = duration.count();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}