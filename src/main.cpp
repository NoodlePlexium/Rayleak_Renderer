#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <vector>
#include <string>
#include "shader.h"
#include "utils.h"
#include "ui.h"

struct Vertex
{
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

void CreateFrameBuffer(unsigned int& FBO, uint32_t& TextureID, unsigned int& RBO, int width, int height)
{
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glGenTextures(1, &TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width,  height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TextureID, 0);
    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void ResizeFramebuffer(float width, float height, uint32_t& TextureID, unsigned int& RBO)
{
	glBindTexture(GL_TEXTURE_2D, TextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TextureID, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
}

int main()
{
    if (!glfwInit()) exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Raytrace", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    if (glewInit() != GLEW_OK) 
    {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return -1;
    }

    glViewport(0, 0, 640, 480);



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

    unsigned int RenderTexture;
    glGenTextures(1, &RenderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    unsigned int FBO, RBO;
    uint32_t TextureID;
    CreateFrameBuffer(FBO, TextureID, RBO, 640, 480);


    float prevViewportWidth = 640;
    float prevViewportHeight = 480;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        UpdateUI();
        glClearColor(0.047f, 0.082f, 0.122f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);




        // Bind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render the quad
        glUseProgram(quadShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, RenderTexture);
        glUniform1i(glGetUniformLocation(quadShader, "RenderTexture"), 0);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Unbind framebuffer



        // APP LAYOUT
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("App Layout", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

        // VIEWPORT WINDOW
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("Viewport", ImVec2(ImGui::GetContentRegionAvail().x * 0.8f, 300), true);
        const float viewportWidth = ImGui::GetContentRegionAvail().x;
        const float viewportHeight = ImGui::GetContentRegionAvail().y;
        if ((int)viewportWidth != (int)prevViewportWidth || (int)viewportHeight != (int)prevViewportHeight)
        {
            ResizeFramebuffer((int)viewportWidth, (int)viewportHeight, TextureID, RBO);
            prevViewportWidth = viewportWidth;
            prevViewportHeight = viewportHeight;
            glViewport(0, 0, (int)viewportWidth, (int)viewportHeight); 
        }
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)TextureID,
            cursorPos,
            ImVec2(cursorPos.x + viewportWidth, cursorPos.y + viewportHeight),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
        ImGui::EndChild();
        ImGui::PopStyleVar();
        // VIEWPORT WINDOW
        

        // OBJECTS PANEL
        ImGui::SameLine();
        ImGui::BeginChild("Objects", ImVec2(0, 300), true);
        ImGui::Text("Objects");
        ImGui::EndChild();
        // OBJECTS PANEL


        // MODEL EXPLORER
        ImGui::BeginChild("Model Explorer", ImVec2(ImGui::GetContentRegionAvail().x * 0.333f, ImGui::GetContentRegionAvail().y), true);
        ImGui::Text("Model Explorer");
        ImGui::EndChild();
        // MODEL EXPLORER


        // MATERIAL EXPLORER
        ImGui::SameLine();
        ImGui::BeginChild("Material Explorer", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetContentRegionAvail().y), true);
        ImGui::Text("Material Explorer");
        ImGui::EndChild();
        // MATERIAL EXPLORER


        // TEXTURE PANEL
        ImGui::SameLine();
        ImGui::BeginChild("Texture Panel", ImVec2(0, ImGui::GetContentRegionAvail().y), true);
        ImGui::Text("Texture Panel");
        ImGui::EndChild();
        // TEXTURE PANEL


        ImGui::End();
        ImGui::PopStyleVar();
        // APP LAYOUT

        
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        // ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
        // ImVec2 viewportPos = ImVec2(3.0f, 3.0f);
        // ImGui::SetCursorPos(viewportPos);
        // const float viewportWidth = ImGui::GetContentRegionAvail().x;
        // const float viewportHeight = ImGui::GetContentRegionAvail().y;
        // if ((int)viewportWidth != (int)prevViewportWidth || (int)viewportHeight != (int)prevViewportHeight)
        // {
        //     ResizeFramebuffer((int)viewportWidth, (int)viewportHeight, TextureID, RBO);
        //     prevViewportWidth = viewportWidth;
        //     prevViewportHeight = viewportHeight;
        //     glViewport(0, 0, (int)viewportWidth, (int)viewportHeight); 
        // }
        // // Display the rendered texture in ImGui
        // ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        // ImGui::GetWindowDrawList()->AddImage(
        //     (ImTextureID)(intptr_t)TextureID,
        //     cursorPos,
        //     ImVec2(cursorPos.x + viewportWidth, cursorPos.y + viewportHeight),
        //     ImVec2(0, 1),
        //     ImVec2(1, 0)
        // );
        // ImGui::PopStyleVar();
        // ImGui::End();








        RenderUI();
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}