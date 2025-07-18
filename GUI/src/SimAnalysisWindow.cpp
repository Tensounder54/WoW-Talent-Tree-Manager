/*
    WoW Talent Tree Manager is an application for creating/editing/sharing talent trees and setups.
    Copyright(C) 2022 Tobias Mielich

    This program is free software : you can redistribute it and /or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see < https://www.gnu.org/licenses/>.

    Contact via https://github.com/TobiasM95/WoW-Talent-Tree-Manager/discussions or BuffMePls#2973 on Discord
*/

#include "SimAnalysisWindow.h"

#include <numeric>
#include <fstream>
#include <sstream>
#include <regex>

#include "curl.h"

#include "TTMGUIPresets.h"

namespace TTM {
    bool OptionSwitch(
        std::string leftText, 
        std::string rightText, 
        int* switchVariable, 
        float switchWidth, 
        bool centered, 
        std::string uniqueSliderId,
        bool showHelperTooltip,
        std::string helperTooltipHovered,
        std::string helperTooltipHelptext) {
        float rowWidth = ImGui::GetContentRegionAvail().x;
        float leftTextWidth = ImGui::CalcTextSize(leftText.c_str()).x;
        float padding = ImGui::GetStyle().ItemSpacing.x;
        ImVec2 start = ImGui::GetCursorPos();

        bool eventFired = false;

        if (centered) {
            ImGui::SetCursorPos(ImVec2(start.x + 0.5f * rowWidth - 0.5f * switchWidth - padding - leftTextWidth, start.y));
            ImGui::Text(leftText.c_str());

            ImGui::SetCursorPos(ImVec2(start.x + 0.5f * rowWidth - 0.5f * switchWidth, start.y));
            ImGui::SetNextItemWidth(switchWidth);
            if (ImGui::SliderInt(("##" + uniqueSliderId).c_str(), switchVariable, 0, 1, "", ImGuiSliderFlags_NoInput)) {
                eventFired = true;
            }

            ImGui::SetCursorPos(ImVec2(start.x + 0.5f * rowWidth + 0.5f * switchWidth + padding, start.y));
            ImGui::Text(rightText.c_str());
            if (showHelperTooltip) {
                ImGui::SameLine();
                HelperTooltip(helperTooltipHovered, helperTooltipHelptext);
            }
            ImGui::Spacing();
            ImGui::Spacing();
        }
        else {
            ImGui::Text(leftText.c_str());
            ImGui::SameLine();

            ImGui::SetNextItemWidth(switchWidth);
            if (ImGui::SliderInt(("##" + uniqueSliderId).c_str(), (int*)&switchVariable, 0, 1, "", ImGuiSliderFlags_NoInput)) {
                eventFired = true;
            }
            ImGui::SameLine();

            ImGui::Text(rightText.c_str());
            if (showHelperTooltip) {
                ImGui::SameLine();
                HelperTooltip(helperTooltipHovered, helperTooltipHelptext);
            }
        }

        return eventFired;
    }

    void PrintColoredRatio(float v1, float v2, float min, float max, int colorMode, std::string formatText) {
        if (v2 == 0.0f) {
            return;
        }
        float ratio = v1 / v2 - 1.0f;
        float interpolationValue;
        if (max == min) {
            interpolationValue = 1.0f;
        }
        else {
            interpolationValue = (ratio - min) / (max - min);
            interpolationValue = ratio  < min ? 0.0f : interpolationValue;
            interpolationValue = ratio > max ? 1.0f : interpolationValue;
        }

        ImVec4 interpolatedColor(0,0,0,0);
        switch (colorMode) {
        case 1: {//from green to red over yellow
            interpolatedColor.x = interpolationValue >= 0.5f ? 1.0f : 2.0f * interpolationValue; 
            interpolatedColor.y = interpolationValue <= 0.5f ? 1.0f : 2.0f - 2.0f * interpolationValue;
            interpolatedColor.z = 0.0f;
            interpolatedColor.w = 1.0f;
        }break;
        case 0: //from red to green over yellow, also default case
        default: {
            interpolatedColor.x = interpolationValue <= 0.5f ? 1.0f : 2.0f - 2.0f * interpolationValue;
            interpolatedColor.y = interpolationValue >= 0.5f ? 1.0f : 2.0f * interpolationValue;
            interpolatedColor.z = 0.0f;
            interpolatedColor.w = 1.0f;
        }break;
        }

        ImGui::TextColored(interpolatedColor, formatText.c_str(), ratio * 100.0f);
    }

    static void HelperTooltip(std::string hovered, std::string helptext) {
        ImGui::TextUnformattedColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), hovered.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15.0f);
            ImGui::TextUnformatted(helptext.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    static void AttachSimAnalysisTooltip(UIData& uiData, Engine::AnalysisResult& result, Engine::Talent_s talent)
    {
        if (ImGui::IsItemHovered())
        {
            if (result.skillsetCount == 0) {
                ImGui::BeginTooltip();
                ImGui::Text("Analyze sim results that include that talent to see details.");
                ImGui::EndTooltip();
                return;
            }
            if (talent->index != uiData.analysisTooltipLastTalentIndex) {
                uiData.analysisTooltipLastTalentIndex = talent->index;
                uiData.analysisTooltipTalentRank = 0;
            }
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    uiData.simAnalysisPage = SimAnalysisPage::Breakdown;
                    uiData.analysisBreakdownTalentIndex = talent->index;
                }
            }
            else {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (talent->type != Engine::TalentType::SWITCH) {
                        uiData.analysisTooltipTalentRank = uiData.analysisTooltipTalentRank + 1 == talent->maxPoints ? 0 : uiData.analysisTooltipTalentRank + 1;
                    }
                    else {
                        uiData.analysisTooltipTalentRank = uiData.analysisTooltipTalentRank == 1 ? 0 : 1;
                    }
                }
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    if (talent->type != Engine::TalentType::SWITCH) {
                        uiData.analysisTooltipTalentRank = uiData.analysisTooltipTalentRank == 0 ? talent->maxPoints - 1 : uiData.analysisTooltipTalentRank - 1;
                    }
                    else {
                        uiData.analysisTooltipTalentRank = uiData.analysisTooltipTalentRank == 1 ? 0 : 1;
                    }
                }
            }
            std::string idLabel = "Id: " + std::to_string(talent->index) + ", Pos: (" + std::to_string(talent->row) + ", " + std::to_string(talent->column) + ")";
            if (talent->type != Engine::TalentType::SWITCH) {
                ImGui::BeginTooltip();
                Presets::PUSH_FONT(uiData.fontsize, 1);
                ImGui::Text(talent->getName().c_str());
                Presets::POP_FONT();
                ImGui::Text(idLabel.c_str());
                ImGui::TextColored(Presets::GET_TOOLTIP_TALENT_TYPE_COLOR(uiData.style), "(click: switch rank)");
                ImGui::TextColored(Presets::GET_TOOLTIP_TALENT_TYPE_COLOR(uiData.style), "(ctrl+click: select talent)");
                ImGui::Separator();

                ImGui::Text("Rank %d:", uiData.analysisTooltipTalentRank + 1);
                ImGui::Spacing();
                int colIndex = result.indexToArrayColMap.at(talent->index) + uiData.analysisTooltipTalentRank;
                Engine::TalentPerformanceInfo& talentPerf = result.talentPerformances[colIndex];
                size_t numSkillsets = talentPerf.skillsetDPS.size();
                ImGui::Text("Number of skillsets: %d", numSkillsets);
                if (numSkillsets > 0) {
                    ImGui::Separator();
                    ImGui::Text("Lowest skillset \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.lowestDPSSkillset.first.second.c_str(),
                        talentPerf.lowestDPSSkillset.first.first.c_str(),
                        talentPerf.lowestDPSSkillset.second);
                    ImGui::Text("Median skillset \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.medianDPSSkillset.first.second.c_str(),
                        talentPerf.medianDPSSkillset.first.first.c_str(),
                        talentPerf.medianDPSSkillset.second);
                    ImGui::Text("Highest skillset \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.highestDPSSkillset.first.second.c_str(),
                        talentPerf.highestDPSSkillset.first.first.c_str(),
                        talentPerf.highestDPSSkillset.second);
                    ImGui::Text("Average skillset dps: %.1f",
                        talentPerf.averageDPSSkillset.second);
                    ImGui::Separator();
                    ImGui::Text("Lowest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.lowestDPSSkillsetWithoutTalent.first.second.c_str(),
                        talentPerf.lowestDPSSkillsetWithoutTalent.first.first.c_str(),
                        talentPerf.lowestDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.lowestDPSSkillsetWithoutTalent.second, talentPerf.lowestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Text("Median skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.medianDPSSkillsetWithoutTalent.first.second.c_str(),
                        talentPerf.medianDPSSkillsetWithoutTalent.first.first.c_str(),
                        talentPerf.medianDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.medianDPSSkillsetWithoutTalent.second, talentPerf.medianDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Text("Highest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.highestDPSSkillsetWithoutTalent.first.second.c_str(),
                        talentPerf.highestDPSSkillsetWithoutTalent.first.first.c_str(),
                        talentPerf.highestDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.highestDPSSkillsetWithoutTalent.second, talentPerf.highestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Text("Average skillset dps without this talent: %.1f",
                        talentPerf.averageDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.averageDPSSkillsetWithoutTalent.second, talentPerf.averageDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Separator();
                    ImGui::Text("Absolute ranking: %.2f%%", result.talentAbsolutePositionRankings[colIndex] * 100.0f);
                    ImGui::Text("Relative performance: %.2f%%", result.talentRelativePerformanceRankings[colIndex] * 100.0f);
                    ImGui::Text("Skillset distribution:");
                    ImGui::PlotHistogramRedGreen("##tooltipSkillsetsDistributionHistogram",
                        &talentPerf.skillsetDPS[0],
                        static_cast<int>(numSkillsets),
                        talentPerf.skillsetDPSNames.data(),
                        0, NULL,
                        0.95f * result.lowestDPSSkillset.second,
                        1.05f * result.highestDPSSkillset.second,
                        ImVec2(0, 80.0f));
                }

                ImGui::EndTooltip();
            }
            else {
                ImGui::BeginTooltip();
                Presets::PUSH_FONT(uiData.fontsize, 1);
                if (uiData.analysisTooltipTalentRank == 0) {
                    ImGui::Text(talent->name.c_str());
                }
                else {
                    ImGui::Text(talent->nameSwitch.c_str());
                }
                Presets::POP_FONT();
                ImGui::Text(idLabel.c_str());
                ImGui::TextColored(Presets::GET_TOOLTIP_TALENT_TYPE_COLOR(uiData.style), "(click: switch talent choice)");
                ImGui::TextColored(Presets::GET_TOOLTIP_TALENT_TYPE_COLOR(uiData.style), "(ctrl+click: select talent)");
                ImGui::Separator();

                int colIndex = result.indexToArrayColMap.at(talent->index) + uiData.analysisTooltipTalentRank;
                Engine::TalentPerformanceInfo& talentPerf = result.talentPerformances[colIndex];
                size_t numSkillsets = talentPerf.skillsetDPS.size();
                ImGui::Text("Number of skillsets: %d", numSkillsets);
                if (numSkillsets > 0) {
                    ImGui::Separator();
                    ImGui::Text("Lowest skillset \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.lowestDPSSkillset.first.second.c_str(),
                        talentPerf.lowestDPSSkillset.first.first.c_str(),
                        talentPerf.lowestDPSSkillset.second);
                    ImGui::Text("Median skillset \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.medianDPSSkillset.first.second.c_str(),
                        talentPerf.medianDPSSkillset.first.first.c_str(),
                        talentPerf.medianDPSSkillset.second);
                    ImGui::Text("Highest skillset \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.highestDPSSkillset.first.second.c_str(),
                        talentPerf.highestDPSSkillset.first.first.c_str(),
                        talentPerf.highestDPSSkillset.second);
                    ImGui::Text("Average skillset dps: %.1f",
                        talentPerf.averageDPSSkillset.second);
                    ImGui::Separator();
                    ImGui::Text("Lowest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.lowestDPSSkillsetWithoutTalent.first.second.c_str(),
                        talentPerf.lowestDPSSkillsetWithoutTalent.first.first.c_str(),
                        talentPerf.lowestDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.lowestDPSSkillsetWithoutTalent.second, talentPerf.lowestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Text("Median skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.medianDPSSkillsetWithoutTalent.first.second.c_str(),
                        talentPerf.medianDPSSkillsetWithoutTalent.first.first.c_str(),
                        talentPerf.medianDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.medianDPSSkillsetWithoutTalent.second, talentPerf.medianDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Text("Highest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                        talentPerf.highestDPSSkillsetWithoutTalent.first.second.c_str(),
                        talentPerf.highestDPSSkillsetWithoutTalent.first.first.c_str(),
                        talentPerf.highestDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.highestDPSSkillsetWithoutTalent.second, talentPerf.highestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Text("Average skillset dps without this talent: %.1f",
                        talentPerf.averageDPSSkillsetWithoutTalent.second);
                    ImGui::SameLine();
                    PrintColoredRatio(talentPerf.averageDPSSkillsetWithoutTalent.second, talentPerf.averageDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                    ImGui::Separator();
                    ImGui::Text("Absolute ranking: %.2f%%", result.talentAbsolutePositionRankings[colIndex]*100.0f);
                    ImGui::Text("Relative performance: %.2f%%", result.talentRelativePerformanceRankings[colIndex]*100.0f);
                    ImGui::Text("Skillset distribution:");
                    ImGui::PlotHistogramRedGreen("##tooltipSkillsetsDistributionHistogram",
                        &talentPerf.skillsetDPS[0],
                        static_cast<int>(numSkillsets),
                        talentPerf.skillsetDPSNames.data(),
                        0, NULL,
                        0.95f * result.lowestDPSSkillset.second,
                        1.05f * result.highestDPSSkillset.second,
                        ImVec2(0, 80.0f));
                }
                ImGui::EndTooltip();
            }
        }
    }

    void RenderSimAnalysisWindow(UIData& uiData, TalentTreeCollection& talentTreeCollection) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
        if (ImGui::Begin("TreeWindow", nullptr, ImGuiWindowFlags_NoDecoration)) {
            ImGuiWindowFlags treeWindowChildFlags = ImGuiWindowFlags_NoScrollbar;
            bool ctrlPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
            if (ctrlPressed) {
                treeWindowChildFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            }
            ImGui::BeginChild("TreeWindowChild", ImVec2(0, 0), true, treeWindowChildFlags);
            if (ImGui::IsItemActive())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetIO().MouseDelta.x);
                    ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseDelta.y);
                }
            }
            if (ImGui::IsWindowHovered() && ctrlPressed) {
                float mouseWheel = ImGui::GetIO().MouseWheel;
                if (mouseWheel != 0) {
                    float oldZoomFactor = uiData.treeEditorZoomFactor;
                    uiData.treeEditorZoomFactor += 0.2f * mouseWheel;
                    uiData.treeEditorZoomFactor = std::clamp(uiData.treeEditorZoomFactor, 1.0f, 3.0f);
                    if (oldZoomFactor != uiData.treeEditorZoomFactor) {
                        uiData.treeEditorWindowPos = ImGui::GetWindowPos();
                        uiData.treeEditorWindowSize = ImGui::GetWindowSize();
                        uiData.treeEditorMousePos = ImGui::GetMousePos();
                    }
                }
            }

            if (!uiData.hoveredAnalysisSkillset) {
                placeSimAnalysisTreeElements(uiData, talentTreeCollection);
            }
            else {
                drawSkillsetPreview(uiData, talentTreeCollection, uiData.hoveredAnalysisSkillset);
            }

            ImGui::EndChild();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        if (ImGui::Begin("SettingsWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::Button("Sim Analysis Settings", ImVec2(ImGui::GetContentRegionAvail().x / 3.0f, 25))) {
                uiData.simAnalysisPage = SimAnalysisPage::Settings;
            }
            ImGui::SameLine();
            if (ImGui::Button("Skillset Rankings", ImVec2(ImGui::GetContentRegionAvail().x / 2.0f, 25))) {
                uiData.simAnalysisPage = SimAnalysisPage::Ranking;
            }
            ImGui::SameLine();
            if (ImGui::Button("Talent Breakdown", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
                uiData.simAnalysisPage = SimAnalysisPage::Breakdown;
            }
            ImGui::Spacing();

            switch (uiData.simAnalysisPage) {
            case SimAnalysisPage::Settings: {
                ImGui::Text("Import simulation results:");
                ImGui::SameLine();
                HelperTooltip("(?)", "In order to use the sim analysis tool, create a Raidbots/SimC sim with profilesets (not copies!, if you use SimC locally, create an \"output=\" file) that are named after your loadout skillsets.\n\nIf you have multiple skillsets with the same name, the first one is used! If no skillset matches the sim import then that dps value is discarded.\n\nIf your sim is done, take the result URL (\"https://www.raidbots.com/simbot/report/rWRjiotwe8wa2389RA\" or the absolute path (\"C:Users\\...\") and paste it here. The absolute path can point to a directory, to read all result files in that directory, or a single result file.");
                ImGui::InputText("##simAnalysisRaidbotsInputText", &uiData.raidbotsInputURL, ImGuiInputTextFlags_AutoSelectAll);
                ImGui::SameLine();
                if (ImGui::Button("Add result##simAnalysisRaidbotsAddResultButton")) {
                    SimData simData = FetchSimData(uiData.raidbotsInputURL);
                    Engine::ImportSimData(simData.simOutputNames, simData.simOutputs, talentTreeCollection.activeTree());
                    AnalyzeRawResults(talentTreeCollection.activeTree());
                    CalculateAnalysisRankings(uiData, talentTreeCollection.activeTree().analysisResult);
                    UpdateColorGlowTextures(uiData, talentTreeCollection, talentTreeCollection.activeTree().analysisResult);
                }

                if (ImGui::BeginListBox("##simAnalysisResultsListbox", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                {
                    for (int n = 0; n < talentTreeCollection.activeTree().simAnalysisRawResults.size(); n++)
                    {
                        const bool is_selected = (talentTreeCollection.activeTree().selectedSimAnalysisRawResult == n);
                        if (ImGui::Selectable((std::to_string(talentTreeCollection.activeTree().simAnalysisRawResults[n].skillsets.size()) + ": " + talentTreeCollection.activeTree().simAnalysisRawResults[n].name + "##" + std::to_string(n)).c_str(), is_selected)) {
                            talentTreeCollection.activeTree().selectedSimAnalysisRawResult = n;
                        }

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndListBox();
                }
                if (ImGui::Button("Remove result##simAnalysisRaidbotsRemoveResultButton")) {
                    if (talentTreeCollection.activeTree().selectedSimAnalysisRawResult >= 0
                        && talentTreeCollection.activeTree().selectedSimAnalysisRawResult < talentTreeCollection.activeTree().simAnalysisRawResults.size()) {
                        talentTreeCollection.activeTree().simAnalysisRawResults.erase(
                            talentTreeCollection.activeTree().simAnalysisRawResults.begin() +
                            talentTreeCollection.activeTree().selectedSimAnalysisRawResult);
                        AnalyzeRawResults(talentTreeCollection.activeTree());
                        CalculateAnalysisRankings(uiData, talentTreeCollection.activeTree().analysisResult);
                        UpdateColorGlowTextures(uiData, talentTreeCollection, talentTreeCollection.activeTree().analysisResult);
                    }
                }
                ImGui::Separator();

                Engine::AnalysisResult& res = talentTreeCollection.activeTree().analysisResult;
                ImGui::Text("Currently analyzed skillsets: %d", res.skillsetCount);
                if (res.skillsetDPS.size() > 0) {
                    ImGui::Text("Lowest skillset \"%s\" in import \"%s\" dps: %.1f",
                        res.lowestDPSSkillset.first.second.c_str(),
                        res.lowestDPSSkillset.first.first.c_str(),
                        res.lowestDPSSkillset.second);
                    ImGui::Text("Median skillset \"%s\" in import \"%s\" dps: %.1f",
                        res.medianDPSSkillset.first.second.c_str(),
                        res.medianDPSSkillset.first.first.c_str(),
                        res.medianDPSSkillset.second);
                    ImGui::Text("Highest skillset \"%s\" in import \"%s\" dps: %.1f",
                        res.highestDPSSkillset.first.second.c_str(),
                        res.highestDPSSkillset.first.first.c_str(),
                        res.highestDPSSkillset.second);
                    ImGui::Text("Average skillset dps: %.1f",
                        res.averageDPSSkillset.second);
                    ImGui::Text("Skillset distribution:");
                    ImGui::PlotHistogramRedGreen("##allSkillsetsDistributionHistogram",
                        &res.skillsetDPS[0],
                        static_cast<int>(res.skillsetDPS.size()),
                        res.skillsetDPSNames.data(),
                        0, NULL,
                        0.95f * res.lowestDPSSkillset.second,
                        1.05f * res.highestDPSSkillset.second,
                        ImVec2(ImGui::GetContentRegionAvail().x, 80.0f));
                }
                ImGui::Separator();

                ImGui::Text("Visualization settings:");
                ImGui::Spacing();
                ImGui::Spacing();
                if (OptionSwitch("Talent Icon", "Rating", &uiData.simAnalysisIconRatingSwitch, 80.0f, true, "simAnalysisRatingSwitch", true, "(?)", "Show the talent icon or the rating of the talent (either absolute position in the ranking or the relative performance compared to the top skillset).")) {
                    //probably needs nothing here
                }
                //ImGui::SameLine();
                //HelperTooltip("(?)", "Show the talent icon or the rating of the talent (either absolute position in the ranking or the relative performance compared to the top skillset).");
                if (OptionSwitch("Relative dps", "Ranking", &uiData.relativeDpsRankingSwitch, 80.0f, true, "simAnalysisRelativeDpsRankingSwitch", true, "(?)", "Show the rating and colors based on absolute position in the ranking or the relative performance compared to the top skillset.")) {
                    CalculateAnalysisRankings(uiData, talentTreeCollection.activeTree().analysisResult);
                    UpdateColorGlowTextures(uiData, talentTreeCollection, talentTreeCollection.activeTree().analysisResult);
                }
                //ImGui::SameLine();
                //HelperTooltip("(?)", "Show the rating and colors based on absolute position in the ranking or the relative performance compared to the top skillset.");
                if (OptionSwitch("Show lowest", "Show highest", &uiData.showLowestHighestSwitch, 80.0f, true, "simAnalysisLowestHighestSwitch", true, "(?)", "For multi-point talents or switch talents choose if the shown rating/color is based on the lowest or highest rating. You can still see all the individual values when hovering over the talent.")) {
                    CalculateAnalysisRankings(uiData, talentTreeCollection.activeTree().analysisResult);
                    UpdateColorGlowTextures(uiData, talentTreeCollection, talentTreeCollection.activeTree().analysisResult);
                }
                //ImGui::SameLine();
                //HelperTooltip("(?)", "For multi-point talents or switch talents choose if the shown rating/color is based on the lowest or highest rating. You can still see all the individual values when hovering over the talent.");
                ImGui::Text("Ranking metric:");
                ImGui::SameLine();
                HelperTooltip("(?)", "Choose the metric that's used to calculate talent performances:\n\nTop X: Take the average of the top X skillsets where this talent is used.\n\nMedian: Take the median of all skillsets where this talent is used.\n\nAverage: Take the average of all skillsets where this talent is used.\n\nTop 1 + Median: Take the top skillset but sort further using the median of all skillsets where this talent is used. The shown relative performance is still only top 1 skillset.");
                int oldSetting = uiData.topMedianPerformanceSwitch;
                ImGui::RadioButton("Top 1", &uiData.topMedianPerformanceSwitch, static_cast<int>(Engine::PerformanceMetric::TOP1));
                ImGui::SameLine();
                ImGui::RadioButton("Top 3", &uiData.topMedianPerformanceSwitch, static_cast<int>(Engine::PerformanceMetric::TOP3));
                ImGui::SameLine();
                ImGui::RadioButton("Top 5", &uiData.topMedianPerformanceSwitch, static_cast<int>(Engine::PerformanceMetric::TOP5));
                ImGui::SameLine();
                ImGui::RadioButton("Median", &uiData.topMedianPerformanceSwitch, static_cast<int>(Engine::PerformanceMetric::MEDIAN));
                ImGui::SameLine();
                ImGui::RadioButton("Average", &uiData.topMedianPerformanceSwitch, static_cast<int>(Engine::PerformanceMetric::AVERAGE));
                ImGui::SameLine();
                ImGui::RadioButton("Top 1 + Median", &uiData.topMedianPerformanceSwitch, static_cast<int>(Engine::PerformanceMetric::TOPMEDIAN));
                if (oldSetting != uiData.topMedianPerformanceSwitch) {
                    CalculateAnalysisRankings(uiData, talentTreeCollection.activeTree().analysisResult);
                    UpdateColorGlowTextures(uiData, talentTreeCollection, talentTreeCollection.activeTree().analysisResult);
                }

                ImGui::Separator();
                ImGui::Text("Misc:");
                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text("Create top %d skillsets simc profilesets", uiData.simAnalysisAddTopSkillsetCount);
                ImGui::SliderInt(
                    "##loadoutSolverAddRandomToLoadoutSlider", 
                    &uiData.simAnalysisAddTopSkillsetCount, 
                    talentTreeCollection.activeTree().analysisResult.skillsetCount > 0 ? 1 : 0,
                    uiData.loadoutSolverAddAllLimit > talentTreeCollection.activeTree().analysisResult.skillsetCount 
                    ? talentTreeCollection.activeTree().analysisResult.skillsetCount
                    : uiData.loadoutSolverAddAllLimit, 
                    "%d", 
                    ImGuiSliderFlags_AlwaysClamp
                );
                ImGui::InputText("##simAnalsyisTopSkillsetSimcInputText", &uiData.simAnalysisExportTopSkillsetsSimcString, ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);
                ImGui::SameLine();
                if (ImGui::Button("Generate")) {
                    int& skillsetCount = talentTreeCollection.activeTree().analysisResult.skillsetCount;
                    if (skillsetCount > 0) {
                        std::vector<std::shared_ptr<Engine::TalentSkillset>> topSkillsets;
                        topSkillsets.reserve(skillsetCount);
                        std::unordered_map<std::string, std::shared_ptr<Engine::TalentSkillset>> allSkillsetMap;
                        for (const auto& skillset : talentTreeCollection.activeTree().loadout) {
                            allSkillsetMap[skillset->name.c_str()] = skillset;
                        }
                        for (int i = 0; i < uiData.simAnalysisAddTopSkillsetCount; i++) {
                            const auto& skillsetName = talentTreeCollection.activeTree().analysisResult.skillsetDPSNames[skillsetCount - i - 1];
                            if (allSkillsetMap.count(skillsetName)) {
                                topSkillsets.push_back(allSkillsetMap.at(skillsetName));
                            }
                        }
                        uiData.simAnalysisExportTopSkillsetsSimcString = Engine::createAllSkillsetsSimcStringRepresentation(
                            talentTreeCollection.activeTree(),
                            topSkillsets
                        );
                    }
                }

                ImGui::Text("Generate single talent SimC export:");
                ImGui::InputText("##simAnalysisSingleTalentSimcExportInputText", &uiData.simAnalysisSingleTalentExportString, ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);
                ImGui::SameLine();
                if (ImGui::Button("Generate##treeEditorImportTalentTreeButton")) {
                    uiData.simAnalysisSingleTalentExportString = Engine::createSingleTalentsSimcString(talentTreeCollection.activeTree());
                }
            }break;
            case SimAnalysisPage::Ranking: {
                if (talentTreeCollection.activeTree().analysisResult.skillsetCount <= 0) {
                    ImGui::Text("Analyze sim results to display ranking");
                    break;
                }
                constexpr ImGuiTableFlags flags = 
                    ImGuiTableFlags_SizingStretchSame 
                    | ImGuiTableFlags_Resizable 
                    | ImGuiTableFlags_BordersOuter 
                    | ImGuiTableFlags_BordersV 
                    | ImGuiTableFlags_BordersInnerH
                    | ImGuiTableFlags_ScrollY;

                auto& result = talentTreeCollection.activeTree().analysisResult;
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
                if (ImGui::BeginTable("Skillset ranking", 4, flags, ImGui::GetContentRegionAvail()))
                {
                    float pageWidth = ImGui::GetContentRegionAvail().x;
                    float rankWidth = ImGui::CalcTextSize("_Rank_").x / pageWidth;
                    float nameWidth = 0.44f * (1.0f - rankWidth);
                    float perfWidth = 0.28f * (1.0f - rankWidth);
                    float butWidth = 0.28f * (1.0f - rankWidth);
                    float textHeight = ImGui::CalcTextSize("@").y;

                    ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthStretch * 0, rankWidth, 0);
                    ImGui::TableSetupColumn("Skillset name", ImGuiTableColumnFlags_WidthStretch * 0, nameWidth, 0);
                    ImGui::TableSetupColumn("Performance", ImGuiTableColumnFlags_WidthStretch * 0, perfWidth, 0);
                    ImGui::TableSetupColumn("Press to view", ImGuiTableColumnFlags_WidthStretch * 0, butWidth, 0);
                    ImGui::TableSetupScrollFreeze(4, 1);
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Rank");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("Skillset name");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("Performance");
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("Press to view");
                    float maxRatio = result.highestDPSSkillset.second / result.referenceDPS;
                    for (int row = result.skillsetCount - 1; row >= 0; row--)
                    {
                        bool blueText = false;
                        if (row == uiData.hoveredAnalysisSkillsetIndex) {
                            blueText = true;
                            ImGui::PushStyleColor(ImGuiCol_Text, Presets::GET_TOOLTIP_TALENT_DESC_COLOR(uiData.style));
                        }
                        ImGui::TableNextRow();
                        for (int column = 0; column < 4; column++)
                        {
                            ImGui::TableSetColumnIndex(column);
                            float ratio = result.skillsetDPS[row] / result.referenceDPS;
                            switch (column) {
                            case 0: {
                                ImGui::Button(std::to_string(result.skillsetCount - row).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, textHeight * 1.375f));
                            }break;
                            case 1: {
                                ImGui::Button(result.skillsetDPSNames[row], ImVec2(ImGui::GetContentRegionAvail().x, textHeight*1.375f));
                            }break;
                            case 2: {
                                if (!blueText) {
                                    if (ratio < 1.0f) {
                                        ImGui::PushStyleColor(ImGuiCol_Text, Presets::GET_SIM_ANALYSIS_YELLOW_COLOR(uiData.style));
                                    }
                                    else {
                                        ImGui::PushStyleColor(ImGuiCol_Text, Presets::GET_SIM_ANALYSIS_GREEN_COLOR(uiData.style));
                                    }
                                }
                                char buff[100];
                                std::snprintf(buff, sizeof(buff), "%.2f (%.1f%%)", result.skillsetDPS[row], ratio * 100.0f);
                                ImGui::Button(buff, ImVec2(ImGui::GetContentRegionAvail().x, textHeight * 1.375f));
                                if (!blueText) {
                                    ImGui::PopStyleColor();
                                }
                            }break;
                            case 3: {
                                float windowWidth = ImGui::GetContentRegionAvail().x;
                                float normRatio = (result.skillsetDPS[row] - result.lowestDPSSkillset.second)
                                    / (result.highestDPSSkillset.second - result.lowestDPSSkillset.second);
                                ImVec4 rankingColor = ImVec4(1.0f - normRatio, 1.0f, 0.0f, 1.0f);
                                if (row == uiData.hoveredAnalysisSkillsetIndex) {
                                    ImGui::PushStyleColor(ImGuiCol_Button, Presets::GET_TOOLTIP_TALENT_DESC_COLOR(uiData.style));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Presets::GET_TOOLTIP_TALENT_DESC_COLOR(uiData.style));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Presets::GET_TOOLTIP_TALENT_DESC_COLOR(uiData.style));
                                }
                                else {
                                    ImGui::PushStyleColor(ImGuiCol_Button, rankingColor);
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rankingColor);
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rankingColor);
                                }
                                ImVec2 cursorPos = ImGui::GetCursorPos();
                                ImGui::Button("###row", ImVec2(windowWidth * ratio / maxRatio, 20.0f));
                                ImGui::SetItemAllowOverlap();
                                ImGui::SetCursorPos(cursorPos);
                                if(ImGui::InvisibleButton(("simAnalysisSelectSkillsetInvisButton" + std::to_string(row)).c_str(), ImVec2(windowWidth, 20.0f))) {
                                    if (uiData.hoveredAnalysisSkillsetIndex >= 0
                                        && uiData.hoveredAnalysisSkillsetIndex < static_cast<int>(result.skillsetDPSNames.size()))
                                    {
                                        std::string skillsetName{ result.skillsetDPSNames[uiData.hoveredAnalysisSkillsetIndex] };
                                        for (size_t i = 0; i < talentTreeCollection.activeTree().loadout.size(); i++) {
                                            auto& skillset_s = talentTreeCollection.activeTree().loadout[i];
                                            if (skillset_s->name == skillsetName) {
                                                talentTreeCollection.activeTree().activeSkillsetIndex = static_cast<int>(i);
                                                Engine::activateSkillset(talentTreeCollection.activeTree(), static_cast<int>(i));
                                                uiData.editorViewTarget = EditorView::LoadoutEdit;
                                                uiData.isLoadoutInitValidated = false;
                                                break;
                                            }
                                        }
                                    }
                                }
                                ImGui::PopStyleColor(3);
                            }break;
                            }
                            if (ImGui::IsItemHovered()) {
                                if (uiData.hoveredAnalysisSkillsetIndex != row) {
                                    uiData.hoveredAnalysisSkillsetIndex = row;
                                    std::string skillsetName{ result.skillsetDPSNames[row] };
                                    for (auto& skillset_s : talentTreeCollection.activeTree().loadout) {
                                        if (skillset_s->name == skillsetName) {
                                            uiData.hoveredAnalysisSkillset = std::make_shared<Engine::TalentSkillset>(*skillset_s);
                                        }
                                    }
                                }
                            }
                        }
                        if (blueText) {
                            blueText = false;
                            ImGui::PopStyleColor();
                        }
                    }
                    ImGui::EndTable();
                    ImGui::PopStyleColor(5);
                    ImGui::PopStyleVar();
                    ImVec2 topLeft = ImGui::GetItemRectMin();
                    ImVec2 bottomRight = ImGui::GetItemRectMax();
                    ImVec2 mousePos = ImGui::GetMousePos();
                    if(mousePos.x < topLeft.x || mousePos.x > bottomRight.x || mousePos.y < topLeft.y || mousePos.y > bottomRight.y) {
                        uiData.hoveredAnalysisSkillset = nullptr;
                        uiData.hoveredAnalysisSkillsetIndex = -1;
                    }
                    else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) 
                        && uiData.hoveredAnalysisSkillsetIndex >= 0
                        && uiData.hoveredAnalysisSkillsetIndex < static_cast<int>(result.skillsetDPSNames.size())) {
                        result.referenceDPS = result.skillsetDPS[uiData.hoveredAnalysisSkillsetIndex];
                    }
                }
            }break;
            case SimAnalysisPage::Breakdown: {
                if (!talentTreeCollection.activeTree().analysisResult.indexToArrayColMap.count(uiData.analysisBreakdownTalentIndex)) {
                    uiData.analysisBreakdownTalentIndex = -1;
                }
                if (uiData.analysisBreakdownTalentIndex == -1) {
                    ImGui::Text("Select talent to display breakdown. (ctrl+click)");
                }
                else {
                    Engine::Talent_s talent = talentTreeCollection.activeTree().orderedTalents[uiData.analysisBreakdownTalentIndex];
                    Engine::AnalysisResult& result = talentTreeCollection.activeTree().analysisResult;
                    std::string idLabel = "Id: " + std::to_string(talent->index) + ", Pos: (" + std::to_string(talent->row) + ", " + std::to_string(talent->column) + ")";
                    if (talent->type != Engine::TalentType::SWITCH) {
                        Presets::PUSH_FONT(uiData.fontsize, 1);
                        ImGui::Text(talent->getName().c_str());
                        Presets::POP_FONT();
                        ImGui::Text(idLabel.c_str());
                        ImGui::Separator();
                        for (int i = 0; i < talent->maxPoints; i++) {
                            ImGui::Text("Rank %d:", i + 1);
                            ImGui::Spacing();
                            int colIndex = result.indexToArrayColMap.at(talent->index) + i;
                            Engine::TalentPerformanceInfo& talentPerf = result.talentPerformances[colIndex];
                            size_t numSkillsets = talentPerf.skillsetDPS.size();
                            ImGui::Text("Number of skillsets: %d", numSkillsets);
                            if (numSkillsets > 0) {
                                ImGui::Separator();
                                ImGui::Text("Lowest skillset \"%s\" in import \"%s\" dps: %.1f",
                                    talentPerf.lowestDPSSkillset.first.second.c_str(),
                                    talentPerf.lowestDPSSkillset.first.first.c_str(),
                                    talentPerf.lowestDPSSkillset.second);
                                ImGui::Text("Median skillset \"%s\" in import \"%s\" dps: %.1f",
                                    talentPerf.medianDPSSkillset.first.second.c_str(),
                                    talentPerf.medianDPSSkillset.first.first.c_str(),
                                    talentPerf.medianDPSSkillset.second);
                                ImGui::Text("Highest skillset \"%s\" in import \"%s\" dps: %.1f",
                                    talentPerf.highestDPSSkillset.first.second.c_str(),
                                    talentPerf.highestDPSSkillset.first.first.c_str(),
                                    talentPerf.highestDPSSkillset.second);
                                ImGui::Text("Average skillset dps: %.1f",
                                    talentPerf.averageDPSSkillset.second);
                                ImGui::Separator();
                                ImGui::Text("Lowest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                    talentPerf.lowestDPSSkillsetWithoutTalent.first.second.c_str(),
                                    talentPerf.lowestDPSSkillsetWithoutTalent.first.first.c_str(),
                                    talentPerf.lowestDPSSkillsetWithoutTalent.second);
                                ImGui::SameLine();
                                PrintColoredRatio(talentPerf.lowestDPSSkillsetWithoutTalent.second, talentPerf.lowestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                                ImGui::Text("Median skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                    talentPerf.medianDPSSkillsetWithoutTalent.first.second.c_str(),
                                    talentPerf.medianDPSSkillsetWithoutTalent.first.first.c_str(),
                                    talentPerf.medianDPSSkillsetWithoutTalent.second);
                                ImGui::SameLine();
                                PrintColoredRatio(talentPerf.medianDPSSkillsetWithoutTalent.second, talentPerf.medianDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                                ImGui::Text("Highest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                    talentPerf.highestDPSSkillsetWithoutTalent.first.second.c_str(),
                                    talentPerf.highestDPSSkillsetWithoutTalent.first.first.c_str(),
                                    talentPerf.highestDPSSkillsetWithoutTalent.second);
                                ImGui::SameLine();
                                PrintColoredRatio(talentPerf.highestDPSSkillsetWithoutTalent.second, talentPerf.highestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                                ImGui::Text("Average skillset dps without this talent: %.1f",
                                    talentPerf.averageDPSSkillsetWithoutTalent.second);
                                ImGui::SameLine();
                                PrintColoredRatio(talentPerf.averageDPSSkillsetWithoutTalent.second, talentPerf.averageDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                                ImGui::Separator();
                                ImGui::Text("Absolute ranking: %.2f%%", result.talentAbsolutePositionRankings[colIndex] * 100.0f);
                                ImGui::Text("Relative performance: %.2f%%", result.talentRelativePerformanceRankings[colIndex] * 100.0f);
                                ImGui::Text("Skillset distribution:");
                                ImGui::PlotHistogramRedGreen("##tooltipSkillsetsDistributionHistogram",
                                    &talentPerf.skillsetDPS[0],
                                    static_cast<int>(numSkillsets),
                                    talentPerf.skillsetDPSNames.data(),
                                    0, NULL,
                                    0.95f * result.lowestDPSSkillset.second,
                                    1.05f * result.highestDPSSkillset.second,
                                    ImVec2(ImGui::GetContentRegionAvail().x, 80.0f));
                            }
                            if (i < talent->maxPoints - 1) {
                                ImGui::Separator();
                            }
                        }
                    }
                    else {
                        Presets::PUSH_FONT(uiData.fontsize, 1);
                        ImGui::Text(talent->name.c_str());
                        Presets::POP_FONT();
                        ImGui::Text(idLabel.c_str());
                        ImGui::Separator();

                        int colIndex = result.indexToArrayColMap.at(talent->index);
                        Engine::TalentPerformanceInfo& talentPerf = result.talentPerformances[colIndex];
                        size_t numSkillsets = talentPerf.skillsetDPS.size();
                        ImGui::Text("Number of skillsets: %d", numSkillsets);
                        if (numSkillsets > 0) {
                            ImGui::Separator();
                            ImGui::Text("Lowest skillset \"%s\" in import \"%s\" dps: %.1f",
                                talentPerf.lowestDPSSkillset.first.second.c_str(),
                                talentPerf.lowestDPSSkillset.first.first.c_str(),
                                talentPerf.lowestDPSSkillset.second);
                            ImGui::Text("Median skillset \"%s\" in import \"%s\" dps: %.1f",
                                talentPerf.medianDPSSkillset.first.second.c_str(),
                                talentPerf.medianDPSSkillset.first.first.c_str(),
                                talentPerf.medianDPSSkillset.second);
                            ImGui::Text("Highest skillset \"%s\" in import \"%s\" dps: %.1f",
                                talentPerf.highestDPSSkillset.first.second.c_str(),
                                talentPerf.highestDPSSkillset.first.first.c_str(),
                                talentPerf.highestDPSSkillset.second);
                            ImGui::Text("Average skillset dps: %.1f",
                                talentPerf.averageDPSSkillset.second);
                            ImGui::Separator();
                            ImGui::Text("Lowest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                talentPerf.lowestDPSSkillsetWithoutTalent.first.second.c_str(),
                                talentPerf.lowestDPSSkillsetWithoutTalent.first.first.c_str(),
                                talentPerf.lowestDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.lowestDPSSkillsetWithoutTalent.second, talentPerf.lowestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Text("Median skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                talentPerf.medianDPSSkillsetWithoutTalent.first.second.c_str(),
                                talentPerf.medianDPSSkillsetWithoutTalent.first.first.c_str(),
                                talentPerf.medianDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.medianDPSSkillsetWithoutTalent.second, talentPerf.medianDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Text("Highest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                talentPerf.highestDPSSkillsetWithoutTalent.first.second.c_str(),
                                talentPerf.highestDPSSkillsetWithoutTalent.first.first.c_str(),
                                talentPerf.highestDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.highestDPSSkillsetWithoutTalent.second, talentPerf.highestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Text("Average skillset dps without this talent: %.1f",
                                talentPerf.averageDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.averageDPSSkillsetWithoutTalent.second, talentPerf.averageDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Separator();
                            ImGui::Text("Absolute ranking: %.2f%%", result.talentAbsolutePositionRankings[colIndex] * 100.0f);
                            ImGui::Text("Relative performance: %.2f%%", result.talentRelativePerformanceRankings[colIndex] * 100.0f);
                            ImGui::Text("Skillset distribution:");
                            ImGui::PlotHistogramRedGreen("##tooltipSkillsetsDistributionHistogram",
                                &talentPerf.skillsetDPS[0],
                                static_cast<int>(numSkillsets),
                                talentPerf.skillsetDPSNames.data(),
                                0, NULL,
                                0.95f * result.lowestDPSSkillset.second,
                                1.05f * result.highestDPSSkillset.second,
                                ImVec2(ImGui::GetContentRegionAvail().x, 80.0f));
                        }

                        ImGui::Separator();

                        Presets::PUSH_FONT(uiData.fontsize, 1);
                        ImGui::Text(talent->nameSwitch.c_str());
                        Presets::POP_FONT();
                        ImGui::Text(idLabel.c_str());
                        ImGui::Separator();
                        
                        colIndex = result.indexToArrayColMap.at(talent->index) + 1;
                        Engine::TalentPerformanceInfo& switchTalentPerf = result.talentPerformances[colIndex];
                        numSkillsets = switchTalentPerf.skillsetDPS.size();
                        ImGui::Text("Number of skillsets: %d", numSkillsets);
                        if (numSkillsets > 0) {
                            ImGui::Separator();
                            ImGui::Text("Lowest skillset \"%s\" in import \"%s\" dps: %.1f",
                                switchTalentPerf.lowestDPSSkillset.first.second.c_str(),
                                switchTalentPerf.lowestDPSSkillset.first.first.c_str(),
                                switchTalentPerf.lowestDPSSkillset.second);
                            ImGui::Text("Median skillset \"%s\" in import \"%s\" dps: %.1f",
                                switchTalentPerf.medianDPSSkillset.first.second.c_str(),
                                switchTalentPerf.medianDPSSkillset.first.first.c_str(),
                                switchTalentPerf.medianDPSSkillset.second);
                            ImGui::Text("Highest skillset \"%s\" in import \"%s\" dps: %.1f",
                                switchTalentPerf.highestDPSSkillset.first.second.c_str(),
                                switchTalentPerf.highestDPSSkillset.first.first.c_str(),
                                switchTalentPerf.highestDPSSkillset.second);
                            ImGui::Text("Average skillset dps: %.1f",
                                switchTalentPerf.averageDPSSkillset.second);
                            ImGui::Separator();
                            ImGui::Text("Lowest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                switchTalentPerf.lowestDPSSkillsetWithoutTalent.first.second.c_str(),
                                switchTalentPerf.lowestDPSSkillsetWithoutTalent.first.first.c_str(),
                                switchTalentPerf.lowestDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.lowestDPSSkillsetWithoutTalent.second, talentPerf.lowestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Text("Median skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                switchTalentPerf.medianDPSSkillsetWithoutTalent.first.second.c_str(),
                                switchTalentPerf.medianDPSSkillsetWithoutTalent.first.first.c_str(),
                                switchTalentPerf.medianDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.medianDPSSkillsetWithoutTalent.second, talentPerf.medianDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Text("Highest skillset without this talent \"%s\" in import \"%s\" dps: %.1f",
                                switchTalentPerf.highestDPSSkillsetWithoutTalent.first.second.c_str(),
                                switchTalentPerf.highestDPSSkillsetWithoutTalent.first.first.c_str(),
                                switchTalentPerf.highestDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.highestDPSSkillsetWithoutTalent.second, talentPerf.highestDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Text("Average skillset dps without this talent: %.1f",
                                switchTalentPerf.averageDPSSkillsetWithoutTalent.second);
                            ImGui::SameLine();
                            PrintColoredRatio(talentPerf.averageDPSSkillsetWithoutTalent.second, talentPerf.averageDPSSkillset.second, -0.5f, 0.5f, 1, "(%+.1f%%)");
                            ImGui::Separator();
                            ImGui::Text("Absolute ranking: %.2f%%", result.talentAbsolutePositionRankings[colIndex] * 100.0f);
                            ImGui::Text("Relative performance: %.2f%%", result.talentRelativePerformanceRankings[colIndex] * 100.0f);
                            ImGui::Text("Skillset distribution:");
                            ImGui::PlotHistogramRedGreen("##tooltipSkillsetsDistributionHistogram",
                                &switchTalentPerf.skillsetDPS[0],
                                static_cast<int>(numSkillsets),
                                switchTalentPerf.skillsetDPSNames.data(),
                                0, NULL,
                                0.95f * result.lowestDPSSkillset.second,
                                1.05f * result.highestDPSSkillset.second,
                                ImVec2(ImGui::GetContentRegionAvail().x, 80.0f));
                        }
                    }
                }
            }break;
            }
        }
        ImGui::End();
        if (ImGui::Begin("SearchWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
            ImGui::Text("Search:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
            if (ImGui::InputText("##loadoutSolverSearchInput", &uiData.talentSearchString, ImGuiInputTextFlags_CallbackCharFilter, TextFilters::FilterNameLetters)) {
                //update the filteredTalentList
                uiData.searchedTalents.clear();

                Engine::filterTalentSearch(uiData.talentSearchString, uiData.searchedTalents, talentTreeCollection.activeTree());
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("(also accepts \"active\", \"passive\", \"switch\")");
        }
        ImGui::End();
    }

    void placeSimAnalysisTreeElements(UIData& uiData, TalentTreeCollection& talentTreeCollection) {
        Engine::TalentTree& tree = talentTreeCollection.activeTree();
        int talentHalfSpacing = static_cast<int>(uiData.treeEditorBaseTalentHalfSpacing * uiData.treeEditorZoomFactor);
        int talentSize = static_cast<int>(uiData.treeEditorBaseTalentSize * uiData.treeEditorZoomFactor);
        float talentWindowPaddingX = static_cast<float>(uiData.treeEditorTalentWindowPaddingX);
        float talentWindowPaddingY = static_cast<float>(uiData.treeEditorTalentWindowPaddingY);
        ImVec2 origin = ImVec2(talentWindowPaddingX, talentWindowPaddingY);
        //calculate full tree width and if that is < window width center tree
        float fullTreeWidth = (tree.maxCol - 1) * 2.0f * talentHalfSpacing;
        float windowWidth = ImGui::GetContentRegionAvail().x;
        if (fullTreeWidth + 2 * origin.x < windowWidth) {
            origin.x = 0.5f * (windowWidth - fullTreeWidth);
        }

        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 scrollOffset = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

        int maxRow = 0;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGuiStyle& imStyle = ImGui::GetStyle();

        for (auto& talent : tree.orderedTalents) {
            for (auto& child : talent.second->children) {
                drawArrowBetweenTalents(
                    talent.second,
                    child,
                    drawList,
                    windowPos,
                    scrollOffset,
                    origin,
                    talentHalfSpacing,
                    talentSize,
                    0.0f,
                    uiData);
            }
        }
        for (auto& reqInfo : tree.requirementSeparatorInfo) {
            drawList->AddLine(
                ImVec2(windowPos.x - scrollOffset.x + origin.x - 2 * talentSize, windowPos.y - scrollOffset.y + talentWindowPaddingY + (reqInfo.second - 1) * talentSize),
                ImVec2(windowPos.x - scrollOffset.x + origin.x + (tree.maxCol + 1) * talentSize, windowPos.y - scrollOffset.y + talentWindowPaddingY + (reqInfo.second - 1) * talentSize),
                ImColor(Presets::GET_TOOLTIP_TALENT_DESC_COLOR(uiData.style)),
                2.0f
            );
            drawList->AddText(
                ImVec2(windowPos.x - scrollOffset.x + origin.x - 2 * talentSize, windowPos.y - scrollOffset.y + talentWindowPaddingY + (reqInfo.second - 1) * talentSize),
                ImColor(Presets::GET_TOOLTIP_TALENT_DESC_COLOR(uiData.style)),
                (std::to_string(reqInfo.first) + " points").c_str()
            );
        }

        for (auto& talent : tree.orderedTalents) {
            maxRow = talent.second->row > maxRow ? talent.second->row : maxRow;
            float posX = origin.x + (talent.second->column - 1) * 2 * talentHalfSpacing;
            float posY = origin.y + (talent.second->row - 1) * 2 * talentHalfSpacing;
            bool talentIsSearchedFor = false;
            bool searchActive = uiData.talentSearchString != "";
            ImGui::SetCursorPos(ImVec2(posX, posY));
            Presets::PUSH_FONT(uiData.fontsize, 1);

            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
            ImGui::PushID((std::to_string(talent.second->points) + "/" + std::to_string(talent.second->maxPoints) + "##" + std::to_string(talent.second->index)).c_str());
            TextureInfo* iconContent = nullptr;
            TextureInfo* iconContentChoice = nullptr;
            if (uiData.simAnalysisIconRatingSwitch) {
                std::string buttonText;
                ImVec4 buttonCol;
                if (talentTreeCollection.activeTreeData().simAnalysisButtonRankingText.count(talent.first) 
                    && talentTreeCollection.activeTreeData().simAnalysisTalentColor.count(talent.first)) {
                    buttonText = talentTreeCollection.activeTreeData().simAnalysisButtonRankingText[talent.first].c_str();
                    buttonCol = talentTreeCollection.activeTreeData().simAnalysisTalentColor[talent.second->index];
                }
                else {
                    buttonText = "";
                    if (uiData.style == Presets::STYLES::COMPANY_GREY) {
                        buttonCol = Presets::TALENT_DEFAULT_BORDER_COLOR_COMPANY_GREY;
                    }
                    else {
                        buttonCol = Presets::TALENT_DEFAULT_BORDER_COLOR;
                    }
                }
                ImGui::PushStyleColor(ImGuiCol_Button, buttonCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonCol);
                if (talent.second->type != Engine::TalentType::ACTIVE) {
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f * uiData.treeEditorZoomFactor);
                    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 14.0f * uiData.treeEditorZoomFactor);
                }
                if (buttonCol.y >= 0.5f) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                }
                if (ImGui::Button(buttonText.c_str(), ImVec2(static_cast<float>(talentSize), static_cast<float>(talentSize)))) {
                }
                if (buttonCol.y >= 0.5f) {
                    ImGui::PopStyleColor();
                }
                if (talent.second->type != Engine::TalentType::ACTIVE) {
                    ImGui::PopStyleVar(2);
                }
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
                if (talent.second->type != Engine::TalentType::SWITCH) {
                    iconContent = uiData.iconIndexMap[talent.second->index].first;
                    ImGui::SetCursorPos(ImVec2(posX, posY));
                    ImGui::Image(
                        iconContent->texture,
                        ImVec2(static_cast<float>(talentSize), static_cast<float>(talentSize)), ImVec2(0, 0), ImVec2(1, 1)
                    );
                }
                else {
                    iconContent = uiData.iconIndexMap[talent.second->index].first;
                    iconContentChoice = uiData.iconIndexMap[talent.second->index].second;
                    float separatorWidth = 0.05f;
                    ImGui::SetCursorPos(ImVec2(posX, posY));
                    ImGui::Image(
                        iconContent->texture,
                        ImVec2(talentSize * (1.0f - separatorWidth) / 2.0f, static_cast<float>(talentSize)), ImVec2(0, 0), ImVec2((1.0f - separatorWidth) / 2.0f, 1)
                    );

                    ImGui::SetCursorPos(ImVec2(posX + talentSize * (1.0f + separatorWidth) / 2.0f, posY));
                    ImGui::Image(
                        iconContentChoice->texture,
                        ImVec2(talentSize * (1.0f - separatorWidth) / 2.0f, static_cast<float>(talentSize)), ImVec2((1.0f + separatorWidth) / 2.0f, 0), ImVec2(1, 1)
                    );

                    ImGui::SetCursorPos(ImVec2(posX, posY));
                    ImGui::Image(
                        uiData.talentIconMasks[static_cast<int>(uiData.style)][static_cast<int>(talent.second->type)].texture,
                        ImVec2(static_cast<float>(talentSize), static_cast<float>(talentSize)), ImVec2(0, 0), ImVec2(1, 1)
                    );
                }
            }
            ImGui::PopID();
            ImGui::PopStyleColor(5);
            Presets::POP_FONT();
            AttachSimAnalysisTooltip(uiData, tree.analysisResult, talent.second);

            ImGui::SetCursorPos(ImVec2(posX, posY));
            ImGui::Image(
                uiData.talentIconMasks[static_cast<int>(uiData.style)][static_cast<int>(talent.second->type)].texture,
                ImVec2(static_cast<float>(talentSize), static_cast<float>(talentSize)), ImVec2(0, 0), ImVec2(1, 1)
            );

            ImGui::SetCursorPos(ImVec2(
                posX - 0.5f * (uiData.treeEditorZoomFactor * uiData.redIconGlow[static_cast<int>(talent.second->type)].width - talentSize),
                posY - 0.5f * (uiData.treeEditorZoomFactor * uiData.redIconGlow[static_cast<int>(talent.second->type)].height - talentSize)));
            if (uiData.talentSearchString != "" && std::find(uiData.searchedTalents.begin(), uiData.searchedTalents.end(), talent.second) != uiData.searchedTalents.end()) {
                talentIsSearchedFor = true;
                ImGui::Image(
                    uiData.blueIconGlow[static_cast<int>(talent.second->type)].texture,
                    ImVec2(
                        uiData.treeEditorZoomFactor * uiData.blueIconGlow[static_cast<int>(talent.second->type)].width,
                        uiData.treeEditorZoomFactor * uiData.blueIconGlow[static_cast<int>(talent.second->type)].height),
                    ImVec2(0, 0), ImVec2(1, 1),
                    ImVec4(1, 1, 1, 1.0f - 0.5f * (uiData.style == Presets::STYLES::COMPANY_GREY))
                );
            }
            drawSimAnalysisShapeAroundTalent(
                talent.second,
                drawList,
                imStyle.Colors,
                ImVec2(posX, posY),
                talentSize,
                ImGui::GetWindowPos(),
                ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY()),
                uiData,
                talentTreeCollection,
                searchActive,
                talentIsSearchedFor);
        }
        if (ImGui::IsWindowHovered() && (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift))
            && !(ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper))) {
            for (auto& talent : tree.orderedTalents) {
                float posX = origin.x + (talent.second->column - 1) * 2 * talentHalfSpacing;
                float posY = origin.y + (talent.second->row - 1) * 2 * talentHalfSpacing;
                ImVec2 textBoxPos = ImVec2(
                    posX - 0.5f * talentSize + ImGui::GetWindowPos().x - ImGui::GetScrollX(),
                    posY - 0.5f * talentSize + ImGui::GetWindowPos().y - ImGui::GetScrollY()
                );
                ImVec2 bounds = ImVec2(textBoxPos.x + 2.0f * talentSize, textBoxPos.y + 2.0f * talentSize);
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(textBoxPos, bounds, IM_COL32(0, 0, 0, 255));
                draw_list->AddRect(textBoxPos, bounds, IM_COL32(255, 255, 255, 255), 0, 0, 2.0f);
                std::string infoText = talent.second->type == Engine::TalentType::SWITCH ? talent.second->getName() + " / " + talent.second->getNameSwitch() : talent.second->getName();
                Presets::PUSH_FONT(uiData.fontsize, 3);
                AddWrappedText(infoText, textBoxPos, 5.0f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f * talentSize, 2.0f * talentSize, ImGui::GetWindowDrawList());
                Presets::POP_FONT();
            }
        }

        ImGui::SetCursorPos(ImVec2(origin.x, 1.5f * origin.y + maxRow * 2 * talentHalfSpacing));
        ImGui::InvisibleButton(
            "##invisbuttonedit",
            ImVec2(
                origin.x + (tree.maxCol - 1) * 2 * talentHalfSpacing,
                0.5f * origin.y
            )
        );

        if ((uiData.maxScrollBuffer.x != ImGui::GetScrollMaxX() || uiData.maxScrollBuffer.y != ImGui::GetScrollMaxY())
            && uiData.treeEditorWindowSize.x != 0 && uiData.treeEditorWindowSize.y != 0) {
            uiData.treeEditorRelWorldMousePos = ImVec2(
                (uiData.scrollBuffer.x + (uiData.treeEditorMousePos.x - uiData.treeEditorWindowPos.x)) / (uiData.maxScrollBuffer.x + uiData.treeEditorWindowSize.x),
                (uiData.scrollBuffer.y + (uiData.treeEditorMousePos.y - uiData.treeEditorWindowPos.y)) / (uiData.maxScrollBuffer.y + uiData.treeEditorWindowSize.y)
            );
            ImVec2 targetPosAbs = ImVec2(
                uiData.treeEditorRelWorldMousePos.x * (ImGui::GetScrollMaxX() + uiData.treeEditorWindowSize.x),
                uiData.treeEditorRelWorldMousePos.y * (ImGui::GetScrollMaxY() + uiData.treeEditorWindowSize.y)
            );
            ImVec2 targetScreenPosAbs = ImVec2(
                targetPosAbs.x - (uiData.treeEditorMousePos.x - uiData.treeEditorWindowPos.x),
                targetPosAbs.y - (uiData.treeEditorMousePos.y - uiData.treeEditorWindowPos.y)
            );
            ImGui::SetScrollX(targetScreenPosAbs.x);
            ImGui::SetScrollY(targetScreenPosAbs.y);

            uiData.maxScrollBuffer = ImVec2(ImGui::GetScrollMaxX(), ImGui::GetScrollMaxY());
            uiData.scrollBuffer.x = std::clamp(targetScreenPosAbs.x, 0.0f, uiData.maxScrollBuffer.x);
            uiData.scrollBuffer.y = std::clamp(targetScreenPosAbs.y, 0.0f, uiData.maxScrollBuffer.y);
        }
    }

    SimData FetchSimData(std::string& urlOrPath) {
        SimData data;
        std::regex pathRegex{"[a-zA-Z]:(?:[\\\\\\/]+[^\\/\\\\\\<\\>\\\"\\|\\*\\?\\:]+)*(.txt)?"};
        bool isPath = std::regex_match(urlOrPath, pathRegex);
        std::regex raidbotsRegex{ "https:\\/\\/www.raidbots.com\\/simbot\\/report\\/[a-zA-Z0-9]+" };
        bool isRaidbots = std::regex_match(urlOrPath, raidbotsRegex);

        if (isPath) {
            std::filesystem::path dataPath{ urlOrPath };
            if (std::filesystem::is_regular_file(dataPath)) {
                SimFile res = ReadSimFile(dataPath);
                data.simOutputNames.push_back(res.simOutputName);
                data.simOutputs.push_back(res.simOutput);
            }
            else if (std::filesystem::is_directory(dataPath)) {
                for (auto& entry : std::filesystem::directory_iterator{ dataPath }) {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                        SimFile res = ReadSimFile(entry.path());
                        data.simOutputNames.push_back(res.simOutputName);
                        data.simOutputs.push_back(res.simOutput);
                    }
                }
            }
        }
        else if (isRaidbots) {
            //SimFile res = ReadRaidbots(urlOrPath);
            urlOrPath = "Fetching data from Raidbots not yet supported. You can still download the outputs.txt from Raidbots and provide an absolute path.";
        }
        else {
            urlOrPath = "Invalid URL or absolute path!";
        }
        return data;
    }

    SimFile ReadSimFile(std::filesystem::path path) {
        SimFile simFile;
        simFile.simOutputName = path.filename().string();
        std::ifstream filestream{ path };
        std::string line;
        while (std::getline(filestream, line)) {
            simFile.simOutput.push_back(line);
        }
        return simFile;
    }

    SimFile ReadRaidbots(std::string url) {
        SimFile simFile;
        std::vector<std::string> urlParts = Engine::splitString(url, "/");
        std::string& hash = urlParts[urlParts.size() - 1];
        simFile.simOutputName = hash;
        std::string fetchUrl = "https://www.raidbots.com/reports/" + hash + "/output.txt";

        std::string result;

        CURL* curl;
        CURLcode res;
        char* errorBuf = new char[2048]();
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, fetchUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuf);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if (CURLE_OK != res) {
                return simFile;
            }
        }
        simFile.simOutput = Engine::splitString(result, "\n");
        return simFile;
    }

    static size_t write_memory(void* buffer, size_t size, size_t nmemb, void* param)
    {
        std::string& text = *static_cast<std::string*>(param);
        size_t totalsize = size * nmemb;
        text.append(static_cast<char*>(buffer), totalsize);
        return totalsize;
    }

    void AnalyzeRawResults(Engine::TalentTree& tree) {
        Engine::AnalysisResult result;
        int col = 0;
        for (auto& indexTalentPair : tree.orderedTalents) {
            result.indexToArrayColMap[indexTalentPair.first] = col;
            if (indexTalentPair.second->type == Engine::TalentType::SWITCH) {
                col += 2;
            }
            else {
                col += indexTalentPair.second->maxPoints;
            }
        }

        std::vector<std::pair<std::string, std::string>> tempSkillsetNames;
        for (auto& simRes : tree.simAnalysisRawResults) {
            for (int i = 0; i < simRes.dps.size(); i++) {
                std::vector<int> talentSelection(col, 0);
                for (auto& indexPointsPair : simRes.skillsets[i].assignedSkillPoints) {
                    if (indexPointsPair.second > 0) {
                        talentSelection[result.indexToArrayColMap[indexPointsPair.first] + indexPointsPair.second - 1] = 1;
                    }
                }
                tempSkillsetNames.emplace_back(simRes.name, simRes.skillsets[i].name);
                result.talentArray.push_back(talentSelection);
                result.skillsetDPS.push_back(simRes.dps[i]);
                result.skillsetDPSNames.push_back(const_cast<char*>(simRes.skillsets[i].name.c_str()));
            }
        }
        result.skillsetCount = static_cast<int>(result.skillsetDPS.size());
        if (result.skillsetCount == 0) {
            result.indexToArrayColMap.clear();
            tree.analysisResult = std::move(result);
            return;
        }
        //sort talentArray and skillsetDPS vectors in the same order
        auto p = sort_permutation(result.skillsetDPS,
            [](float const& a, float const& b) { return a < b; });

        result.skillsetDPS = apply_permutation(result.skillsetDPS, p);
        result.skillsetDPSNames = apply_permutation(result.skillsetDPSNames, p);
        result.talentArray = apply_permutation(result.talentArray, p);
        tempSkillsetNames = apply_permutation(tempSkillsetNames, p);

        result.lowestDPSSkillset = std::pair<std::pair<std::string, std::string>, float>(tempSkillsetNames[0], result.skillsetDPS[0]);
        int medianIndex = (result.skillsetCount - 1) / 2;
        result.medianDPSSkillset = std::pair<std::pair<std::string, std::string>, float>(tempSkillsetNames[medianIndex], result.skillsetDPS[medianIndex]);
        float totalDPS = 0.0f;
        for (float dps : result.skillsetDPS) {
            totalDPS += dps;
        }
        result.averageDPSSkillset = std::pair<std::pair<std::string, std::string>, float>({ "N/A", "N/A" }, totalDPS / result.skillsetDPS.size());
        result.highestDPSSkillset = std::pair<std::pair<std::string, std::string>, float>(tempSkillsetNames[result.skillsetCount - 1], result.skillsetDPS[result.skillsetCount - 1]);
        result.referenceDPS = result.highestDPSSkillset.second;

        //calculate talent performances
        std::vector<std::pair<std::string, std::string>> tempTalentSkillsetNames;
        std::vector<std::pair<std::string, std::string>> tempWithoutTalentSkillsetNames;
        for (auto& indexTalentPair : tree.orderedTalents) {
            int maxP = indexTalentPair.second->type == Engine::TalentType::SWITCH ? 2 : indexTalentPair.second->maxPoints;
            for (int points = 1; points <= maxP; points++) {
                int colIndex = result.indexToArrayColMap[indexTalentPair.first] + points - 1;
                Engine::TalentPerformanceInfo tInfo;
                Engine::TalentPerformanceInfo withoutTInfo;
                tempTalentSkillsetNames.clear();
                tempWithoutTalentSkillsetNames.clear();
                for (int i = 0; i < result.talentArray.size(); i++) {
                    if (result.talentArray[i][colIndex] > 0) {
                        tInfo.skillsetDPS.push_back(result.skillsetDPS[i]);
                        tInfo.skillsetDPSNames.push_back(result.skillsetDPSNames[i]);
                        tempTalentSkillsetNames.push_back(tempSkillsetNames[i]);
                    }
                    else {
                        withoutTInfo.skillsetDPS.push_back(result.skillsetDPS[i]);
                        withoutTInfo.skillsetDPSNames.push_back(result.skillsetDPSNames[i]);
                        tempWithoutTalentSkillsetNames.push_back(tempSkillsetNames[i]);
                    }
                }
                if (tempTalentSkillsetNames.size() > 0 && tInfo.skillsetDPS.size() > 0) {
                    tInfo.lowestDPSSkillset = std::pair<std::pair<std::string, std::string>, float>(tempTalentSkillsetNames[0], tInfo.skillsetDPS[0]);
                    size_t medianIndex = (tInfo.skillsetDPS.size() - 1) / 2;
                    tInfo.medianDPSSkillset = std::pair<std::pair<std::string, std::string>, float>(tempTalentSkillsetNames[medianIndex], tInfo.skillsetDPS[medianIndex]);
                    float totalDPS = 0.0f;
                    for (float dps : tInfo.skillsetDPS) {
                        totalDPS += dps;
                    }
                    tInfo.averageDPSSkillset = std::pair<std::pair<std::string, std::string>, float>({"N/A", "N/A"}, totalDPS / tInfo.skillsetDPS.size());
                    tInfo.highestDPSSkillset = std::pair<std::pair<std::string, std::string>, float>(tempTalentSkillsetNames[tInfo.skillsetDPS.size() - 1], tInfo.skillsetDPS[tInfo.skillsetDPS.size() - 1]);

                    if (tempWithoutTalentSkillsetNames.size() > 0 && withoutTInfo.skillsetDPS.size() > 0) {
                        tInfo.lowestDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>(tempWithoutTalentSkillsetNames[0], withoutTInfo.skillsetDPS[0]);
                        medianIndex = (withoutTInfo.skillsetDPS.size() - 1) / 2;
                        tInfo.medianDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>(tempWithoutTalentSkillsetNames[medianIndex], withoutTInfo.skillsetDPS[medianIndex]);
                        totalDPS = 0.0f;
                        for (float dps : withoutTInfo.skillsetDPS) {
                            totalDPS += dps;
                        }
                        tInfo.averageDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>({ "N/A", "N/A" }, totalDPS / withoutTInfo.skillsetDPS.size());
                        tInfo.highestDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>(tempWithoutTalentSkillsetNames[withoutTInfo.skillsetDPS.size() - 1], withoutTInfo.skillsetDPS[withoutTInfo.skillsetDPS.size() - 1]);
                    }
                    else {
                        tInfo.lowestDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>({ "N/A", "N/A" }, 0.0f);
                        tInfo.medianDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>({ "N/A", "N/A" }, 0.0f);
                        tInfo.averageDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>({ "N/A", "N/A" }, 0.0f);
                        tInfo.highestDPSSkillsetWithoutTalent = std::pair<std::pair<std::string, std::string>, float>({ "N/A", "N/A" }, 0.0f);
                    }
                }

                result.talentPerformances.push_back(tInfo);
            }
        }

        tree.analysisResult = std::move(result);
    }

    void CalculateAnalysisRankings(UIData& uiData, Engine::AnalysisResult& result) {
        result.minRelPerf = 1.0f;
        result.talentAbsolutePositionRankings.clear();
        result.talentRelativePerformanceRankings.clear();
        if (result.skillsetCount == 0 || result.talentPerformances.size() == 0) {
            return;
        }

        Engine::PerformanceMetric performanceMetric = static_cast<Engine::PerformanceMetric>(uiData.topMedianPerformanceSwitch);

        const int DPS_SLOTS = 6;
        std::vector<std::pair<int, std::vector<float>>> performance;
        int numTalents = static_cast<int>(result.talentPerformances.size());
        //TTMNOTE: dps[0] holds the relative performance value and is main sorting value, all additional criteria are put in dps[1...], relevant for TOPMEDIAN
        for (int i = 0; i < numTalents; i++) {
            switch (performanceMetric) {
            case Engine::PerformanceMetric::TOP1: {
                performance.emplace_back(i, std::vector<float>(DPS_SLOTS, result.talentPerformances[i].highestDPSSkillset.second));
            }break;
            case Engine::PerformanceMetric::TOP3: {
                std::vector<float> dps(DPS_SLOTS, 0);
                float avg = 0.0f;
                if (result.talentPerformances[i].skillsetDPS.size() >= 3) {
                    for (int j = 0; j < 3; j++) {
                        dps[j + 1] = result.talentPerformances[i].skillsetDPS[result.talentPerformances[i].skillsetDPS.size() - 1 - j];
                        avg += dps[j + 1];
                    }
                }
                dps[0] = avg / 3;
                performance.emplace_back(i, dps);
            }break;
            case Engine::PerformanceMetric::TOP5: {
                std::vector<float> dps(DPS_SLOTS, 0);
                float avg = 0.0f;
                if (result.talentPerformances[i].skillsetDPS.size() >= 5) {
                    for (int j = 0; j < 5; j++) {
                        dps[j + 1] = result.talentPerformances[i].skillsetDPS[result.talentPerformances[i].skillsetDPS.size() - 1 - j];
                        avg += dps[j + 1];
                    }
                }
                dps[0] = avg / 5;
                performance.emplace_back(i, dps);
            }break;
            case Engine::PerformanceMetric::MEDIAN: {
                performance.emplace_back(i, std::vector<float>(DPS_SLOTS, result.talentPerformances[i].medianDPSSkillset.second));
            }break;
            case Engine::PerformanceMetric::AVERAGE: {
                performance.emplace_back(i, std::vector<float>(DPS_SLOTS, result.talentPerformances[i].averageDPSSkillset.second));
            }break;
            case Engine::PerformanceMetric::TOPMEDIAN: {
                std::vector<float> dps(DPS_SLOTS, 0);
                dps[0] = result.talentPerformances[i].highestDPSSkillset.second;
                dps[1] = result.talentPerformances[i].medianDPSSkillset.second;
                performance.emplace_back(i, dps);
            }break;
            }
        }
        std::sort(performance.begin(), performance.end(), [](auto& left, auto& right) {
            return left.second < right.second;
            });
        float maxPerf = performance[performance.size() - 1].second[0];
        float minPerf = maxPerf;
        for (auto& p : performance) {
            if (p.second[0] > 0.0f && p.second[0] < minPerf) {
                minPerf = p.second[0];
            }
        }
        //this is needed for color glow texture generation, since that is a separate function
        result.minRelPerf = minPerf / maxPerf;

        result.talentAbsolutePositionRankings.resize(numTalents);
        result.talentRelativePerformanceRankings.resize(numTalents);

        std::map<std::vector<float>, int> rankMap;
        int rankCounter = 0;
        for (int i = 0; i < numTalents; i++) {
            if (performance[i].second[0] > 0.0f && !rankMap.count(performance[i].second)) {
                rankMap[performance[i].second] = rankCounter;
                rankCounter++;
            }
        }
        for (int i = 0; i < numTalents; i++) {
            if (rankCounter <= 1) {
                result.talentAbsolutePositionRankings[performance[i].first] = performance[i].second[0] > 0.0f ? 1.0f : -1.0f;
            }
            else {
                result.talentAbsolutePositionRankings[performance[i].first] = performance[i].second[0] > 0.0f ? rankMap[performance[i].second] * 1.0f / (rankCounter - 1) : -1.0f;
            }
            result.talentRelativePerformanceRankings[performance[i].first] = performance[i].second[0] > 0.0f ? performance[i].second[0] / maxPerf : -1.0f;
        }
    }


    void UpdateColorGlowTextures(UIData& uiData, TalentTreeCollection& talentTreeCollection, Engine::AnalysisResult& result) {
        //create glows and colors
        //glow always goes from red to green 
        //but for absolute rankings red = 0, green = 1
        //and for relative rankings red = minRelPerf, green = 1
        //store pairs of (color, glowTexture) in uiData
        talentTreeCollection.activeTreeData().simAnalysisTalentColor.clear();

        if (result.talentAbsolutePositionRankings.size() == 0 || result.talentRelativePerformanceRankings.size() == 0) {
            return;
        }

        bool calculateAbsolutePositionRanking = uiData.relativeDpsRankingSwitch;
        for (auto& indexTalentPair : talentTreeCollection.activeTree().orderedTalents) {
            float extremeRanking = uiData.showLowestHighestSwitch ? -1.0f : 2.0f;
            std::vector<unsigned char> color(3);
            if (calculateAbsolutePositionRanking) {
                int maxP = indexTalentPair.second->maxPoints;
                if (indexTalentPair.second->type == Engine::TalentType::SWITCH) {
                    maxP = 2;
                }
                if (uiData.showLowestHighestSwitch) {
                    //get highest position ranking value of multipoint/switch/regular talent
                    for (int i = 0; i < maxP; i++) {
                        if (result.talentAbsolutePositionRankings[result.indexToArrayColMap[indexTalentPair.first] + i] > extremeRanking) {
                            extremeRanking = result.talentAbsolutePositionRankings[result.indexToArrayColMap[indexTalentPair.first] + i];
                        }
                    }
                }
                else {
                    //get lowest position ranking value of multipoint/switch/regular talent
                    for (int i = 0; i < maxP; i++) {
                        if (result.talentAbsolutePositionRankings[result.indexToArrayColMap[indexTalentPair.first] + i] >= 0.0f
                            && result.talentAbsolutePositionRankings[result.indexToArrayColMap[indexTalentPair.first] + i] < extremeRanking) {
                            extremeRanking = result.talentAbsolutePositionRankings[result.indexToArrayColMap[indexTalentPair.first] + i];
                        }
                    }
                }
                //get interpolated red-green color for 0=red, 1=green
                color[0] = 2.0f * (1.0f - extremeRanking) >= 1.0f ? 255 : static_cast<unsigned char>(2.0f * (1.0f - extremeRanking) * 255);
                color[1] = 2.0f * extremeRanking >= 1.0f ? 255 : static_cast<unsigned char>(2.0f * extremeRanking * 255);
                color[2] = 0;
            }
            else {
                //get highest position ranking value of multipoint/switch/regular talent
                int maxP = indexTalentPair.second->maxPoints;
                if (indexTalentPair.second->type == Engine::TalentType::SWITCH) {
                    maxP = 2;
                }
                if (uiData.showLowestHighestSwitch) {
                    for (int i = 0; i < maxP; i++) {
                        if (result.talentRelativePerformanceRankings[result.indexToArrayColMap[indexTalentPair.first] + i] > extremeRanking) {
                            extremeRanking = result.talentRelativePerformanceRankings[result.indexToArrayColMap[indexTalentPair.first] + i];
                        }
                    }
                }
                else {
                    for (int i = 0; i < maxP; i++) {
                        if (result.talentRelativePerformanceRankings[result.indexToArrayColMap[indexTalentPair.first] + i] >= 0.0f
                            && result.talentRelativePerformanceRankings[result.indexToArrayColMap[indexTalentPair.first] + i] < extremeRanking) {
                            extremeRanking = result.talentRelativePerformanceRankings[result.indexToArrayColMap[indexTalentPair.first] + i];
                        }
                    }
                }
                //get interpolated red-green color for minRelPerf=red, 1=green
                float intermediate = (extremeRanking + 0.0001f - result.minRelPerf) / (1.0001f - result.minRelPerf);
                color[0] = 2.0f * (1.0f - intermediate) >= 1.0f ? 255 : static_cast<unsigned char>(2.0f * (1.0f - intermediate) * 255);
                color[1] = 2.0f * intermediate >= 1.0f ? 255 : static_cast<unsigned char>(2.0f * intermediate * 255);
                color[2] = 0;
            }
            if(extremeRanking >= 0.0f && extremeRanking <= 1.0f){
                //add it to map with talent index as key
                talentTreeCollection.activeTreeData().simAnalysisTalentColor[indexTalentPair.first] = ImVec4(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, 1.0f);
                std::stringstream stream;
                int precision = (extremeRanking == 1.0f || extremeRanking == 0.0f) ? 0 : 1;
                stream << std::fixed << std::setprecision(precision) << extremeRanking * 100.0f;
                talentTreeCollection.activeTreeData().simAnalysisButtonRankingText[indexTalentPair.first] = stream.str();
            }
        }

    }

    //taken from: https://stackoverflow.com/questions/17074324/how-can-i-sort-two-vectors-in-the-same-way-with-criteria-that-uses-only-one-of
    template <typename T, typename Compare>
    std::vector<std::size_t> sort_permutation(
        const std::vector<T>& vec,
        Compare compare)
    {
        std::vector<std::size_t> p(vec.size());
        std::iota(p.begin(), p.end(), 0);
        std::sort(p.begin(), p.end(),
            [&](std::size_t i, std::size_t j) { return compare(vec[i], vec[j]); });
        return p;
    }

    //taken from: https://stackoverflow.com/questions/17074324/how-can-i-sort-two-vectors-in-the-same-way-with-criteria-that-uses-only-one-of
    template <typename T>
    std::vector<T> apply_permutation(
        const std::vector<T>& vec,
        const std::vector<std::size_t>& p)
    {
        std::vector<T> sorted_vec(vec.size());
        std::transform(p.begin(), p.end(), sorted_vec.begin(),
            [&](std::size_t i) { return vec[i]; });
        return sorted_vec;
    }
}