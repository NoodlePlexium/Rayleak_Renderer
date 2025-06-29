// EXTERNAL LIBRARIES
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include "../lib/tinyfiledialogs/tinyfiledialogs.h"
#include "thumbnail_renderer.h"

// STANDARD LIBRARY
#include <cstdint>

// PROJECT HEADERS
#include "material_manager.h"


class UserInterface
{
public:

    bool restartRender;

    UserInterface(const unsigned int _pathtraceShader)
    {
        pathtraceShader = _pathtraceShader;
        fontBody = LoadFont("fonts/Inter/Inter-VariableFont_opsz,wght.ttf", 18);
        fontHeader = LoadFont("fonts/Inter/Inter-VariableFont_opsz,wght.ttf", 24);
        LoadIcons();
    }

    void NewFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        restartRender = false;
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

    void LoadIcons()
    {
        OBJ_Icon.LoadImage("./icons/obj_icon.png");
        Blank_Texture.LoadImage("./icons/blank.png");
    }

    void BeginAppLayout()
    {
        ImGui::PushFont(fontBody);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, HexToRGBA(SCROLLBAR_BG));
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


        if (draggedModelIndex != -1)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                draggedModelReleased = true;
            }
        }

        if (releasingDraggedTexture)
        {
            draggedTextureIndex = -1;
            releasingDraggedTexture = false;
            droppedTextureIntoSlot = false;
        }
        
        if (draggedTextureIndex != -1)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                releasingDraggedTexture = true;
            }
        }

        if (draggedMaterialIndex != -1)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                releasingDraggedMaterial = true;
            }
        }
    }

    void EndAppLayout()
    {
        ImGui::End();
        ImGui::PopFont();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }

    void RenderViewportPanel(int width, int height, float frameTime, bool cursorOverViewport, unsigned int frameBufferTextureID, Camera& camera, ModelManager& modelManager, RenderSystem& renderSystem, const unsigned int &raycastShader)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("Viewport", ImVec2(width, height), true);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)frameBufferTextureID,
            cursorPos,
            ImVec2(cursorPos.x + width, cursorPos.y + height),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );

        ImGui::Dummy(ImVec2(0, 15));

        if (draggedModelReleased)
        {
            if (cursorOverViewport)
            {
                int instanceID = modelManager.CreateModelInstance(draggedModelIndex);
                modelManager.AddModelToScene(&modelManager.modelInstances[instanceID]);
                restartRender = true;
            }
            draggedModelIndex = -1;
            draggedModelReleased = false;
        }

        if (releasingDraggedMaterial)
        {
            if (cursorOverViewport)
            {
                ImVec2 mousePos = ImGui::GetMousePos();

        
                int y = height - mousePos.y + 3;
                int meshIndex = renderSystem.Raycast(raycastShader, camera, modelManager.meshCount, (int)mousePos.x, y);
                
                if (meshIndex >= 0 && meshIndex < modelManager.meshCount)
                {
                    modelManager.UpdateMeshMaterial( 
                        static_cast<uint32_t>(meshIndex), 
                        static_cast<uint32_t>(draggedMaterialIndex)
                    );
                    restartRender = true;
                }
            }
            releasingDraggedMaterial = false;
            draggedMaterialIndex = -1;
        }

        // EXPORT RENDER BUTTON
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::Dummy(ImVec2(15, 0));
        ImGui::SameLine();
        if (ImGui::Button("Export Render", ImVec2(280, 0)))
        {
            // ADAPTED FROM USER tinyfiledialogs https://stackoverflow.com/questions/6145910/cross-platform-native-open-save-file-dialogs
            const char *lFilterPatterns[1] = { "*.png" };
            const char* filename = tinyfd_saveFileDialog("Export Render", "render.png", 1, lFilterPatterns, "(*.png)");
            if (filename)
            {
                SaveRender(frameBufferTextureID, filename);
            }
        }
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 0));


        RenderSettingsPanel(camera, renderSystem);
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    void RenderSettingsPanel(Camera& camera, RenderSystem& renderSystem)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
        ImGui::BeginChild("Settings Panel", ImVec2(280.0f, 0), false);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, HexToRGBA(BORDER));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) 
        {
            // BEGIN CONTAINER
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, GAP));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(MATERIAL_EDITOR_BG));
            ImGui::BeginChild("Settings", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
            ImGui::Dummy(ImVec2(0, 0));

            // CAMERA SETTINGS
            bool changed = false;
            changed |= DragFloatAttribute("Field of View", "FOV", "", 3, 3, &camera.fov, 0, 120);
            changed |= CheckboxAttribute("Depth of Field", "DOF", 3, 3, &camera.dof);
            changed |= DragFloatAttribute("Focus Distance", "FOCUS", "m", 3, 3, &camera.focus_distance, 0.1, 500);
            changed |= DragFloatAttribute("fStops", "fStops", "", 3, 3, &camera.fStop, 0.05f, 5.0f);
            changed |= CheckboxAttribute("Anti Aliasing", "AA", 3, 3, &camera.anti_aliasing);
            changed |= DragFloatAttribute("Exposure", "EXPOSURE", "", 3, 3, &camera.exposure, 0.0f, 3.0f, 0.01f);

            // RENDER SETTINGS
            if (IntAttribute("Light Bounces", "BOUNCES", 3, &renderSystem.bounces, 1, 10))
            {
                renderSystem.ResizePathBuffer();
                changed = true;
            }

            // ENVIRONMENT SETTINGS
            changed |= ColourSelectAttribute("Sky colour", "###Sky Colour Button", "###Sky Colour", renderSystem.skyColour, skyColourPopupOpen, GAP, 3);
            changed |= DragFloatAttribute("Sky Brightness", "SKY BRIGHTNESS", "", 3, 3, &renderSystem.skyBrightness, 0.0f, 4.0f, 0.01f);
            if (changed) restartRender = true;

            // CLOSE CONTAINER
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::EndChild();
    }

    void BeginSidebar(float height)
    {
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::BeginChild("sidebar", ImVec2(0, height), true);
    }

    void RenderObjectsPanel(ModelManager& modelManager, float height)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#000000"));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::BeginChild("Objects", ImVec2(0, height), true);
        PanelTitleBar("Objects");

        // OBJECTS CONTAINER
        ImGui::Dummy(ImVec2(GAP, 0));
        ImGui::SameLine();
        ImGui::BeginChild("Objects Container", ImVec2(SpaceX() - GAP, SpaceY()), false);
        ImGui::Dummy(ImVec2(1, GAP));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, GAP));
        int meshIndex = 0;
        for (int i=0; i<modelManager.modelInstances.size(); ++i)
        {
            Model* model = &modelManager.modelInstances[i];
            if (model->inScene)
            {   
                // MODEL DROPDOWN
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Header, HexToRGBA(BORDER));
                if (ImGui::CollapsingHeader(model->name, ImGuiTreeNodeFlags_DefaultOpen)) 
                {
                    for (int j=0; j<model->submeshPtrs.size(); j++)
                    {
                        Mesh* mesh = model->submeshPtrs[j];

                        // BUTTON COLOUR
                        bool isSelectedMesh;
                        ImVec4 buttonColour = HexToRGBA(BUTTON);
                        if (selectedMesh == meshIndex) 
                        {
                            isSelectedMesh = true;
                            buttonColour = HexToRGBA(SELECTED);
                        }

                        // MESH BUTTON
                        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
                        std::string buttonID = "Mesh Button" + std::to_string(meshIndex);
                        ImGui::PushID(buttonID.c_str());
                        ImGui::PushStyleColor(ImGuiCol_Button, buttonColour);
                        if (ImGui::Button(mesh->name.c_str(), ImVec2(SpaceX()-25, 0)))
                        {
                            selectedMesh = meshIndex;
                            selectedDirectionalLight = -1;
                            selectedPointLight = -1;
                            selectedSpotlight = -1;
                        }
                        ImGui::PopStyleVar();

                        // DELETE MESH BUTTON
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
                        if (ImGui::Button("X", ImVec2(SpaceX(), 0)))
                        {
                            if (selectedMesh == meshIndex) selectedMesh = -1;
                            else if (selectedMesh != -1 && selectedMesh > meshIndex) selectedMesh--;
                            modelManager.DeleteInstanceMesh(i, j, meshIndex);
                            restartRender = true;
                        }

                        meshIndex++;
                        ImGui::PopID();
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor();
                    }
                }
                else
                {
                    meshIndex += model->submeshPtrs.size();
                }
                ImGui::PopID();
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        // OBJECTS CONTAINER

        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    void RenderLightsPanel(LightManager &lightManager, float height)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#000000"));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::BeginChild("Lights", ImVec2(0, height), true);
        PanelTitleBar("Lights");

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(GAP, 0)); 
        ImGui::BeginChild("ADD LIGHTS CONTAINER", ImVec2(SpaceX() - GAP, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::Dummy(ImVec2(0, GAP)); 
        ImGui::Dummy(ImVec2(0, 0)); ImGui::SameLine();
        if (ImGui::Button("Dir", ImVec2(SpaceX() / 3.0f, 0)))
        {
            lightManager.AddDirectionalLight();
            restartRender = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Point", ImVec2(SpaceX() * 0.5f, 0)))
        {
            lightManager.AddPointLight();
            restartRender = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Spot", ImVec2(SpaceX(), 0)))
        {
            lightManager.AddSpotlight();
            restartRender = true;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();


        // LIGHTS CONTAINER
        ImGui::Dummy(ImVec2(GAP, 0));
        ImGui::SameLine();
        ImGui::BeginChild("Lights Container", ImVec2(SpaceX() - GAP, SpaceY()), false);
        ImGui::Dummy(ImVec2(1, GAP));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, GAP));

        int lightID = 0;

        // CREATE DIRECTIONAL LIGHT BUTTONS
        for (int i=0; i<lightManager.directionalLights.size(); ++i)
        {
            ImVec4 buttonColour = HexToRGBA(BUTTON);
            if (selectedDirectionalLight == i) buttonColour = HexToRGBA(SELECTED);

            ImGui::PushID(lightID++);
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColour);
            if (ImGui::Button(lightManager.directionalLightNames[i].c_str(), ImVec2(SpaceX() - 25, 0)))
            {
                selectedDirectionalLight = i;
                selectedPointLight = -1;
                selectedSpotlight = -1;
                selectedMesh = -1;
            }

            // DELETE LIGHT BUTTON
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(SpaceX(), 0)))
            {
                if (selectedDirectionalLight == i) selectedDirectionalLight = -1;
                else if (selectedDirectionalLight != -1 && selectedDirectionalLight > i) selectedDirectionalLight--;
                lightManager.DeleteDirectionalLight(i);
                restartRender = true;
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }   

        // CREATE POINT LIGHT BUTTONS
        for (int i=0; i<lightManager.pointLights.size(); ++i)
        {
            ImVec4 buttonColour = HexToRGBA(BUTTON);
            if (selectedPointLight == i) buttonColour = HexToRGBA(SELECTED);

            ImGui::PushID(lightID++);
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColour);
            if (ImGui::Button(lightManager.pointLightNames[i].c_str(), ImVec2(SpaceX()-25, 0)))
            {
                selectedDirectionalLight = -1;
                selectedPointLight = i;
                selectedSpotlight = -1;
                selectedMesh = -1;
            }

            // DELETE LIGHT BUTTON
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(SpaceX(), 0)))
            {
                if (selectedPointLight == i) selectedPointLight = -1;
                else if (selectedPointLight != -1 && selectedPointLight > i) selectedPointLight--;
                lightManager.DeletePointLight(i);
                restartRender = true;
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        // CREATE SPOTLIGHT BUTTONS
        for (int i=0; i<lightManager.spotlights.size(); ++i)
        {
            ImVec4 buttonColour = HexToRGBA(BUTTON);
            if (selectedSpotlight == i) buttonColour = HexToRGBA(SELECTED);

            ImGui::PushID(lightID++);
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColour);
            if (ImGui::Button(lightManager.spotlightNames[i].c_str(), ImVec2(SpaceX()-25, 0)))
            {
                selectedDirectionalLight = -1;
                selectedPointLight = -1;
                selectedSpotlight = i;
                selectedMesh = -1;
            }
            // DELETE LIGHT BUTTON
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(SpaceX(), 0)))
            {
                if (selectedSpotlight == i) selectedSpotlight = -1;
                else if (selectedSpotlight != -1 && selectedSpotlight > i) selectedSpotlight--;
                lightManager.DeleteSpotlight(i);
                restartRender = true;
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        // LIGHTS CONTAINER

        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    void RenderTransformPanel(ModelManager& modelManager, LightManager& lightManager, float height)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#000000"));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::BeginChild("Transform", ImVec2(0, height), false);
        PanelTitleBar("Transform");

        ImGui::Dummy(ImVec2(GAP, 0));
        ImGui::SameLine();
        ImGui::BeginChild("Transform Container", ImVec2(SpaceX()-GAP, SpaceY()), false);
        ImGui::Dummy(ImVec2(1, GAP));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, GAP));
        
        // IF MESH IS SELECTED
        if (selectedMesh != -1)
        {
            Mesh* mesh = modelManager.meshes[selectedMesh];

            bool changed = false;
            changed |= TransformAttribute("position", GAP, &mesh->position.x, &mesh->position.y, &mesh->position.z); ImGui::Dummy(ImVec2(0, 0));
            changed |= TransformAttribute("rotation", GAP, &mesh->rotation.x, &mesh->rotation.y, &mesh->rotation.z); ImGui::Dummy(ImVec2(0, 0));
            changed |= TransformAttribute("scale",    GAP, &mesh->scale.x,    &mesh->scale.y,    &mesh->scale.z);    ImGui::Dummy(ImVec2(0, 0));
            restartRender |= changed;

            mesh->UpdateInverseTransformMat();
            if (changed) modelManager.UpdateMeshTransform(mesh, selectedMesh);
        }

        // IF DIRECTIONAL LIGHT IS SELECTED
        if (selectedDirectionalLight != -1)
        {
            DirectionalLight* light = &lightManager.directionalLights[selectedDirectionalLight];
            bool changed = false;
            changed |= TransformAttribute("rotation", GAP, &light->rotation.x, &light->rotation.y, &light->rotation.z); ImGui::Dummy(ImVec2(0, 0));
            changed |= FloatAttribute("brightness", "##BRIGHTNESS", "", 3, 0, &light->brightness, 0, 1000);
            
            // LIGHT COLOUR SELECTION
            changed |= ColourSelectAttribute("colour", "###COLOUR", "###Light Colour", light->colour, lightColourPopupOpen, GAP, 0);

            restartRender |= changed;


            light->TransformDirection();
            if (changed) lightManager.UpdateDirectionalLight(selectedDirectionalLight);
        }

        // IF POINT LIGHT IS SELECTED
        if (selectedPointLight != -1)
        {
            PointLight* light = &lightManager.pointLights[selectedPointLight];
            bool changed = false;
            changed |= TransformAttribute("position", GAP, &light->position.x, &light->position.y, &light->position.z); ImGui::Dummy(ImVec2(0, 0));
            changed |= FloatAttribute("brightness", "##BRIGHTNESS", "", 3, 0, &light->brightness, 0, 1000);
            
            // LIGHT COLOUR SELECTION
            changed |= ColourSelectAttribute("colour", "###COLOUR", "###Light Colour", light->colour, lightColourPopupOpen, GAP, 0);
        
            restartRender |= changed;

            if (changed) lightManager.UpdatePointLight(selectedPointLight);
        }

        // IF SPOTLIGHT IS SELECTED
        if (selectedSpotlight != -1)
        {
            Spotlight* light = &lightManager.spotlights[selectedSpotlight];
            bool changed = false;
            changed |= TransformAttribute("position", GAP, &light->position.x, &light->position.y, &light->position.z); ImGui::Dummy(ImVec2(0, 0));
            changed |= TransformAttribute("rotation", GAP, &light->rotation.x, &light->rotation.y, &light->rotation.z); ImGui::Dummy(ImVec2(0, 0));
            changed |= FloatAttribute("brightness", "##BRIGHTNESS", "", 3, 0, &light->brightness, 0, 1000); 
            changed |= FloatAttribute("angle", "##ANGLE", "", 3, 0, &light->angle, 0, 360); 
            changed |= FloatAttribute("falloff", "##FALLOFF", "", 3, 0, &light->falloff, 0, 360 - light->angle);

            // LIGHT COLOUR SELECTION
            changed |= ColourSelectAttribute("colour", "###COLOUR", "###Light Colour", light->colour, lightColourPopupOpen, GAP, 0);

            restartRender |= changed;

            light->TransformDirection();
            if (changed) lightManager.UpdateSpotlight(selectedSpotlight);
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        // OBJECTS CONTAINER

        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    void EndSidebar()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    void RenderModelExplorer(ModelManager& modelManager)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::BeginChild("Model Explorer", ImVec2(SpaceX() * 0.333f, SpaceY()), true);
        ModelPanelHeader(modelManager);

        float materialContainerWidth = 0;

        // MODEL CONTAINER GRID
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MODEL_PANEL_GAP, MODEL_PANEL_GAP));
        ImGui::BeginChild("Model Container", ImVec2(SpaceX() - materialContainerWidth, SpaceY()), false);
        ImGui::Dummy(ImVec2(1, 0));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + MODEL_PANEL_GAP);
        float hSpace = SpaceX() - materialContainerWidth;
        float usedHSpace = MODEL_PANEL_GAP;
        for (int i=0; i<modelManager.models.size(); i++)
        {   
            if (usedHSpace + MODEL_THUMBNAIL_SIZE < hSpace && i > 0) 
            {
                ImGui::SameLine();
            }
            else if (i > 0)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + MODEL_PANEL_GAP);
                usedHSpace = MODEL_PANEL_GAP;
            }
            ModelComponent(modelManager.models[i], i);
            usedHSpace += MODEL_THUMBNAIL_SIZE + MODEL_PANEL_GAP;
        }
        ImGui::Dummy(ImVec2(1, 0));
        ImGui::EndChild();
        ImGui::PopStyleVar();
    

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void RenderMaterialExplorer(MaterialManager& materialManager, RenderSystem& renderSystem)
    {   
        // MATERIAL EXPLORER PANEL
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,1));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); 
        ImGui::BeginChild("Material Explorer", ImVec2(SpaceX() * 0.6f, SpaceY()), true);
        
        // MATERIAL EXPLORER TITLE BAR
        MaterialPanelHeader(materialManager, renderSystem);

        float materialEditorWidth = 280;

        // MATERIAL CONTAINER GRID
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MATERIAL_PANEL_GAP, MATERIAL_PANEL_GAP));
        ImGui::BeginChild("Material Container", ImVec2(SpaceX()-materialEditorWidth, SpaceY()), false);
        ImGui::Dummy(ImVec2(1,0));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + MATERIAL_PANEL_GAP);
        float hSpace = SpaceX();
        float usedHSpace = MATERIAL_PANEL_GAP;
        for (int i=1; i<materialManager.materials.size(); i++)
        {   
            if (usedHSpace + MODEL_THUMBNAIL_SIZE < hSpace && i > 1) { ImGui::SameLine(); }
            else if (i > 1)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + MATERIAL_PANEL_GAP);
                usedHSpace = MATERIAL_PANEL_GAP;
            }
            MaterialComponent(materialManager.materials[i], i);
            usedHSpace += MODEL_THUMBNAIL_SIZE + MATERIAL_PANEL_GAP;
        }
        ImGui::Dummy(ImVec2(1,0));
        ImGui::EndChild();
        ImGui::PopStyleVar();
        // END MATERIAL CONTAINER GRID


        // MATERIAL EDITOR CONTAINER
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(MATERIAL_EDITOR_BG));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, GAP));
        ImGui::BeginChild("Material Editor", ImVec2(SpaceX(), SpaceY()), false);

        if (selectedMaterialIndex != -1)
        {   
            bool updatedMaterial = false;
            MaterialData& materialData = materialManager.materials[selectedMaterialIndex].data;
            std::string baseID = std::to_string(selectedMaterialIndex).c_str();
            PaddedText(materialManager.materials[selectedMaterialIndex].name, 5);

            // MATERIAL COLOUR PICKER
            updatedMaterial  |= ColourSelectAttribute("colour", "###Material Colour", "### Material Colour Button", materialData.colour, materialColourPopupOpen, GAP, 0);
            std::string roughnessID = std::string("Material Roughness") + baseID;
            std::string emissionID = std::string("Material Emission") + baseID;
            std::string refractiveID = std::string("Material Refractive") + baseID;
            std::string iorID = std::string("Material IOR") + baseID;
            updatedMaterial |= DragFloatAttribute("roughness", roughnessID.c_str(), "", 3, 0, &materialData.roughness, 0, 1, 0.01f);
            updatedMaterial |= DragFloatAttribute("emission", emissionID.c_str(), "", 3, 0, &materialData.emission, 0, 1000);
            bool refractive = materialData.refractive == 1;
            updatedMaterial |= CheckboxAttribute("refractive", refractiveID.c_str(), 3, 0, &refractive);
            updatedMaterial |= DragFloatAttribute("IOR", iorID.c_str(), "", 3, 0, &materialData.IOR, 0, 5);

            // HANDLE REFRACTIVE NON BOOL CASE
            if (refractive) materialData.refractive = 1;
            else materialData.refractive = 0;

            ImGui::Dummy(ImVec2(0, 0)); ImGui::SameLine();
            MaterialTextureSlot("Albedo Map", selectedMaterialIndex, materialManager, 0, updatedMaterial);
            ImGui::Dummy(ImVec2(0, 0)); ImGui::SameLine();
            MaterialTextureSlot("Normal Map", selectedMaterialIndex, materialManager, 1, updatedMaterial);
            ImGui::Dummy(ImVec2(0, 0)); ImGui::SameLine();
            MaterialTextureSlot("Roughness Map", selectedMaterialIndex, materialManager, 2, updatedMaterial);


            if (updatedMaterial) 
            {
                // UPDATE MATERIAL SETTINGS ON THE GPU
                materialManager.UpdateMaterial(materialData, selectedMaterialIndex);

                // RERENDER MATERIAL THUMBNAIL
                renderSystem.RenderThumbnail(materialManager.materials[selectedMaterialIndex], MATERIAL_THUMBNAIL_SIZE, MATERIAL_THUMBNAIL_SIZE);

                // RESTART RENDER
                restartRender = true;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        // END MATERIAL EDITOR CONTAINER

        // END MATERIAL PANEL 
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    void RenderTexturesPanel(MaterialManager &materialManager)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::SameLine();
        ImGui::BeginChild("Texture Panel", ImVec2(0, SpaceY()), true);
        TexturePanelHeader(materialManager);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(TEXTURE_PANEL_GAP, TEXTURE_PANEL_GAP));
        ImGui::BeginChild("Textures Container", ImVec2(SpaceX(), SpaceY()), false);
        ImGui::Dummy(ImVec2(1, 0));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + TEXTURE_PANEL_GAP);
        float hSpace = SpaceX();
        float usedHSpace = TEXTURE_PANEL_GAP;
        for (int i=0; i<materialManager.textures.size(); i++)
        {   
            if (usedHSpace + TEXTURE_THUMBNAIL_SIZE < hSpace && i > 0) 
            {
                ImGui::SameLine();
            }
            else if (i > 0)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + TEXTURE_PANEL_GAP);
                usedHSpace = TEXTURE_PANEL_GAP;
            }
            TextureComponent(materialManager.textures[i], i);
            usedHSpace += TEXTURE_THUMBNAIL_SIZE + TEXTURE_PANEL_GAP;
        }

        ImGui::Dummy(ImVec2(1, 0));
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

private:
    unsigned int pathtraceShader;

    ImFont* fontBody;
    ImFont* fontHeader;
    Texture OBJ_Icon;
    Texture Blank_Texture;

    // STYLING
    const char* BORDER = "#3d444d";
    const char* ATTRIBUTE_BG = "#3d444d";
    const char* INPUT_BG = "#02050a";
    const char* MATERIAL_EDITOR_BG = "#121314";
    const char* SCROLLBAR_BG = "#0e0e0f";
    const char* BUTTON = "#1a2129";
    const char* BUTTON_HOVER = "#516c8a";
    const char* SELECTED = "#387ccf";
    const float GAP = 3;
    const float TEXTURE_PANEL_GAP = 5;
    const float TEXTURE_THUMBNAIL_SIZE = 125;
    const float MODEL_PANEL_GAP = 5;
    const float MODEL_THUMBNAIL_SIZE = 104;
    const float MATERIAL_PANEL_GAP = 5;
    const float MATERIAL_THUMBNAIL_SIZE = 104;
    const float MATERIAL_TEXTURE_SLOT_SIZE = 100;

    // MODEL PANEL CONTROLS
    int draggedModelIndex = -1;
    bool draggedModelReleased = false;

    // MATERIAL PANEL CONTROLS
    int selectedMaterialIndex = -1;
    int draggedMaterialIndex = -1;
    bool releasingDraggedMaterial = false;

    // TEXTURE PANEL CONTROLS
    int draggedTextureIndex = -1;
    bool releasingDraggedTexture = false;
    bool droppedTextureIntoSlot = false;

    // TRANSFORM PANEL CONTROLS
    int selectedMesh = -1;
    int selectedDirectionalLight = -1;
    int selectedPointLight = -1;
    int selectedSpotlight = -1;
    bool lightColourPopupOpen = false;
    bool materialColourPopupOpen = false;

    // SKY CONTROLS
    bool skyColourPopupOpen = false;


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
    
    ImFont* LoadFont(const char* fontpath, float size)
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.Fonts->AddFontFromFileTTF(fontpath, size);
    }

    float SpaceX()
    {
        return ImGui::GetContentRegionAvail().x;
    }

    float SpaceY()
    {
        return ImGui::GetContentRegionAvail().y;
    }

    void PaddedText(const char* text, float padding)
    {
        ImGui::Indent(padding);
        ImGui::Text("%s", text); 
        ImGui::Unindent(padding);
    }

    // }----------{ ATTRIBUTE COMPONENTS }----------{
    bool FloatAttribute(std::string label, const char* id, const char* suffix, float padding, float margin, float* value, float min, float max)
    {
        bool changed = false;
        std::string frameID = "###" + std::string(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(INPUT_BG));

        ImGui::Dummy(ImVec2(margin, 1)); 
        ImGui::SameLine();

        ImGui::BeginChild(frameID.c_str(), ImVec2(SpaceX() - margin, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        std::string uniqueID = "##" + std::string(id);
        std::string format = "%.2f" + std::string(suffix);
        ImGui::Dummy(ImVec2(1, padding));
        ImGui::Indent(padding); 
        ImGui::Text("%s", label.c_str());   
        ImGui::SameLine(SpaceX() - 100); 
        ImGui::PushItemWidth(100);

        float tempValue = *value;
        ImGui::InputFloat(uniqueID.c_str(), &tempValue, 0, 0, format.c_str());
        if (tempValue < min) tempValue = min;
        else if (tempValue > max) tempValue = max;
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            changed = *value != tempValue; 
            *value = tempValue; 
        }
        ImGui::PopItemWidth();
        ImGui::Unindent(padding); 
        ImGui::Dummy(ImVec2(1, padding)); 
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        return changed;
    }

    bool DragFloatAttribute(std::string label, const char* id, const char* suffix, float padding, float margin, float* value, float min, float max, float step=0.1f)
    {
        bool changed = false;
        std::string frameID = "###" + std::string(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(INPUT_BG));

        ImGui::Dummy(ImVec2(margin, 1));
        ImGui::SameLine();

        ImGui::BeginChild(frameID.c_str(), ImVec2(SpaceX() - margin, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        std::string uniqueID = "##" + std::string(id);
        std::string format = "%.2f" + std::string(suffix);
        ImGui::Dummy(ImVec2(1, padding));
        ImGui::Indent(padding); 
        ImGui::Text("%s", label.c_str());   
        ImGui::SameLine(SpaceX() - 100); 
        ImGui::PushItemWidth(100);

        float tempValue = *value;
        ImGui::DragFloat(uniqueID.c_str(), &tempValue, step, min, max);
        changed = *value != tempValue; 
        *value = tempValue; 
        ImGui::PopItemWidth();
        ImGui::Unindent(padding); 
        ImGui::Dummy(ImVec2(1, padding)); 
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        return changed;
    }

    bool IntAttribute(std::string label, const char* id, float padding, int* value, int min, int max)
    {
        bool changed = false;
        std::string frameID = "###" + std::string(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(INPUT_BG));

        ImGui::Dummy(ImVec2(padding, 1)); 
        ImGui::SameLine();

        ImGui::BeginChild(frameID.c_str(), ImVec2(SpaceX() - padding, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::Dummy(ImVec2(1, padding));
        ImGui::Indent(padding); 
        ImGui::Text("%s", label.c_str());   
        ImGui::SameLine(SpaceX() - 100); 

        // TEXT INPUT
        ImGui::PushItemWidth(44);
        int tempValue = *value;
        std::string inputID = "##" + std::string(id);
        ImGui::InputInt(inputID.c_str(), &tempValue, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            changed = *value != tempValue; 
            *value = tempValue; 
        }

        // SUBTRACTION BUTTON
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(padding, 0));
        std::string subID = inputID + "+";
        ImGui::PushID(subID.c_str());
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(25, 0)) && *value > min)
        {
            changed = true;
            *value -= 1;
        }
        ImGui::PopID();

        // ADDITION BUTTON
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(padding, 0));
        std::string addID = inputID + "+";
        ImGui::PushID(addID.c_str());
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(25, 0)) && *value < max) 
        {
            changed = true;
            *value += 1;
        }
        ImGui::PopID();

        // CLAMP VALUE
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        ImGui::PopItemWidth();
        ImGui::Unindent(padding); 
        ImGui::Dummy(ImVec2(1, padding)); 
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        return changed;
    }

    bool CheckboxAttribute(std::string label, const char* id, float padding, float margin, bool* value)
    {
        bool changed = false;
        std::string frameID = "###" + std::string(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(INPUT_BG));

        ImGui::Dummy(ImVec2(margin, 1)); 
        ImGui::SameLine();

        ImGui::BeginChild(frameID.c_str(), ImVec2(SpaceX() - margin, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        std::string uniqueID = "##" + std::string(id);
        ImGui::Dummy(ImVec2(1, padding)); // VERTICAL PADDING
        ImGui::Indent(padding); // HORIZONTAL PADDING
        ImGui::Text("%s", label.c_str());   
        ImGui::SameLine(SpaceX() - 100.0f); 
        if (ImGui::Checkbox(uniqueID.c_str(), value))
        {
            changed = true;
        }
        ImGui::Unindent(padding); // HORIZONTAL PADDING
        ImGui::Dummy(ImVec2(1, padding)); // VERTICAL PADDING
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        return changed;
    }

    bool TransformAttribute(std::string label, float padding, float* x, float* y, float* z)
    {   
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(INPUT_BG));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::BeginChild(label.c_str(), ImVec2(SpaceX(), 0), ImGuiChildFlags_AutoResizeY);

        ImGui::Dummy(ImVec2(0, padding)); 
        ImGui::Dummy(ImVec2(padding, 0)); 
        ImGui::SameLine();

        std::string fakeInputID = "##fakeinput" + label;
        std::string labelID = "##inputlabel" + label;
        std::string xFrameID = "##inputframex" + label;
        std::string yFrameID = "##inputframey" + label;
        std::string zFrameID = "##inputframez" + label;
        std::string xID = "##inputx" + label;
        std::string yID = "##inputy" + label;
        std::string zID = "##inputz" + label;
        float tempX = *x;
        float tempY = *y;
        float tempZ = *z;



        float frameHeight = ImGui::GetFrameHeight();
        float labelWidth = 80;
        float labelHeight = ImGui::CalcTextSize(label.c_str()).y;
        float labelPadX = SpaceX() - labelWidth - 210.0f - padding * 3;
        float labelPadY = (frameHeight - labelHeight) * 0.5f;

        // LABEL
        ImGui::BeginChild(labelID.c_str(), ImVec2(labelWidth, frameHeight));
            ImGui::Dummy(ImVec2(0, labelPadY));
            ImGui::Text("%s", label.c_str());
        ImGui::EndChild();

        ImGui::SameLine(); 
        ImGui::Dummy(ImVec2(labelPadX, 0));

        // X INPUT
        ImGui::SameLine(); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#00911f"));
        ImGui::BeginChild(xFrameID.c_str(), ImVec2(20, frameHeight));
            labelWidth = ImGui::CalcTextSize("X").x;
            labelHeight = ImGui::CalcTextSize("X").y;
            labelPadY = (frameHeight - labelHeight) * 0.5f;
            labelPadX = (20 - labelWidth) * 0.5f;
            ImGui::Dummy(ImVec2(0, labelPadY));
            ImGui::Dummy(ImVec2(labelPadX, 0));
            ImGui::SameLine();
            ImGui::Text("%s", "X");
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(labelPadX, 0));
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushItemWidth(50); ImGui::InputFloat(xID.c_str(), &tempX, 0, 0, "%.2f"); ImGui::PopItemWidth();
        ImGui::SameLine(); 
        ImGui::Dummy(ImVec2(padding, 0));

        // Y INPUT
        ImGui::SameLine(); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#003891"));
        ImGui::BeginChild(yFrameID.c_str(), ImVec2(20, frameHeight));
            labelWidth = ImGui::CalcTextSize("Y").x;
            labelHeight = ImGui::CalcTextSize("Y").y;
            labelPadY = (frameHeight - labelHeight) * 0.5f;
            labelPadX = (20 - labelWidth) * 0.5f;
            ImGui::Dummy(ImVec2(0, labelPadY));
            ImGui::Dummy(ImVec2(labelPadX, 0));
            ImGui::SameLine();
            ImGui::Text("%s", "Y");
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(labelPadX, 0));
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushItemWidth(50); ImGui::InputFloat(yID.c_str(), &tempY, 0, 0, "%.2f"); ImGui::PopItemWidth();
        ImGui::SameLine(); 
        ImGui::Dummy(ImVec2(padding, 0));

        // Z INPUT
        ImGui::SameLine(); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA("#910000"));
        ImGui::BeginChild(zFrameID.c_str(), ImVec2(20, frameHeight));
            labelWidth = ImGui::CalcTextSize("Z").x;
            labelHeight = ImGui::CalcTextSize("Z").y;
            labelPadY = (frameHeight - labelHeight) * 0.5f;
            labelPadX = (20 - labelWidth) * 0.5f;
            ImGui::Dummy(ImVec2(0, labelPadY));
            ImGui::Dummy(ImVec2(labelPadX, 0));  
            ImGui::SameLine();  
            ImGui::Text("%s", "Z");
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(labelPadX, 0));
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushItemWidth(50); ImGui::InputFloat(zID.c_str(), &tempZ, 0, 0, "%.2f"); ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0, padding)); 
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        bool changed = false;
        changed |= tempX != *x;
        changed |= tempY != *y;
        changed |= tempZ != *z;
        *x = tempX;
        *y = tempY;
        *z = tempZ;
        return changed;
    }

    bool ColourSelectAttribute(const char* label, const char* colourButtonID, const char* colourPickerID, glm::vec3 &colour, bool &colourPopupOpen, float padding, float margin)
    {

        // COLOUR BUTTON ATTRIBUTE
        std::string frameID = "###" + std::string(colourButtonID);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(INPUT_BG));
        ImGui::Dummy(ImVec2(margin, 1));
        ImGui::SameLine();
        ImGui::BeginChild(frameID.c_str(), ImVec2(SpaceX() - margin, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        std::string uniqueID = "##" + std::string(colourButtonID);
        ImGui::Dummy(ImVec2(1, padding));
        ImGui::Indent(padding); 
        ImGui::Text("%s", label);   
        ImGui::SameLine(SpaceX() - 100); 
        ImGui::PushItemWidth(100);
        ImVec4 col(colour.x, colour.y, colour.z, 1);
        bool colourButtonClicked = ImGui::ColorButton(colourButtonID, col, 0, ImVec2(100, 0));
        ImGui::PopItemWidth();
        ImGui::Unindent(padding); 
        ImGui::Dummy(ImVec2(1, padding)); 
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);

        // COLOUR SELECTION POPUP
        bool changed = false;
        if (colourButtonClicked) colourPopupOpen = true;
        if (colourPopupOpen)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
            ImGui::OpenPopup(label); 
            ImGui::BeginPopup(label);
            ImGui::Text("%s", label); 
            float col[3];
            col[0] = colour.x;
            col[1] = colour.y;
            col[2] = colour.z;
            ImGui::ColorPicker3(colourPickerID, col, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview);
            changed |= col[0] != colour.x;
            changed |= col[1] != colour.y;
            changed |= col[2] != colour.z;
            colour.x = col[0];
            colour.y = col[1];
            colour.z = col[2];

            if (!ImGui::IsAnyItemHovered() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
                colourPopupOpen = false; 
            }

            ImGui::EndPopup();
            ImGui::PopStyleVar();
        }
        return changed;
    }

    void PanelTitleBar(const char* label)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::BeginChild(label, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushFont(fontHeader);
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::Indent(6);
        ImGui::Text("%s", label); 
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::PopFont();  
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }
    
    void ImageComponent(float width, float height, unsigned int textureID)
    {
        std::string uniqueID = "Image Component" + std::to_string(textureID);
        ImGui::BeginChild(uniqueID.c_str(), ImVec2(width, height), false);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            textureID,
            cursorPos,
            ImVec2(cursorPos.x + TEXTURE_THUMBNAIL_SIZE, cursorPos.y + TEXTURE_THUMBNAIL_SIZE),
            ImVec2(0, 1),
            ImVec2(1, 0)
        );
        ImGui::EndChild();
    }

    // }----------{ MODEL EXPLORER PANEL }----------{
    void ModelPanelHeader(ModelManager& modelManager)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::BeginChild("Model Explorer", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushFont(fontHeader);
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::Indent(6);
        ImGui::Text("%s", "Model Explorer"); 
        ImGui::PopFont();  
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(SpaceX() - 120.0f - GAP, 0));
        ImGui::SameLine();
        if (ImGui::Button("Import OBJ", ImVec2(120.0f, 0)))
        {   
            // ADAPTED FROM USER tinyfiledialogs https://stackoverflow.com/questions/6145910/cross-platform-native-open-save-file-dialogs
            const char *lFilterPatterns[1] = { "*.obj" };
            const char* selection = tinyfd_openFileDialog("Import OBJ", "C:\\", 1,lFilterPatterns, NULL, 0 );
            if (selection)
            {
                modelManager.LoadModel(selection);
            }
        }
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    void ModelComponent(Model& model, int i)
    {
        std::string uniqueID = "Model Element " + std::to_string(i);
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::BeginChild(uniqueID.c_str(), ImVec2(MODEL_THUMBNAIL_SIZE, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        // BORDER COLOUR
        ImVec4 borderColour = HexToRGBA(BUTTON);
        ImVec4 hoverColour = HexToRGBA(BUTTON_HOVER);
        if (draggedModelIndex == i) {
            borderColour = HexToRGBA(SELECTED);
            hoverColour = borderColour;
        }

        // MODEL THUMBNAIL
        std::string buttonID = "Model Element Button " + std::to_string(i);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, borderColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverColour);
        if (ImGui::ImageButton(
            buttonID.c_str(), 
            OBJ_Icon.textureID, 
            ImVec2(MODEL_THUMBNAIL_SIZE-4, MODEL_THUMBNAIL_SIZE-2)))
        {
        
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            draggedModelIndex = i;
        }
        
        // NAME INPUT
        std::string nameInputID = "Model Element Name " + std::to_string(i);
        ImGui::BeginChild(nameInputID.c_str(), ImVec2(MODEL_THUMBNAIL_SIZE, 0), ImGuiChildFlags_AutoResizeY);
        std::string textInputID = "Model Element Name Input " + std::to_string(i);
        
        ImGui::PushItemWidth(MODEL_THUMBNAIL_SIZE);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(BUTTON));
        ImGui::InputText(textInputID.c_str(), model.tempName, 32);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            strcpy_s(model.name, 32, model.tempName);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();


        ImGui::SetCursorPosX(cursorPos.x + MODEL_PANEL_GAP);
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void ModelMaterialComponent(const char* materialName, int i)
    {
        std::string uniqueID = "Model Material Element " + std::to_string(i);
        std::string buttonID = "Model Material Element Button " + std::to_string(i);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(BORDER));
        ImGui::BeginChild(uniqueID.c_str(), ImVec2(SpaceX(), 0), ImGuiChildFlags_AutoResizeY);

        ImGui::Text("%s", materialName);
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();

        if (ImGui::Button(buttonID.c_str(), ImVec2(SpaceX(), 0)))
        {

        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // }----------{ MATERIAL EXPLORER PANEL }----------{
    void MaterialPanelHeader(MaterialManager &materialManager, RenderSystem& renderSystem)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::BeginChild("Material Explorer", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushFont(fontHeader);
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::Indent(6);
        ImGui::Text("%s", "Material Explorer"); 
        ImGui::PopFont();  
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(SpaceX() - 120.0f - GAP, 0));
        ImGui::SameLine();
        if (ImGui::Button("Create Material", ImVec2(120.0f, 0)))
        {   
            materialManager.CreateMaterial(MATERIAL_THUMBNAIL_SIZE, renderSystem);
        }
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    void MaterialComponent(Material& material, int i)
    {
        std::string uniqueID = "Material Element " + std::to_string(i);
        std::string buttonID = "Material Element Button " + std::to_string(i);
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::BeginChild(uniqueID.c_str(), ImVec2(MODEL_THUMBNAIL_SIZE, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        // BORDER COLOUR
        ImVec4 borderColour = HexToRGBA(BUTTON);
        ImVec4 hoverColour = HexToRGBA(BUTTON_HOVER);
        if (selectedMaterialIndex == i) {
            borderColour = HexToRGBA(SELECTED);
            hoverColour = borderColour;
        }

        // MATERIAL THUMBNAIL
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, borderColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverColour);
        if (ImGui::ImageButton(
            buttonID.c_str(), 
            material.FrameBufferTextureID, 
            ImVec2(MODEL_THUMBNAIL_SIZE-4, MODEL_THUMBNAIL_SIZE-2)))
        {
            selectedMaterialIndex = i;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            draggedMaterialIndex = i;
        }

        // NAME INPUT
        std::string nameInputID = "Material Element Name " + std::to_string(i);
        ImGui::BeginChild(nameInputID.c_str(), ImVec2(MODEL_THUMBNAIL_SIZE, 0), ImGuiChildFlags_AutoResizeY);
        std::string textInputID = "Material Element Name Input " + std::to_string(i);
        
        ImGui::PushItemWidth(MODEL_THUMBNAIL_SIZE);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(BUTTON));
        ImGui::InputText(textInputID.c_str(), material.tempName, 32);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            strcpy_s(material.name, 32, material.tempName);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();
        // END NAME INPUT

        ImGui::SetCursorPosX(cursorPos.x + MODEL_PANEL_GAP);
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void MaterialTextureSlot(std::string label, int materialIndex, MaterialManager &materialManager, int textureType, bool &updatedMaterial)
    {   
        unsigned int textureID = Blank_Texture.textureID;
        Material &material = materialManager.materials[materialIndex];
        if (textureType == 0 && (material.data.textureFlags & (1 << 0)) != 0) textureID = material.albedoID;
        if (textureType == 1 && (material.data.textureFlags & (1 << 1)) != 0) textureID = material.normalID;
        if (textureType == 2 && (material.data.textureFlags & (1 << 2)) != 0) textureID = material.roughnessID;

        std::string uniqueID = std::string("Material Texture Slot") + label;
        std::string buttonID = std::string("Material Texture Slot Button") + label;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(ATTRIBUTE_BG));
        ImGui::BeginChild(uniqueID.c_str(), ImVec2(SpaceX(), 0), ImGuiChildFlags_AutoResizeY);

        // LABEL
        PaddedText(label.c_str(), 5);
        
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(SpaceX() - MATERIAL_TEXTURE_SLOT_SIZE - MATERIAL_PANEL_GAP, 0));
        ImGui::SameLine();

        // BORDER COLOUR
        ImVec4 borderColour = HexToRGBA(BUTTON);
        ImVec4 hoverColour = HexToRGBA(BUTTON_HOVER);

        // CHECK IF MOUSE IS HOVERING OVER THUMBNAIL
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        bool hoverX = mousePos.x >= cursorPos.x && mousePos.x <= cursorPos.x + MATERIAL_TEXTURE_SLOT_SIZE;
        bool hoverY = mousePos.y >= cursorPos.y && mousePos.y <= cursorPos.y + MATERIAL_TEXTURE_SLOT_SIZE;
        bool mouseOverThumbnail = hoverX && hoverY;

        if (draggedTextureIndex != -1 && mouseOverThumbnail)
        {
            borderColour = HexToRGBA(SELECTED);
        }

        // IF DROP DRAGGED TEXTURE INTO SLOT
        if (mouseOverThumbnail && releasingDraggedTexture)
        {
            Texture& texture = materialManager.textures[draggedTextureIndex];
            if (textureType == 0) {
                material.UpdateAlbedo(texture.textureID, texture.textureHandle);
            }
            if (textureType == 1) {
                material.UpdateNormal(texture.textureID, texture.textureHandle);
            }
            if (textureType == 2) {
                material.UpdateRoughness(texture.textureID, texture.textureHandle);
            }
            draggedTextureIndex = -1;
            releasingDraggedTexture = false;
            droppedTextureIntoSlot = true;
            textureID = texture.textureID;
            updatedMaterial = true;
        }

        // TEXTURE SLOT 
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, borderColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverColour);
        if (ImGui::ImageButton(
            buttonID.c_str(), 
            textureID, 
            ImVec2(MATERIAL_TEXTURE_SLOT_SIZE-4, MATERIAL_TEXTURE_SLOT_SIZE-2)))
        {
        
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::EndChild();  
        ImGui::PopStyleColor();
    }

    // }----------{ TEXTURE PANEL }----------{
    void TexturePanelHeader(MaterialManager &materialManager)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::PushStyleColor(ImGuiCol_Button, HexToRGBA(BUTTON));
        ImGui::BeginChild("Texture Panel", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushFont(fontHeader);
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::Indent(6);
        ImGui::Text("%s", "Texture Panel"); 
        ImGui::PopFont();  
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(SpaceX() - 120.0f - GAP, 0));
        ImGui::SameLine();
        if (ImGui::Button("Import Texture", ImVec2(120.0f, 0)))
        {   
            materialManager.ImportTexture();
        }
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(1, 3));
        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    void TextureComponent(Texture &texture, int i)
    {
        std::string uniqueID = "Texture Element" + std::to_string(texture.textureID);
        ImGui::PushStyleColor(ImGuiCol_Border, HexToRGBA(BORDER));
        ImGui::BeginChild(uniqueID.c_str(), ImVec2(TEXTURE_THUMBNAIL_SIZE, 0), ImGuiChildFlags_AutoResizeY);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        

        // TEXTURE THUMBNAIL
        ImVec4 borderColour = HexToRGBA(BUTTON);
        ImVec4 hoverColour = HexToRGBA(BUTTON_HOVER);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, borderColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColour);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverColour);
        std::string imageID = "Texture Image" + std::to_string(texture.textureID);
        ImGui::SameLine();
        if (ImGui::ImageButton(
            imageID.c_str(), 
            texture.textureID, 
            ImVec2(TEXTURE_THUMBNAIL_SIZE-4, TEXTURE_THUMBNAIL_SIZE-2)))
        {
            
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            draggedTextureIndex = i;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        
        // NAME INPUT
        std::string nameInputID = "Texture Element Name" + std::to_string(texture.textureID);
        ImGui::BeginChild(nameInputID.c_str(), ImVec2(TEXTURE_THUMBNAIL_SIZE, 0), ImGuiChildFlags_AutoResizeY);
        std::string textInputID = "Texture Element Name Input" + std::to_string(texture.textureID);

        ImGui::PushItemWidth(TEXTURE_THUMBNAIL_SIZE);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, HexToRGBA(BUTTON));
        ImGui::InputText(textInputID.c_str(), texture.tempName, 32);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            strcpy_s(texture.name, 32, texture.tempName);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();


        ImGui::SetCursorPosX(cursorPos.x + TEXTURE_PANEL_GAP);
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    // }----------{ TEXTURE PANEL }----------{
};







