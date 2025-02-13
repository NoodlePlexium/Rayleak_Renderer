#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <imgui.h>

// ADAPTED FROM Tor Klingberg https://stackoverflow.com/questions/3723846/convert-from-hex-color-to-rgb-struct-in-c
ImVec4 HexToRGBA(const char* hex)
{
    int r, g, b;
    // Mateen Ulhaq remove first char in array https://stackoverflow.com/questions/5711490/c-remove-the-first-character-of-an-array
    sscanf_s(hex + 1, "%02x%02x%02x", &r, &g, &b);
    return ImVec4(
        static_cast<float>(r) / 255,
        static_cast<float>(g) / 255, 
        static_cast<float>(b) / 255, 1.0f);
}

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

ImFont* LoadFont(const char* fontpath, float size)
{
    ImGuiIO& io = ImGui::GetIO();
    return io.Fonts->AddFontFromFileTTF(fontpath, size);
}

void PanelTitleBar(const char* label, ImFont* fontHeader)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#003c78"));
    ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA("#2e343b"));
    ImGui::BeginChild(label, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    ImGui::PushFont(fontHeader);
    ImGui::Dummy(ImVec2(1, 3));
    ImGui::Indent(6);
    ImGui::Text("%s", label); 
    ImGui::Unindent();
    ImGui::Dummy(ImVec2(1, 3));
    ImGui::PopFont();  
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

void RenderObjectsPanel(const std::vector<Mesh*> &meshes, float VIEWPORT_HEIGHT, ImFont* fontHeader)
{
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#000000"));
    ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA("#2e343b"));
    ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA("#1b1e21"));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
    ImGui::BeginChild("Objects", ImVec2(0, VIEWPORT_HEIGHT), true);
    PanelTitleBar("Objects", fontHeader);

    // OBJECTS CONTAINER
    ImGui::BeginChild("Objects Container", ImVec2(0, ImGui::GetContentRegionAvail().y), false);
    ImGui::Dummy(ImVec2(1, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));
    ImGui::Indent(3);
    for (int i=0; i<meshes.size(); ++i)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        if (ImGui::Button(meshes[i]->name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 3.0f, 0)))
        {

        }
        ImGui::PopStyleVar();
    }
    ImGui::Unindent();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    // OBJECTS CONTAINER

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

void RenderModelExplorer(ImFont* fontHeader)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA("#2e343b"));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
    ImGui::BeginChild("Model Explorer", ImVec2(ImGui::GetContentRegionAvail().x * 0.333f, ImGui::GetContentRegionAvail().y), true);
    PanelTitleBar("Model Explorer", fontHeader);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void RenderMaterialExplorer(ImFont* fontHeader)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA("#2e343b"));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
    ImGui::SameLine();
    ImGui::BeginChild("Material Explorer", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetContentRegionAvail().y), true);
    PanelTitleBar("Material Explorer", fontHeader);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void RenderTexturesPanel(ImFont* fontHeader)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA("#2e343b"));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
    ImGui::SameLine();
    ImGui::BeginChild("Texture Panel", ImVec2(0, ImGui::GetContentRegionAvail().y), true);
    PanelTitleBar("Texture Panel", fontHeader);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void FloatAttribute(std::string label, const char* id, const char* suffix, float margin, float padding, float* value)
{
    std::string frameID = "###" + std::string(id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Indent(margin); // MARGIN

    ImGui::BeginChild(frameID.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - margin, 0), ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    std::string uniqueID = "##" + std::string(id);
    std::string format = "%.2f" + std::string(suffix);
    ImGui::Dummy(ImVec2(1, padding)); // VERTICAL PADDING
    ImGui::Indent(padding); // HORIZONTAL PADDING
    ImGui::Text("%s", label.c_str());   
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f); 
    ImGui::PushItemWidth(100.0f);
    ImGui::InputFloat(uniqueID.c_str(), value, 0.0f, 0.0f, format.c_str());
    ImGui::PopItemWidth();
    ImGui::Unindent(padding); // HORIZONTAL PADDING
    ImGui::Dummy(ImVec2(1, padding)); // VERTICAL PADDING
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::PopStyleColor();
    ImGui::Unindent(margin); // MARGIN
}

void CheckboxAttribute(std::string label, const char* id, float margin, float padding, bool* value)
{
    std::string frameID = "###" + std::string(id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Indent(margin); // MARGIN

    ImGui::BeginChild(frameID.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - margin, 0), ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    std::string uniqueID = "##" + std::string(id);
    ImGui::Dummy(ImVec2(1, padding)); // VERTICAL PADDING
    ImGui::Indent(padding); // HORIZONTAL PADDING
    ImGui::Text("%s", label.c_str());   
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f); 
    ImGui::Checkbox(uniqueID.c_str(), value);
    ImGui::Unindent(padding); // HORIZONTAL PADDING
    ImGui::Dummy(ImVec2(1, padding)); // VERTICAL PADDING
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::PopStyleColor();
    ImGui::Unindent(margin); // MARGIN
}

void PaddedText(const char* text, float padding)
{
    ImGui::Indent(padding);
    ImGui::Text("Camera Settings");
    ImGui::Unindent(padding);
}