// External Libraries
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "glm/glm.hpp"

// Standard Library
#include <iostream>
#include <vector>
#include <string>

// Project Headers
#include "shader.h"
#include "utils.h"
#include "ui.h"

struct Vertex
{
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

void CreateFrameBuffer(unsigned int& FBO, uint32_t& FrameBufferTextureID, unsigned int& RBO, int width, int height)
{
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glGenTextures(1, &FrameBufferTextureID);
    glBindTexture(GL_TEXTURE_2D, FrameBufferTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width,  height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FrameBufferTextureID, 0);
    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

}

void ResizeFramebuffer(float width, float height, uint32_t& FrameBufferTextureID, unsigned int& RBO)
{
	glBindTexture(GL_TEXTURE_2D, FrameBufferTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FrameBufferTextureID, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
}

int main()
{
    float WIDTH = 640;
    float HEIGHT = 480;
    float VIEWPORT_WIDTH = WIDTH * 0.8f;
    float VIEWPORT_HEIGHT = HEIGHT * 0.8f;

    // GLFW INIT CHECK
    if (!glfwInit()) exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Raytrace", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    
    // IMGUI SETUP
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

    // INITIALISE OPENGL VIEWPORT
    glViewport(0, 0, WIDTH, HEIGHT);

    // QUAD VERTEX AND FRAGMENT SHADER
    std::string vertexShaderSource = LoadShaderFromFile("./shaders/vert.shader");
    std::string fragmentShaderSource = LoadShaderFromFile("./shaders/frag.shader");
    unsigned int quadShader = CreateRasterShader(vertexShaderSource, fragmentShaderSource);
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f};
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
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
    
    // FRAME BUFFER SETUP
    unsigned int FBO, RBO;
    uint32_t FrameBufferTextureID;
    CreateFrameBuffer(FBO, FrameBufferTextureID, RBO, WIDTH, HEIGHT);






    // }----------{ APPLICATION LOOP }----------{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // GET GLFW WINDOW DIMENSIONS
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
            ResizeFramebuffer((int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT, FrameBufferTextureID, RBO);
            glBindTexture(GL_TEXTURE_2D, RenderTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);
            glViewport(0, 0, (int)VIEWPORT_WIDTH, (int)VIEWPORT_HEIGHT); 
        }
        UpdateUI();
        glClearColor(0.047f, 0.082f, 0.122f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // }----------{ INVOKE PATH TRACER }----------{
        glUseProgram(pathtraceShader);

        // CAMERA UNIFORM
        glm::vec3 camPos{0.0f, 0.0f, 0.0f};
        glm::vec3 camForward{0.0f, 0.0f, 1.0f};
        glm::vec3 camRight{1.0f, 0.0f, 0.0f};
        glm::vec3 camUp{0.0f, 1.0f, 0.0f};
        float camFOV = 90.0f;
        unsigned int camInfoPosLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.pos");
        unsigned int camInfoForwardLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.forward");
        unsigned int camInfoRightLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.right");
        unsigned int camInfoUpLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.up");
        unsigned int camInfoFOVLocation = glGetUniformLocation(pathtraceShader, "cameraInfo.FOV");
        glUniform3f(camInfoPosLocation, camPos.x, camPos.y, camPos.z);
        glUniform3f(camInfoForwardLocation, camForward.x, camForward.y, camForward.z);
        glUniform3f(camInfoRightLocation, camRight.x, camRight.y, camRight.z);
        glUniform3f(camInfoUpLocation, camUp.x, camUp.y, camUp.z);
        glUniform1f(camInfoFOVLocation, camFOV);

        // RENDER TEXTURE
        glBindImageTexture(0, RenderTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8); 
        glDispatchCompute(VIEWPORT_WIDTH / 1, VIEWPORT_HEIGHT / 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // }----------{ PATH TRACER ENDS }----------{


        // }----------{ RENDER THE QUAD TO THE FRAME BUFFER }----------{
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(quadShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, RenderTexture);
        glUniform1i(glGetUniformLocation(quadShader, "RenderTexture"), 0);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
            (ImTextureID)(intptr_t)FrameBufferTextureID,
            cursorPos,
            ImVec2(cursorPos.x + VIEWPORT_WIDTH, cursorPos.y + VIEWPORT_HEIGHT),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
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
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}