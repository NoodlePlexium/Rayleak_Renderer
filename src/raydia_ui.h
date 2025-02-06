#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <imgui.h>

void UpdateUI()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::ShowDemoWindow();
}

void RenderUI()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ShutdownImGUI()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void RenderObjectsPanel(const std::vector<Mesh*> &meshes, float VIEWPORT_HEIGHT)
{
    // ImVec4 backgroundColour = ImVec4(0.196f, 0.352f, 0.533f, 1.0f);
    ImVec4 backgroundColour = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 buttonColour = ImVec4(0.047f, 0.161f, 0.294f, 1.0f);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColour);
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColour);
    ImGui::BeginChild("Objects", ImVec2(0, VIEWPORT_HEIGHT), true);
    ImGui::Text("Objects");
    ImVec2 ObjectPanelWidth = ImVec2(ImGui::GetContentRegionAvail().x, 0);
    for (int i=0; i<meshes.size(); ++i)
    {

        if (ImGui::Button(meshes[i]->name.c_str(), ObjectPanelWidth))
        {

        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

void RenderModelExplorer()
{
    ImVec4 backgroundColour = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColour);
    ImGui::BeginChild(
        "Model Explorer", 
        ImVec2(ImGui::GetContentRegionAvail().x * 0.333f, 
        ImGui::GetContentRegionAvail().y), true
    );
    ImGui::Text("Model Explorer");
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void RenderMaterialExplorer()
{
    ImVec4 backgroundColour = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColour);
    ImGui::SameLine();
    ImGui::BeginChild(
        "Material Explorer", 
        ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 
        ImGui::GetContentRegionAvail().y), true
    );
    ImGui::Text("Material Explorer");
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void RenderTexturesPanel()
{
    ImVec4 backgroundColour = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColour);
    ImGui::SameLine();
    ImGui::BeginChild("Texture Panel", ImVec2(0, ImGui::GetContentRegionAvail().y), true);
    ImGui::Text("Texture Panel");
    ImGui::EndChild();
    ImGui::PopStyleColor();
}