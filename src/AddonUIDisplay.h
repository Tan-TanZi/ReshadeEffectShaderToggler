///////////////////////////////////////////////////////////////////////
//
// Part of ShaderToggler, a shader toggler add on for Reshade 5+ which allows you
// to define groups of shaders to toggle them on/off with one key press
//
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/ShaderToggler
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////

#pragma once
#include "AddonUIAbout.h"
#include "AddonUIConstants.h"
#include "ConstantManager.h"
#include "KeyData.h"
#include "ResourceManager.h"
#include <cwctype>
#include <format>
#include <imgui.h>
#include <ranges>
#include <reshade.hpp>

#define MAX_DESCRIPTOR_INDEX 10

// From Reshade, see https://github.com/crosire/reshade/blob/main/source/imgui_widgets.cpp
static bool key_input_box(const char* name, uint32_t* keys, const reshade::api::effect_runtime* runtime) {
    char buf[48];
    buf[0] = '\0';
    if (*keys)
        buf[ShaderToggler::reshade_key_name(*keys).copy(buf, sizeof(buf) - 1)] = '\0';

    ImGui::InputTextWithHint(name,
                             "Click to set keyboard shortcut",
                             buf,
                             sizeof(buf),
                             ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

    if (ImGui::IsItemActive()) {
        const uint32_t last_key_pressed = ShaderToggler::reshade_last_key_pressed(runtime);
        if (last_key_pressed != 0) {
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                *keys = 0;

            } else if (last_key_pressed < 0x10 || last_key_pressed > 0x12) // Exclude modifier keys
            {
                *keys = last_key_pressed;
                *keys |= static_cast<uint32_t>(runtime->is_key_down(0x11)) << 8;
                *keys |= static_cast<uint32_t>(runtime->is_key_down(0x10)) << 16;
                *keys |= static_cast<uint32_t>(runtime->is_key_down(0x12)) << 24;
            }

            return true;
        }
    } else if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
    }

    return false;
}

static constexpr const char* invocationDescription[] = { "BEFORE DRAW", "AFTER DRAW", "ON RENDER TARGET CHANGE" };

static void DisplayIsPartOfToggleGroup() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::SameLine();
    ImGui::Text(" Shader is part of this toggle group.");
    ImGui::PopStyleColor();
}

static void DisplayTechniqueSelection(reshade::api::effect_runtime* runtime,
                                      AddonImGui::AddonUIData& instance,
                                      ShaderToggler::ToggleGroup* group,
                                      float tblWidth = 0) {
    if (group == nullptr) {
        return;
    }

    RuntimeDataContainer& runtimeData = runtime->get_private_data<RuntimeDataContainer>();

    std::unordered_set<std::string> curTechniques = group->preferredTechniques();
    std::unordered_set<std::string> newTechniques;
    static char searchBuf[256] = "\0";

    bool allowAll = group->getAllowAllTechniques();
    bool exceptions = group->getHasTechniqueExceptions();

    if (ImGui::BeginTable("Technique selection##options", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("##columnsetup", ImGuiTableColumnFlags_WidthFixed, tblWidth);

        ImGui::TableNextColumn();
        ImGui::Text("应用所有启用的特效技术");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Catchalltechniques", &allowAll);

        ImGui::TableNextRow();

        if (allowAll) {
            ImGui::TableNextColumn();
            ImGui::Text("排除选中的特效技术");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Exceptfor", &exceptions);

            ImGui::TableNextRow();
        }

        ImGui::TableNextColumn();
        ImGui::Text("搜索");
        ImGui::TableNextColumn();
        ImGui::InputText("##techniqueSearch", searchBuf, 256, ImGuiInputTextFlags_None);

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        if (ImGui::Button("全部取消勾选")) {
            curTechniques.clear();
        }
        ImGui::TableNextColumn();

        ImGui::EndTable();
    }

    ImGui::Separator();

    if (allowAll && !exceptions) {
        ImGui::BeginDisabled();
    }
    if (ImGui::BeginTable("Technique selection##table", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("##columnsetupSelection", ImGuiTableColumnFlags_WidthFixed, tblWidth);

        std::string searchString(searchBuf);

        if (runtimeData.allTechniques.size() > 0) {
            for (const auto& [name, effData] : runtimeData.allTechniques) {
                bool enabled = curTechniques.contains(name);

                if (std::ranges::search(
                      name, searchString, [](const wchar_t lhs, const wchar_t rhs) { return lhs == rhs; }, std::towupper, std::towupper)
                      .begin() != name.end()) {
                    ImGui::TableNextColumn();
                    ImGui::Checkbox(name.c_str(), &enabled);
                }

                if (enabled) {
                    newTechniques.insert(name);
                }
            }
        }

        ImGui::EndTable();
    }

    if (allowAll && !exceptions) {
        ImGui::EndDisabled();
    }

    group->setHasTechniqueExceptions(exceptions);
    group->setAllowAllTechniques(allowAll);

    std::shared_lock<std::shared_mutex> techLock(runtimeData.technique_mutex);
    if (runtimeData.allTechniques.size() > 0) {
        group->setPreferredTechniques(newTechniques);
        instance.AssignPreferredGroupTechniques(runtimeData.allTechniques);
    }
}

static void DrawPreview(unsigned long long textureId, uint32_t srcWidth, uint32_t srcHeight) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    float height = ImGui::GetWindowHeight();
    float width = ImGui::GetWindowWidth();

    float new_width = static_cast<float>(srcWidth);
    float new_height = static_cast<float>(srcHeight);

    float ratio = std::min(width / new_width, height / new_height);
    new_width *= ratio;
    new_height *= ratio;

    auto initialCursorPos = ImGui::GetCursorPos();
    auto centralizedCursorpos = ImVec2((width - new_width) * 0.5f, (height - new_height) * 0.5f);
    ImGui::SetCursorPos(centralizedCursorpos);

    ImGui::Image(textureId, ImVec2(new_width, new_height));

    ImGui::PopStyleVar();
}

static void DisplayPreview(AddonImGui::AddonUIData& instance,
                           Rendering::ResourceManager& resManager,
                           reshade::api::effect_runtime* runtime,
                           ShaderToggler::ToggleGroup* group,
                           float width = 0) {
    if (ImGui::BeginChild("RTPreview##child", { width, 0 }, true, ImGuiWindowFlags_None)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();
        reshade::api::resource_view srv = reshade::api::resource_view{ 0 };
        resManager.SetPongPreviewHandles(runtime->get_device(), nullptr, nullptr, &srv);
        bool clearAlpha = group->getClearPreviewAlpha();

        ImGui::Text("清除 Alpha 通道");
        ImGui::SameLine();
        ImGui::Checkbox("##Clearalpha", &clearAlpha);

        if (srv != 0) {
            ImGui::SameLine();
            ImGui::Text(std::format(" Address: 0x{:x} ", deviceData.huntPreview.target.handle).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Format: {} ", static_cast<uint32_t>(deviceData.huntPreview.format)).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Width: {} ", deviceData.huntPreview.width).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Height: {} ", deviceData.huntPreview.height).c_str());
            ImGui::Separator();

            if (ImGui::BeginChild("RTPreview##preview", { 0, 0 }, false, ImGuiWindowFlags_None)) {
                DrawPreview(srv.handle, deviceData.huntPreview.width, deviceData.huntPreview.height);
            }
            ImGui::EndChild();
        }

        group->setClearPreviewAlpha(clearAlpha);

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

static void DisplayBindingPreview(AddonImGui::AddonUIData& instance,
                                  Rendering::ResourceManager& resManager,
                                  reshade::api::effect_runtime* runtime,
                                  ShaderToggler::ToggleGroup* group) {
    if (ImGui::BeginChild("BindingPreview##child", { 0, 0 }, true, ImGuiWindowFlags_None)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();
        ShaderToggler::GroupResource& groupResource = group->GetGroupResource(ShaderToggler::GroupResourceType::RESOURCE_BINDING);

        reshade::api::resource_view res_view = { 0 };
        if (groupResource.owning) {
            res_view = groupResource.srv;
        } else if (groupResource.g_res != nullptr) {
            res_view = groupResource.g_res->srv;
        }

        if (res_view != 0) {
            ImGui::Text(std::format("Format: {} ", static_cast<uint32_t>(groupResource.target_description.texture.format)).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Width: {} ", groupResource.target_description.texture.width).c_str());
            ImGui::SameLine();
            ImGui::Text(std::format("Height: {} ", groupResource.target_description.texture.height).c_str());
            ImGui::Separator();

            if (ImGui::BeginChild("BindingPreview##preview", { 0, 0 }, false, ImGuiWindowFlags_None)) {
                DrawPreview(res_view.handle, groupResource.target_description.texture.width, groupResource.target_description.texture.height);
            }
            ImGui::EndChild();
        }

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

static void DisplayRenderTargets(AddonImGui::AddonUIData& instance,
                                 Rendering::ResourceManager& resManager,
                                 reshade::api::effect_runtime* runtime,
                                 ShaderToggler::ToggleGroup* group) {
    static float height = ImGui::GetWindowHeight();
    static float width = ImGui::GetWindowWidth();

    const char* typeSelectedItem = invocationDescription[group->getInvocationLocation()];
    uint32_t selectedIndex = group->getInvocationLocation();

    const char* typeDestItems[] = { "Render target", "Shader Resource View" };
    uint32_t selectedDestIndex = group->getRenderToResourceViews() ? 1 : 0;
    const char* typeSelectedDestItem = typeDestItems[selectedDestIndex];

    static const char* stageItems[] = { "PIXEL", "VERTEX", "COMPUTE" };
    uint32_t selectedStageIndex = group->getRenderSRVShaderStage();
    const char* selectedStage = stageItems[selectedStageIndex];

    bool retry = group->getRequeueAfterRTMatchingFailure();
    bool tonemap = group->getToneMap();
    bool preserveAlpha = group->getPreserveAlpha();
    bool flipbuffer = group->getFlipBuffer();
    static const char* swapchainMatchOptions[] = { "RESOLUTION", "ASPECT RATIO", "EXTENDED ASPECT RATIO", "NONE" };
    uint32_t selectedSwapchainMatchMode = group->getMatchSwapchainResolution();
    const char* typesSelectedSwapchainMatchMode = swapchainMatchOptions[selectedSwapchainMatchMode];

    bool supportsSRVwrite = runtime->get_device()->get_api() < reshade::api::device_api::d3d12;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::BeginChild("RenderTargets", { 0, height / 1.5f }, true, ImGuiChildFlags_AlwaysAutoResize)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        if (ImGui::BeginTable("RenderTargetsSettings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("##RTcolumnsetup", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() / 3);

            if (supportsSRVwrite) {
                ImGui::TableNextColumn();
                ImGui::Text("Render destination");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##Renderdestination", typeSelectedDestItem, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(typeDestItems); n++) {
                        bool is_selected = (typeSelectedDestItem == typeDestItems[n]);
                        if (ImGui::Selectable(typeDestItems[n], is_selected)) {
                            typeSelectedDestItem = typeDestItems[n];
                            selectedDestIndex = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::Separator();
            }

            if (supportsSRVwrite && selectedDestIndex == 1) {
                if (!instance.GetTrackDescriptors()) {
                    ImGui::BeginDisabled();
                    group->setRenderToResourceViews(false);
                } else {
                    group->setRenderToResourceViews(true);
                }

                ImGui::TableNextColumn();
                ImGui::Text("Shader Stage");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##RenderShaderStage", selectedStage, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(stageItems); n++) {
                        bool is_selected = (selectedStage == stageItems[n]);
                        if (ImGui::Selectable(stageItems[n], is_selected)) {
                            selectedStageIndex = n;
                            selectedStage = stageItems[n];
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                group->setRenderSRVShaderStage(selectedStageIndex);

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("Slot");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getRenderSRVSlotIndex());
                ImGui::SameLine();
                ImGui::PushID(0);
                if (ImGui::SmallButton("+")) {
                    group->setRenderSRVSlotIndex(group->getRenderSRVSlotIndex() + 1);
                }
                ImGui::PopID();

                if (group->getRenderSRVSlotIndex() != 0) {
                    ImGui::SameLine();

                    if (ImGui::SmallButton("-")) {
                        group->setRenderSRVSlotIndex(group->getRenderSRVSlotIndex() - 1);
                    }
                }

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("绑定");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getRenderSRVDescriptorIndex());
                ImGui::SameLine();
                ImGui::PushID(2);
                if (ImGui::SmallButton("+")) {
                    group->setRenderSRVDescriptorIndex(group->getRenderSRVDescriptorIndex() + 1);
                }
                ImGui::PopID();

                if (group->getRenderSRVDescriptorIndex() != 0) {
                    ImGui::SameLine();

                    ImGui::PushID(1);
                    if (ImGui::SmallButton("-")) {
                        group->setRenderSRVDescriptorIndex(group->getRenderSRVDescriptorIndex() - 1);
                    }
                    ImGui::PopID();
                }

                if (!instance.GetTrackDescriptors()) {
                    ImGui::EndDisabled();
                }
            } else {
                group->setRenderToResourceViews(false);

                ImGui::TableNextColumn();
                ImGui::Text("渲染目标索引");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getRenderTargetIndex());
                ImGui::SameLine();

                if (ImGui::SmallButton("+")) {
                    group->setRenderTargetIndex(group->getRenderTargetIndex() + 1);
                }

                if (group->getRenderTargetIndex() != 0) {
                    ImGui::SameLine();

                    if (ImGui::SmallButton("-")) {
                        group->setRenderTargetIndex(group->getRenderTargetIndex() - 1);
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::Text("调用时机");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##Invocationlocation", typeSelectedItem, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(invocationDescription); n++) {
                        bool is_selected = (typeSelectedItem == invocationDescription[n]);
                        if (ImGui::Selectable(invocationDescription[n], is_selected)) {
                            typeSelectedItem = invocationDescription[n];
                            selectedIndex = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("重试渲染目标分配");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##RetryRTassignment", &retry);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("应用色调映射钳位");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##tonemap", &tonemap);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("翻转渲染目标");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##flipbuffer", &flipbuffer);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("保留目标 Alpha 通道");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##preserveAlpha", &preserveAlpha);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("匹配交换链");
            ImGui::TableNextColumn();
            if (ImGui::BeginCombo("##effSwapChainMatchMode", typesSelectedSwapchainMatchMode, ImGuiComboFlags_None)) {
                for (int n = 0; n < IM_ARRAYSIZE(swapchainMatchOptions); n++) {
                    bool is_selected = (typesSelectedSwapchainMatchMode == swapchainMatchOptions[n]);
                    if (ImGui::Selectable(swapchainMatchOptions[n], is_selected)) {
                        typesSelectedSwapchainMatchMode = swapchainMatchOptions[n];
                        selectedSwapchainMatchMode = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::EndTable();
        }

        group->setRequeueAfterRTMatchingFailure(retry);
        group->setMatchSwapchainResolution(selectedSwapchainMatchMode);
        group->setInvocationLocation(selectedIndex);
        group->setToneMap(tonemap);
        group->setPreserveAlpha(preserveAlpha);
        group->setFlipBuffer(flipbuffer);

        ImGui::Separator();

        DisplayTechniqueSelection(runtime, instance, group, ImGui::GetWindowWidth() / 3);

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::PushID(4);
    ImGui::Button("", ImVec2(-1, 8.0f));
    ImGui::PopID();
    if (ImGui::IsItemActive())
        height += ImGui::GetIO().MouseDelta.y;

    DisplayPreview(instance, resManager, runtime, group);

    ImGui::PopStyleVar();
}

static void DisplayGroupView(AddonImGui::AddonUIData& instance,
                             Rendering::ResourceManager& resManager,
                             reshade::api::effect_runtime* runtime,
                             ShaderToggler::ToggleGroup* group,
                             ShaderToggler::ShaderManager* shaderManager) {
    float height = ImGui::GetWindowHeight();
    float width = ImGui::GetWindowWidth();

    if (*instance.ActiveCollectorFrameCounter() > 0) {
        return;
    }

    const std::unordered_set<uint32_t>& hashes = shaderManager->getCollectedShaderHashes();
    static int32_t selected = -1;
    uint32_t index = 0;
    ImGuiStyle style = ImGui::GetStyle();

    if (ImGui::BeginTable("ShaderHashView",
                          1,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody |
                            ImGuiTableColumnFlags_NoHeaderLabel,
                          ImVec2(0, height - 47))) {
        for (auto h : hashes) {
            ImGui::TableNextColumn();

            bool marked = false;
            if (shaderManager->isHuntedShaderMarked(shaderManager->getCollectedShaderHash(index))) {
                marked = true;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }

            if (ImGui::Selectable(std::format("{:#08x}", h).c_str(), selected == index, ImGuiSelectableFlags_AllowDoubleClick) &&
                (ImGui::IsMouseDoubleClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
                shaderManager->toggleMarkOnHuntedShader();
            }

            if (marked) {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemFocused()) {
                shaderManager->setActivedHuntedShaderIndex(index);
                instance.UpdateToggleGroupsForShaderHashes();
                selected = index;
            };

            index++;
        }

        ImGui::EndTable();
    }
}

static void DisplayTextureBindings(AddonImGui::AddonUIData& instance,
                                   ShaderToggler::ToggleGroup* group,
                                   reshade::api::effect_runtime* runtime,
                                   Rendering::ResourceManager& resManager) {
    static float height = ImGui::GetWindowHeight();
    float width = ImGui::GetWindowWidth();

    const char* typeItems[] = { "渲染目标", "着色器资源视图" };
    uint32_t selectedIndex = group->getExtractResourceViews() ? 1 : 0;
    const char* typeSelectedItem = typeItems[selectedIndex];
    DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();

    static const char* swapchainMatchOptions[] = { "RESOLUTION", "ASPECT RATIO", "EXTENDED ASPECT RATIO", "NONE" };
    uint32_t selectedSwapchainMatchMode = group->getBindingMatchSwapchainResolution();
    const char* typesSelectedSwapchainMatchMode = swapchainMatchOptions[selectedSwapchainMatchMode];

    static const char* stageItems[] = { "PIXEL", "VERTEX", "COMPUTE" };
    uint32_t selectedStageIndex = group->getSRVShaderStage();
    const char* selectedStage = stageItems[selectedStageIndex];

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::BeginChild("Texture bindings viewer", { 0, height / 2.0f }, true, ImGuiChildFlags_AlwaysAutoResize)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

        // Name of group
        char tmpBuffer[150];

        // Name of Binding
        bool isBindingEnabled = group->isProvidingTextureBinding();

        const std::string& bindingName = group->getTextureBindingName();
        strncpy_s(tmpBuffer, 150, bindingName.c_str(), bindingName.size());

        bool copyBinding = group->getCopyTextureBinding();
        bool clearBinding = group->getClearBindings();
        bool flipBinding = group->getFlipBufferBinding();

        if (ImGui::BeginTable("Bindingsettings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("##BindingColumnSetup", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() / 3);

            ImGui::TableNextColumn();
            ImGui::Text("启用纹理绑定");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Texturebindingenabled", &isBindingEnabled);

            if (!isBindingEnabled) {
                ImGui::BeginDisabled();
                group->setProvidingTextureBinding(false);
            } else {
                group->setProvidingTextureBinding(true);
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("纹理语义");
            ImGui::TableNextColumn();
            ImGui::InputText("##BindingName", tmpBuffer, 149);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("纹理来源");
            ImGui::TableNextColumn();
            if (ImGui::BeginCombo("##Bindingsource", typeSelectedItem, ImGuiComboFlags_None)) {
                for (int n = 0; n < IM_ARRAYSIZE(typeItems); n++) {
                    bool is_selected = (typeSelectedItem == typeItems[n]);
                    if (ImGui::Selectable(typeItems[n], is_selected)) {
                        typeSelectedItem = typeItems[n];
                        selectedIndex = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("为绑定创建纹理副本");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Copybinding", &copyBinding);

            ImGui::TableNextRow();

            ImGui::BeginDisabled(!copyBinding);
            ImGui::TableNextColumn();
            ImGui::Text("翻转绑定纹理");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##flipbinding", &flipBinding);
            ImGui::EndDisabled();

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("哈希值不匹配时清除绑定");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##Clearbinding", &clearBinding);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();

            ImGui::EndTable();
        }

        group->setTextureBindingName(tmpBuffer);
        group->setCopyTextureBinding(copyBinding);
        group->setClearBindings(clearBinding);
        group->setFlipBufferBinding(flipBinding);

        ImGui::Separator();

        if (ImGui::BeginTable("BindingSourcesettings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("##BindingSourceColumnSetup", ImGuiTableColumnFlags_WidthFixed, ImGui::GetWindowWidth() / 3);

            if (selectedIndex == 1) {
                if (!instance.GetTrackDescriptors()) {
                    ImGui::BeginDisabled();
                    group->setExtractResourceViews(false);
                } else {
                    group->setExtractResourceViews(true);
                }

                ImGui::TableNextColumn();
                ImGui::Text("着色器阶段");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##ShaderStage", selectedStage, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(stageItems); n++) {
                        bool is_selected = (selectedStage == stageItems[n]);
                        if (ImGui::Selectable(stageItems[n], is_selected)) {
                            selectedStageIndex = n;
                            selectedStage = stageItems[n];
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                group->setSRVShaderStage(selectedStageIndex);

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("Slot");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getBindingSRVSlotIndex());
                ImGui::SameLine();
                ImGui::PushID(0);
                if (ImGui::SmallButton("+")) {
                    group->setBindingSRVSlotIndex(group->getBindingSRVSlotIndex() + 1);
                }
                ImGui::PopID();

                if (group->getBindingSRVSlotIndex() != 0) {
                    ImGui::SameLine();

                    if (ImGui::SmallButton("-")) {
                        group->setBindingSRVSlotIndex(group->getBindingSRVSlotIndex() - 1);
                    }
                }

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("绑定");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getBindingSRVDescriptorIndex());
                ImGui::SameLine();
                ImGui::PushID(2);
                if (ImGui::SmallButton("+")) {
                    group->dispatchSRVCycle(ShaderToggler::CYCLE_UP);
                }
                ImGui::PopID();

                if (group->getBindingSRVDescriptorIndex() != 0) {
                    ImGui::SameLine();

                    ImGui::PushID(1);
                    if (ImGui::SmallButton("-")) {
                        group->dispatchSRVCycle(ShaderToggler::CYCLE_DOWN);
                    }
                    ImGui::PopID();
                }

                if (!instance.GetTrackDescriptors()) {
                    ImGui::EndDisabled();
                }
            } else {
                group->setExtractResourceViews(false);

                const char* rtTypeSelectedItem = invocationDescription[group->getBindingInvocationLocation()];
                uint32_t rtSelectedIndex = group->getBindingInvocationLocation();

                ImGui::TableNextColumn();
                ImGui::Text("渲染目标索引");
                ImGui::TableNextColumn();
                ImGui::Text("%u", group->getBindingRenderTargetIndex());
                ImGui::SameLine();

                ImGui::PushID(0);
                if (ImGui::SmallButton("+")) {
                    group->setBindingRenderTargetIndex(group->getBindingRenderTargetIndex() + 1);
                }
                ImGui::PopID();

                if (group->getBindingRenderTargetIndex() != 0) {
                    ImGui::SameLine();

                    if (ImGui::SmallButton("-")) {
                        group->setBindingRenderTargetIndex(group->getBindingRenderTargetIndex() - 1);
                    }
                }

                ImGui::TableNextRow();

                if (!copyBinding) {
                    ImGui::BeginDisabled();
                }

                ImGui::TableNextColumn();
                ImGui::Text("调用位置");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##Invocationlocation", rtTypeSelectedItem, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(invocationDescription); n++) {
                        bool is_selected = (rtTypeSelectedItem == invocationDescription[n]);
                        if (ImGui::Selectable(invocationDescription[n], is_selected)) {
                            rtTypeSelectedItem = invocationDescription[n];
                            rtSelectedIndex = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (!copyBinding) {
                    ImGui::EndDisabled();
                }

                ImGui::TableNextColumn();
                ImGui::Text("匹配交换链");
                ImGui::TableNextColumn();
                if (ImGui::BeginCombo("##swapChainMatchMode", typesSelectedSwapchainMatchMode, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(swapchainMatchOptions); n++) {
                        bool is_selected = (typesSelectedSwapchainMatchMode == swapchainMatchOptions[n]);
                        if (ImGui::Selectable(swapchainMatchOptions[n], is_selected)) {
                            typesSelectedSwapchainMatchMode = swapchainMatchOptions[n];
                            selectedSwapchainMatchMode = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                group->setBindingInvocationLocation(rtSelectedIndex);
                group->setBindingMatchSwapchainResolution(selectedSwapchainMatchMode);
            }

            ImGui::EndTable();
        }

        if (!isBindingEnabled) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::PushID(3);
    ImGui::Button("", ImVec2(-1, 8.0f));
    ImGui::PopID();
    if (ImGui::IsItemActive())
        height += ImGui::GetIO().MouseDelta.y;

    DisplayBindingPreview(instance, resManager, runtime, group);

    ImGui::PopStyleVar();
}

static void DisplayOverlay(AddonImGui::AddonUIData& instance, Rendering::ResourceManager& resManager, reshade::api::effect_runtime* runtime) {
    if (instance.GetToggleGroupIdShaderEditing() >= 0) {
        std::string editingGroupName = "";
        const int idx = instance.GetToggleGroupIdShaderEditing();
        ShaderToggler::ToggleGroup* group = nullptr;
        if (instance.GetToggleGroups().find(idx) != instance.GetToggleGroups().end()) {
            editingGroupName = instance.GetToggleGroups()[idx].getName();
            group = &instance.GetToggleGroups()[idx];
        }

        if (group == nullptr)
            return;

        ImGui::SetNextWindowBgAlpha(1.0);
        ImGui::SetNextWindowSize({ 1024, 768 }, ImGuiCond_Once);
        bool wndOpen = true;

        static float height = ImGui::GetWindowHeight();
        static float width = ImGui::GetWindowWidth();

        const char* typeItems[] = { "Pixel shader", "Vertex shader", "Compute Shader" };
        static const char* typeSelectedItem = typeItems[0];
        static uint32_t selectedIndex = 0;

        ShaderToggler::ShaderManager* selectedShaderManager =
          selectedIndex == 0 ? instance.GetPixelShaderManager() : (selectedIndex == 1 ? instance.GetVertexShaderManager() : instance.GetComputeShaderManager());

        if (ImGui::Begin(std::format("Group settings ({})", editingGroupName).c_str(), &wndOpen)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            if (ImGui::BeginChild("GroupView", { width / 3.0f, 0 }, true, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

                DisplayGroupView(instance, resManager, runtime, group, selectedShaderManager);

                ImGui::PushItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x * 2);
                if (ImGui::BeginCombo("##shaderType", typeSelectedItem, ImGuiComboFlags_None)) {
                    for (int n = 0; n < IM_ARRAYSIZE(typeItems); n++) {
                        bool is_selected = (typeSelectedItem == typeItems[n]);
                        if (ImGui::Selectable(typeItems[n], is_selected)) {
                            if (n != selectedIndex) {
                                // Reset hunting selections in other managers on switch
                                switch (n) {
                                    case 0: {
                                        instance.GetVertexShaderManager()->resetActiveHuntedShader();
                                        instance.GetComputeShaderManager()->resetActiveHuntedShader();
                                    } break;
                                    case 1: {
                                        instance.GetPixelShaderManager()->resetActiveHuntedShader();
                                        instance.GetComputeShaderManager()->resetActiveHuntedShader();
                                    } break;
                                    case 2: {
                                        instance.GetPixelShaderManager()->resetActiveHuntedShader();
                                        instance.GetVertexShaderManager()->resetActiveHuntedShader();
                                    } break;
                                    default:
                                        break;
                                }

                                instance.UpdateToggleGroupsForShaderHashes();
                            }

                            typeSelectedItem = typeItems[n];
                            selectedIndex = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::PushID(0);
            ImGui::Button("", ImVec2(8.0f, -1));
            ImGui::PopID();
            if (ImGui::IsItemActive())
                width += ImGui::GetIO().MouseDelta.x;

            ImGui::SameLine();

            if (ImGui::BeginChild("GroupSettings", { 0, 0 }, true, ImGuiChildFlags_AlwaysAutoResize)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));

                ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
                if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
                    if (ImGui::BeginTabItem("Effects")) {
                        instance.SetCurrentTabType(AddonImGui::TAB_RENDER_TARGET);
                        DisplayRenderTargets(instance, resManager, runtime, group);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Constant bindings")) {
                        instance.SetCurrentTabType(AddonImGui::TAB_CONSTANT_BUFFER);
                        DisplayConstantTab(instance, group, runtime->get_device());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Texture bindings")) {
                        instance.SetCurrentTabType(AddonImGui::TAB_TEXTURE_BINDING);
                        DisplayTextureBindings(instance, group, runtime, resManager);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::PopStyleVar();
        }
        ImGui::End();

        if (!wndOpen) {
            instance.SetCurrentTabType(AddonImGui::TAB_NONE);
            instance.EndShaderEditing(true, *group);
        }
    } else {
        instance.SetCurrentTabType(AddonImGui::TAB_NONE);
    }
}

static void CheckHotkeys(AddonImGui::AddonUIData& instance, reshade::api::effect_runtime* runtime) {
    if (*instance.ActiveCollectorFrameCounter() > 0) {
        --(*instance.ActiveCollectorFrameCounter());
    }
}

static void ShowHelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void DisplaySettings(AddonImGui::AddonUIData& instance, reshade::api::effect_runtime* runtime) {
    DisplayAbout();
    
    if (ImGui::CollapsingHeader("General info and help")) {
        ImGui::PushTextWrapPos();
        ImGui::TextUnformatted(
          "着色器开关插件允许你创建一个或多个着色器分组，可对分组整体启用/禁用。你可为每个分组自定义键盘快捷键（支持 Shift、Alt、Ctrl 等组合键），并设置易识别的分组名称。每个分组可绑定一个或多个顶点着色器、像素着色器。按下对应快捷键后，所有使用该分组内着色器的绘制调用都会被禁用，实现隐藏 3D 场景中对应元素的效果。");
        ImGui::TextUnformatted("\n点击分组的「修改着色器」按钮后，将启用以下固定快捷键：");
        ImGui::TextUnformatted("* 小键盘1/2：上一个/下一个像素着色器");
        ImGui::TextUnformatted("* Ctrl+小键盘1/2：上一个/下一个分组内已标记的像素着色器");
        ImGui::TextUnformatted("* 小键盘3：标记/取消标记当前像素着色器分组成员");
        ImGui::TextUnformatted("$ 小键盘4/5：上一个/下一个顶点着色器");
        ImGui::TextUnformatted("$ Ctrl+小键盘4/5：上一个/下一个分组内已标记的顶点着色器");
        ImGui::TextUnformatted("$ 小键盘6：标记/取消标记当前顶点着色器分组成员");
        ImGui::TextUnformatted(
          "\n当你按下快捷键遍历着色器时，当前选中的着色器会在 3D 场景中临时隐藏，方便你确认目标");
        ImGui::TextUnformatted("配置完成后，请点击“保存所有切换分组”按钮！！！下次启动游戏时分组会自动加载，可直接使用，或者分享他人");
        ImGui::PopTextWrapPos();
}

    ImGui::AlignTextToFramePadding();
    if (ImGui::CollapsingHeader("覆盖层不透明度", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::AlignTextToFramePadding();
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);
        ImGui::SliderFloat("Overlay opacity", instance.OverlayOpacity(), 0.0f, 1.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::SliderInt("# 采样帧数", instance.StartValueFramecountCollectionPhase(), 10, 1000);
        ImGui::SameLine();
        ShowHelpMarker("这是插件收集活动着色器的帧数。如果您想要标记的着色器仅在少数帧中出现，请将此值设置为一个较大的数字 "
                       "偶尔使用。仅能标记在收集的帧中使用的着色器。");
        ImGui::PopItemWidth();
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_None)) {
        ImGui::AlignTextToFramePadding();
        std::string varSelectedItem = instance.GetResourceShim();
        if (ImGui::BeginCombo("资源适配层", varSelectedItem.c_str(), ImGuiComboFlags_None)) {
            for (auto& v : Rendering::ResourceShimNames) {
                bool is_selected = (varSelectedItem == v);
                if (ImGui::Selectable(v.c_str(), is_selected)) {
                    varSelectedItem = v;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        instance.SetResourceShim(varSelectedItem);

        ImGui::AlignTextToFramePadding();
        std::string varSelectedCopyMethod = instance.GetConstHookCopyType();
        if (ImGui::BeginCombo("常量缓冲区复制方式", varSelectedCopyMethod.c_str(), ImGuiComboFlags_None)) {
            for (auto& v : Shim::Constants::ConstantCopyTypeNames) {
                bool is_selected = (varSelectedCopyMethod == v);
                if (ImGui::Selectable(v.c_str(), is_selected)) {
                    varSelectedCopyMethod = v;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        instance.SetConstHookCopyType(varSelectedCopyMethod);

        ImGui::AlignTextToFramePadding();
        bool trackDescriptors = instance.GetTrackDescriptors();
        ImGui::Checkbox("跟踪描述符", &trackDescriptors);
        instance.SetTrackDescriptors(trackDescriptors);

        bool runtimeReload = instance.GetPreventRuntimeReload();
        ImGui::Checkbox("禁止运行时重载", &runtimeReload);
        instance.SetPreventRuntimeReload(runtimeReload);
    }

    if (ImGui::CollapsingHeader("Keybindings", ImGuiTreeNodeFlags_None)) {
        for (uint32_t i = 0; i < IM_ARRAYSIZE(AddonImGui::KeybindNames); i++) {
            uint32_t keys = instance.GetKeybinding(static_cast<AddonImGui::Keybind>(i));
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.35f);
            if (key_input_box(AddonImGui::KeybindNames[i], &keys, runtime)) {
                instance.SetKeybinding(static_cast<AddonImGui::Keybind>(i), keys);
            }
            ImGui::PopItemWidth();
        }
    }

    if (ImGui::CollapsingHeader("List of Toggle Groups", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button(" 新建 ")) {
            instance.AddDefaultGroup();
        }
        ImGui::Separator();

        std::vector<ShaderToggler::ToggleGroup*> toRemove;
        for (auto& [_, group] : instance.GetToggleGroups()) {

            ImGui::PushID(group.getId());
            ImGui::AlignTextToFramePadding();
            if (ImGui::Button("X")) {
                toRemove.push_back(&group);
            }
            ImGui::SameLine();
            ImGui::Text(" %d ", group.getId());

            ImGui::SameLine();
            bool groupActive = group.isActive();
            ImGui::Checkbox("Active", &groupActive);
            if (groupActive != group.isActive()) {
                group.toggleActive();

                if (!groupActive && instance.GetConstantHandler() != nullptr) {
                    instance.GetConstantHandler()->RemoveGroup(&group, runtime->get_device());
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("编辑属性")) {
                group.setEditing(true);
            }

            ImGui::SameLine();
            if (instance.GetToggleGroupIdShaderEditing() >= 0) {
                if (instance.GetToggleGroupIdShaderEditing() == group.getId()) {
                    if (ImGui::Button(" 完成 ")) {
                        instance.EndShaderEditing(true, group);
                    }
                } else {
                    ImGui::BeginDisabled(true);
                    ImGui::Button("      ");
                    ImGui::EndDisabled();
                }
            } else {
                if (ImGui::Button("打开配置")) {
                    ImGui::SameLine();
                    instance.StartShaderEditing(group);
                }
            }

            ImGui::SameLine();
            if (group.getToggleKey() > 0) {
                ImGui::Text(" %s (%s)", group.getName().c_str(), ShaderToggler::reshade_key_name(group.getToggleKey()).c_str());
            } else {
                ImGui::Text(" %s", group.getName().c_str());
            }

            if (group.isEditing()) {
                ImGui::Separator();
                ImGui::Text("Edit group %d", group.getId());

                // Name of group
                char tmpBuffer[150];
                const std::string& name = group.getName();
                strncpy_s(tmpBuffer, 150, name.c_str(), name.size());
                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.7f);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("名称");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.2f);
                ImGui::InputText("##Name", tmpBuffer, 149);
                group.setName(tmpBuffer);
                ImGui::PopItemWidth();

                // Key binding of group
                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.7f);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("绑定快捷键");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.2f);

                uint32_t keys = group.getToggleKey();
                if (key_input_box(ShaderToggler::reshade_key_name(keys).c_str(), &keys, runtime)) {
                    group.setToggleKey(keys);
                }
                ImGui::PopItemWidth();

                if (ImGui::Button("OK")) {
                    group.setEditing(false);
                }
                ImGui::Separator();
            }

            ImGui::PopID();
        }
        if (toRemove.size() > 0) {
            // switch off keybinding editing or shader editing, if in progress
            instance.GetToggleGroupIdEffectEditing() = -1;
            instance.GetToggleGroupIdShaderEditing() = -1;
            instance.GetToggleGroupIdConstantEditing() = -1;
            instance.StopHuntingMode();
        }
        for (const auto& group : toRemove) {
            instance.SignalToggleGroupRemoved(runtime, group);

            std::erase_if(instance.GetToggleGroups(), [&group](const auto& item) { return item.first == group->getId(); });
        }

        if (toRemove.size() > 0) {
            instance.UpdateToggleGroupsForShaderHashes();
        }

        ImGui::Separator();
        if (instance.GetToggleGroups().size() > 0) {
            if (ImGui::Button("Save all Toggle Groups")) {
                instance.SaveShaderTogglerIniFile();
            }
        }
    }
}
