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
#include "luminite_render_system.h"
#include "luminite_shader.h"
#include "luminite_mesh.h"
#include "luminite_light.h"
#include "luminite_debug.h"
#include "luminite_ui.h"
#include "luminite_object_manager.h"
#include "luminite_camera.h"

int main()
{
    float WIDTH = 1280;
    float HEIGHT = 720;
    int VIEWPORT_WIDTH = static_cast<int>(WIDTH - 250.0f);
    int VIEWPORT_HEIGHT = static_cast<int>(HEIGHT - 250.0f);
    float frameTime = 0.0f;


    // CAMERA SETTINGS


    double lastMouseX = 0.0f;
    double lastMouseY = 0.0f;

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
    std::string pathtraceShaderSource = LoadShaderFromFile("./shaders/bidirectional.shader");
    unsigned int pathtraceShader = CreateComputeShader(pathtraceShaderSource);


    
    // CREATE CAMERA
    Camera camera(pathtraceShader);


    RenderSystem renderSystem(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);


    // }----------{ LOAD 3D MESHES }----------{
    std::vector<Mesh*> meshes;
    std::vector<Material> materials;

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
    curtainMat.colour = glm::vec3(0.91f, 0.357f, 0.337f);

    Material bannerMat;
    bannerMat.LoadAlbedo("textures/fabric_colour.jpg");
    bannerMat.LoadNormal("textures/fabric_normal.jpg");
    bannerMat.LoadRoughness("textures/fabric_roughness.jpg");
    bannerMat.colour = glm::vec3(0.224f, 0.302f, 0.502f);

    materials.push_back(stoneMat);
    materials.push_back(roofMat);
    materials.push_back(curtainMat);
    materials.push_back(bannerMat);

    LoadOBJ("./models/sponza_simple.obj", meshes);

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
    ModelManager modelManager;
    modelManager.CopyMeshDataToGPU(meshes, materials, pathtraceShader);
    // }----------{ SEND MESH DATA TO THE GPU }----------{


    // }----------{ SEND LIGHT DATA TO THE GPU }----------{
    DirectionalLight sun;
    sun.brightness = 2.5f;
    std::vector<DirectionalLight> directionalLights;
    // directionalLights.push_back(sun);



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


    ImFont* fontBody = LoadFont("fonts/Inter/Inter-VariableFont_opsz,wght.ttf", 18);
    ImFont* fontHeader = LoadFont("fonts/Inter/Inter-VariableFont_opsz,wght.ttf", 24);

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
            VIEWPORT_WIDTH = static_cast<int>(WIDTH - 250.0f);
            VIEWPORT_HEIGHT = static_cast<int>(HEIGHT - 250.0f);

            // RESIZE FRAME BUFFER, OPENGL VIEWPORT
            renderSystem.ResizeFramebuffer(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
            glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT); 
        }
        // }----------{ HANDLE WINDOW RESIZING }----------{


        UpdateUI();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        renderSystem.SetRendererStatic();

        // }----------{ VIEWPORT CONTROLS }----------{
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        float dx = float(mouseX) - float(lastMouseX);
        float dy = float(mouseY) - float(lastMouseY);
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            camera.rotation.y += dx * 0.55f;
            camera.rotation.x += dy * 0.55f;
            camera.rotation.x = std::max(-89.0f, std::min(89.0f, camera.rotation.x));   
            renderSystem.SetRendererDynamic();
        }

        glm::vec3 moveDir = glm::vec3(0.0f, 0.0f, 0.0f);
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
            moveDir += glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        {
            moveDir += glm::vec3(0.0f, -1.0f, 0.0f);
        }  
        if (glm::length(moveDir) != 0.0f)
        {
            camera.pos += glm::normalize(moveDir) * frameTime * 3.0f;
            renderSystem.SetRendererDynamic();
        }
        // }----------{ VIEWPORT CONTROLS }----------{





        // }----------{ INVOKE PATH TRACER }----------{
        renderSystem.PathtraceFrame(
            pathtraceShader,  
            camera
        );
        // }----------{ PATH TRACER ENDS }----------{


    

        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{
        renderSystem.RenderToViewport();
        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{


        // }----------{ APP LAYOUT }----------{
        ImGui::PushFont(fontBody);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
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
            (ImTextureID)(intptr_t)renderSystem.GetFrameBufferTextureID(),
            cursorPos,
            ImVec2(cursorPos.x + VIEWPORT_WIDTH, cursorPos.y + VIEWPORT_HEIGHT),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
        int frameRate = int((1.0f / frameTime) + 0.5f);
        std::string frameTimeString = std::to_string(frameTime * 1000) + "ms";
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 128, 255));
        ImGui::Text("%s", frameTimeString.c_str());
        ImGui::PopStyleColor();


        
        // Settings Menu
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
        ImGui::BeginChild("Settings Panel", ImVec2(240.0f, 0), false);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.1f));
            ImGui::BeginChild("Settings", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

            PaddedText("Camera Settings", 5.0f);
            FloatAttribute("Field of View", "FOV", "", 5.0f, 5.0f, &camera.fov);
            CheckboxAttribute("Depth of Field", "DOF", 5.0f, 5.0f, &camera.dof);
            FloatAttribute("Focus Distance", "FOCUS", "m", 5.0f, 5.0f, &camera.focus_distance);
            FloatAttribute("fStops", "fStops", "", 5.0f, 5.0f, &camera.fStop);
            CheckboxAttribute("Anti Aliasing", "AA", 5.0f, 5.0f, &camera.anti_aliasing);
            FloatAttribute("Exposure", "EXPOSURE", "", 5.0f, 5.0f, &camera.exposure);
            
            if (camera.SettingsChanged())
            {
                renderSystem.RestartRender();
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::EndChild();
        // Settings Menu



        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        // }----------{ VIEWPORT WINDOW   }----------{
        

        // }----------{ OBJECTS PANEL     }----------{
        RenderObjectsPanel(meshes, VIEWPORT_HEIGHT, fontHeader);
        // }----------{ OBJECTS PANEL     }----------{


        // }----------{ MODEL EXPLORER    }----------{
        RenderModelExplorer(fontHeader);
        // }----------{ MODEL EXPLORER    }----------{


        // }----------{ MATERIAL EXPLORER }----------{
        RenderMaterialExplorer(fontHeader);
        // }----------{ MATERIAL EXPLORER }----------{


        // }----------{ TEXTURE PANEL     }----------{
        RenderTexturesPanel(fontHeader);
        // }----------{ TEXTURE PANEL     }----------{


        ImGui::End();
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        // }----------{ APP LAYOUT ENDS   }----------{


        RenderUI();
        glfwSwapBuffers(window);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);
        frameTime = duration.count();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}