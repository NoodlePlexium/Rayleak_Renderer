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
#include "model_manager.h"
#include "light_manager.h"
#include "debug.h"
#include "user_interface.h"
#include "gpu_memory_manager.h"
#include "camera.h"
#include "material.h"

int main()
{
    float WIDTH = 1400;
    float HEIGHT = 900;
    float BOTTOM_PANEL_HEIGHT = 320;
    float OBJECT_PANEL_WIDTH = 310;
    int VIEWPORT_WIDTH = static_cast<int>(WIDTH - OBJECT_PANEL_WIDTH);
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

    // RAYCASTING COMPUTE SHADER
    std::string raycastShaderSource = LoadShaderFromFile("./shaders/raycast.shader");
    unsigned int raycastShader = CreateComputeShader(raycastShaderSource);

    // CREATE CAMERA
    Camera camera(pathtraceShader);

    // CREATE RENDER SYSTEM
    RenderSystem renderSystem(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    // CREATE USER INTERFACE OBJECT
    UserInterface UI(pathtraceShader);

    // CREATE A MODEL MANAGER
    ModelManager modelManager(pathtraceShader);

    // CREATE A LIGHT MANAGER
    LightManager lightManager(pathtraceShader);

    // CREATE A MATERIAL MANAGER
    MaterialManager materialManager(pathtraceShader); 

    // LOAD DEMONSTRATION MODELS
    modelManager.LoadModel("./models/vw.obj");
    modelManager.LoadModel("./models/lion.obj");




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
            VIEWPORT_WIDTH = static_cast<int>(WIDTH - OBJECT_PANEL_WIDTH);
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
                camera.rotation.y += dx * 0.4f;
                camera.rotation.x -= dy * 0.4f;
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
                moveDir += camera.right * 1.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            {
                moveDir += camera.forward * -1.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            {
                moveDir -= camera.right * 1.0f;
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
        UI.BeginAppLayout();
        UI.RenderViewportPanel(
            VIEWPORT_WIDTH, 
            VIEWPORT_HEIGHT, 
            frameTime, 
            cursorOverViewport, 
            renderSystem.GetFrameBufferTextureID(),
            camera, 
            modelManager, 
            renderSystem, 
            raycastShader
        );

        UI.BeginSidebar(VIEWPORT_HEIGHT);

        float transformPanelHeight = 231.0f;  
        float remainingHeight = VIEWPORT_HEIGHT - transformPanelHeight;  
        float objectsPanelHeight = remainingHeight * 0.5f;  
        float lightsPanelHeight = remainingHeight * 0.5f;  
        UI.RenderObjectsPanel(modelManager, objectsPanelHeight);
        UI.RenderLightsPanel(lightManager, lightsPanelHeight);
        UI.RenderTransformPanel(modelManager, lightManager, transformPanelHeight);
        UI.EndSidebar();

        ImGui::Dummy(ImVec2(1, 0));
        UI.RenderModelExplorer(modelManager);
        UI.RenderMaterialExplorer(materialManager, renderSystem);
        UI.RenderTexturesPanel(materialManager);
        UI.EndAppLayout();
        glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT); // RESET GL VIEWPORT
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