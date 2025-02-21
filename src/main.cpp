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
#include "render_system.h"
#include "shader.h"
#include "mesh.h"
#include "light.h"
#include "debug.h"
#include "user_interface.h"
#include "object_manager.h"
#include "camera.h"
#include "material.h"

int main()
{
    float WIDTH = 1280;
    float HEIGHT = 720;
    float BOTTOM_PANEL_HEIGHT = 400;
    int VIEWPORT_WIDTH = static_cast<int>(WIDTH - BOTTOM_PANEL_HEIGHT);
    int VIEWPORT_HEIGHT = static_cast<int>(HEIGHT - BOTTOM_PANEL_HEIGHT);
    float frameTime = 0.0f;



    double lastMouseX = 0.0f;
    double lastMouseY = 0.0f;
    bool viewportSelected = false;
    bool leftMouseHeld = false;
    bool rightMouseHeld = false;
    bool leftMouseDown = false;
    bool rightMouseDown = false;

    // SETUP A GLFW WINDOW
    if (!glfwInit()) exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Luminite Renderer", NULL, NULL);
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
    
    // PATH TRACING COMPUTE SHADER
    std::string pathtraceShaderSource = LoadShaderFromFile("./shaders/pathtrace.shader");
    unsigned int pathtraceShader = CreateComputeShader(pathtraceShaderSource);

    // CREATE CAMERA
    Camera camera(pathtraceShader);

    // CREATE RENDER SYSTEM
    RenderSystem renderSystem(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // CREATE USER INTERFACE OBJECT
    UserInterface UI(pathtraceShader);


    // }----------{ LOAD 3D MESHES }----------{
    uint32_t modelIncrement = 0;
    std::vector<Model> models;
    std::vector<Mesh*> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

    Material stoneMat;
    stoneMat.LoadAlbedo("textures/stone_colour.jpg");
    stoneMat.LoadNormal("textures/stone_normal.jpg");
    stoneMat.LoadRoughness("textures/stone_roughness.jpg");

    Material roofMat;
    roofMat.LoadAlbedo("textures/roof_colour.jpg");
    roofMat.LoadNormal("textures/roof_normal.jpg");
    roofMat.LoadRoughness("textures/roof_roughness.jpg");

    Material curtainMat;
    curtainMat.LoadAlbedo("textures/fabric_colour.jpg");
    curtainMat.LoadNormal("textures/fabric_normal.jpg");
    curtainMat.LoadRoughness("textures/fabric_roughness.jpg");
    curtainMat.data.colour = glm::vec3(0.91f, 0.357f, 0.337f);

    Material bannerMat;
    bannerMat.LoadAlbedo("textures/fabric_colour.jpg");
    bannerMat.LoadNormal("textures/fabric_normal.jpg");
    bannerMat.LoadRoughness("textures/fabric_roughness.jpg");
    bannerMat.data.colour = glm::vec3(0.224f, 0.302f, 0.502f);

    materials.push_back(stoneMat);
    materials.push_back(roofMat);
    materials.push_back(curtainMat);
    materials.push_back(bannerMat);

    LoadOBJ("./models/vw.obj", meshes, models, modelIncrement);
    LoadOBJ("./models/lion.obj", meshes, models, modelIncrement);

    // APPLY STONE TO EVERYTHING
    for (int i=0; i<meshes.size(); i++)
    {
        meshes[i]->materialIndex = 0;
    }

    // // APPLY ROOF MATERIAL
    // uint32_t roofMeshIndex = FindMeshByName("roof", meshes);
    // meshes[roofMeshIndex]->materialIndex = 1;

    // // APPLY CURTAIN MATERIAL
    // uint32_t curtainMeshIndex = FindMeshByName("curtains", meshes);
    // meshes[curtainMeshIndex]->materialIndex = 2;

    // // APPLY BANNER MATERIAL
    // uint32_t bannerMeshIndex = FindMeshByName("banners", meshes);
    // meshes[bannerMeshIndex]->materialIndex = 3;

    // }----------{ LOAD 3D MESHES }----------{


    // }----------{ SEND MESH DATA TO THE GPU }----------{
    ModelManager modelManager(pathtraceShader);
    modelManager.AddModelToScene(models[0], pathtraceShader);
    // modelManager.AddModelToScene(models[1], pathtraceShader);
    modelManager.AddMaterialToScene(materials[0].data, pathtraceShader);
    modelManager.AddMaterialToScene(materials[1].data, pathtraceShader);
    modelManager.AddMaterialToScene(materials[2].data, pathtraceShader);
    modelManager.AddMaterialToScene(materials[3].data, pathtraceShader);
    // }----------{ SEND MESH DATA TO THE GPU }----------{


    // }----------{ SEND LIGHT DATA TO THE GPU }----------{
    DirectionalLight sun;
    sun.brightness = 2.5f;
    std::vector<DirectionalLight> directionalLights;
    directionalLights.push_back(sun);



    PointLight pLight;
    pLight.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight.position = glm::vec3(-3.5f, 6.0f, 0.0f);
    pLight.brightness = 3.0f;

    PointLight pLight1;
    pLight1.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight1.position = glm::vec3(-3.5f, 6.0f, 5.0f);
    pLight1.brightness = 3.0f;

    PointLight pLight2;
    pLight2.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight2.position = glm::vec3(-3.5f, 6.0f, -5.0f);
    pLight2.brightness = 3.0f;



    PointLight pLight3;
    pLight3.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight3.position = glm::vec3(3.5f, 6.0f, 0.0f);
    pLight3.brightness = 3.0f;

    PointLight pLight4;
    pLight4.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight4.position = glm::vec3(3.5f, 6.0f, 5.0f);
    pLight4.brightness = 3.0f;

    PointLight pLight5;
    pLight5.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    pLight5.position = glm::vec3(3.5f, 6.0f, -5.0f);
    pLight5.brightness = 3.0f;

    std::vector<PointLight> pointLights;
    pointLights.push_back(pLight);
    pointLights.push_back(pLight1);
    pointLights.push_back(pLight2);

    pointLights.push_back(pLight3);
    pointLights.push_back(pLight4);
    pointLights.push_back(pLight5);

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


    modelManager.CopyLightDataToGPU(directionalLights, pointLights, spotlights, pathtraceShader);
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
            VIEWPORT_WIDTH = static_cast<int>(WIDTH - BOTTOM_PANEL_HEIGHT);
            VIEWPORT_HEIGHT = static_cast<int>(HEIGHT - BOTTOM_PANEL_HEIGHT);

            // RESIZE FRAME BUFFER, OPENGL VIEWPORT
            renderSystem.ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
            glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT); 
        }
        // }----------{ HANDLE WINDOW RESIZING }----------{

        UI.NewFrame();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);


        renderSystem.SetRendererStatic();

        // }----------{ VIEWPORT CONTROLS }----------{
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        float dx = float(mouseX) - float(lastMouseX);
        float dy = float(mouseY) - float(lastMouseY);
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            if (!leftMouseHeld) leftMouseDown = true;
            else leftMouseDown = false;
            leftMouseHeld = true;
        }
        else
        {
            leftMouseHeld = false;
            leftMouseDown = false;
        }

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            if (!rightMouseHeld) rightMouseDown = true;
            else rightMouseDown = false;
            rightMouseHeld = true;
        }
        else
        {
            rightMouseHeld = false;
            rightMouseDown = false;
        }

        bool xInVP = mouseX >= 5 && mouseX <= 5 + VIEWPORT_WIDTH;
        bool yInVP = mouseY >= 5 && mouseY <= 5 + VIEWPORT_HEIGHT;
        bool cursorOverViewport = xInVP && yInVP;

        if (leftMouseDown ||
            rightMouseDown)
        {
            viewportSelected = cursorOverViewport;
        }

        // DESELECT FOCUSED IMGUI ELEMENT ON RIGHT CLICK
        if (ImGui::IsAnyItemActive() && rightMouseDown) {
            ImGui::SetKeyboardFocusHere(-1);
        }


        if (viewportSelected)
        {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                camera.rotation.y += dx * 0.55f;
                camera.rotation.x += dy * 0.55f;
                camera.rotation.x = std::max(-89.0f, std::min(89.0f, camera.rotation.x));   
                renderSystem.SetRendererDynamic();
            }

            glm::vec3 moveDir = glm::vec3(0, 0, 0);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            {
                moveDir += camera.forward * 1.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            {
                moveDir += camera.right * -1.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            {
                moveDir += camera.forward * -1.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            {
                moveDir += camera.right * 1.0f;
            }
             if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            {
                moveDir += glm::vec3(0, 1, 0);
            }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            {
                moveDir += glm::vec3(0, -1, 0);
            }  
            if (glm::length(moveDir) != 0.0f)
            {
                camera.pos += glm::normalize(moveDir) * frameTime * 3.0f;
                renderSystem.SetRendererDynamic();
            }
        }
        // }----------{ VIEWPORT CONTROLS }----------{



        // }----------{ INVOKE PATH TRACER }----------{
        renderSystem.PathtraceFrame(pathtraceShader, camera);
        // }----------{ PATH TRACER ENDS }----------{


    

        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{
        renderSystem.RenderToViewport();
        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{


        // }----------{ APP LAYOUT }----------{
        UI.BeginAppLayout(cursorOverViewport);
        UI.RenderViewportPanel(VIEWPORT_WIDTH, VIEWPORT_HEIGHT, frameTime, cursorOverViewport, renderSystem.GetFrameBufferTextureID(), camera, modelManager, models);
        UI.RenderObjectsPanel(meshes, VIEWPORT_HEIGHT);
        ImGui::Dummy(ImVec2(1, 0));
        UI.RenderModelExplorer(meshes, models, modelIncrement);
        UI.RenderMaterialExplorer(materials, textures, modelManager);
        UI.RenderTexturesPanel(textures);
        UI.EndAppLayout();
        // }----------{ APP LAYOUT ENDS   }----------{


        UI.RenderUI();
        glfwSwapBuffers(window);

        if (UI.restartRender)
        {
            renderSystem.RestartRender();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);
        frameTime = duration.count();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}