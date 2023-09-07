#include "GUI.h"

#include "Engine.h"
#include "Utils.h"
#include <imgui_impl_glfw.h>

using namespace ImGui;

ImGuiIO& GUI::IO()
{
    return GetIO();
}

GUI::GUI(Engine& engine)
    : engine(engine)
{
    const std::string regularFontPath = fontPath("Caveat", "Regular").string();
    const std::string boldFontPath = fontPath("Caveat", "Bold").string();

    IMGUI_CHECKVERSION();
    CreateContext();
    ImGuiIO& io = IO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    titleFont = io.Fonts->AddFontFromFileTTF(boldFontPath.data(), 128.0f);
    headingFont = io.Fonts->AddFontFromFileTTF(boldFontPath.data(), 64.0f);
    textFont = io.Fonts->AddFontFromFileTTF(regularFontPath.data(), 64.0f);
    descriptionFont = io.Fonts->AddFontFromFileTTF(regularFontPath.data(), 32.0f);
    StyleColorsDark();
    engine.glfw.initializeGuiWindow();
    io.BackendRendererName = "Vulkan";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    engine.vulkan.initializeGuiPipeline();
}

ImageData GUI::fontTexture()
{
    ImGuiIO& io = IO();
    ImageData fontTexture{};
    io.Fonts->GetTexDataAsRGBA32(reinterpret_cast<uint8_t**>(&fontTexture.data), &fontTexture.width,
                                 &fontTexture.height);
    fontTexture.size = fontTexture.width * fontTexture.height * fontTexture.channels;
    return fontTexture;
}

void GUI::Text(const std::string& text, const ImVec2& position, float opacity)
{
    const auto windowSize = GetWindowSize();
    const auto textSize = CalcTextSize(text.data());

    SetCursorPos({
        position.x * (windowSize.x - textSize.x),
        position.y * (windowSize.y - textSize.y),
    });
    PushStyleColor(ImGuiCol_Text, {1.0f, 1.0f, 1.0f, opacity});
    ImGui::Text("%s", text.data());
    PopStyleColor();
}

void GUI::Intro()
{
    if (interval(10.225f, 13.5f).contains(engine.stateTime))
    {
        PushFont(titleFont);
        Text("Little Star", {0.5f, 0.45f}, fade(engine.stateTime, 10.225f, 10.725f, 12.0f, 13.5f));
        PopFont();
    }
}

void GUI::Finale()
{
    if (interval(19.25f, 24.5f).contains(engine.stateTime))
    {
        PushFont(titleFont);
        Text("Little Star", {0.5f, 0.45f}, fade(engine.stateTime, 19.25f, 19.75f, 23.0f, 24.5f));
        PopFont();
    }
}

void GUI::Credits()
{
    if (interval(0.0f, 152.0f).contains(engine.stateTime))
    {
        const uint32_t i = fadeIn(engine.stateTime, 0.0f, 152.001f) * credits.size();
        const Credit& credit = credits[i];
        const float timePerCredit = 152.0f / static_cast<float>(credits.size());
        const float start = static_cast<float>(i) * timePerCredit;
        const float end = start + timePerCredit;
        const float opacity = fade(engine.stateTime, start, start + 3.0f, end - 3.0f, end);
        const auto windowSize = GetWindowSize();

        PushFont(headingFont);
        Text(credit.heading, {0.5f, 0.8f - (64.0f / windowSize.y)}, opacity);
        PopFont();

        PushFont(textFont);
        Text(credit.text, {0.5f, 0.8f}, opacity);
        PopFont();

        PushFont(descriptionFont);
        Text(credit.description, {0.5f, 0.8f + (40.0f / windowSize.y)}, opacity);
        PopFont();
    }
}

void GUI::create()
{
    ImGui_ImplGlfw_NewFrame();
    NewFrame();

    SetNextWindowPos({0.0f, 0.0f});
    SetNextWindowSize(IO().DisplaySize);
    Begin("Demo", {}, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);
    switch (engine.state)
    {
    case Engine::State::Intro:
        Intro();
        break;
    case Engine::State::Finale:
        Finale();
        break;
    case Engine::State::Credits:
        Credits();
        break;
    default:
        break;
    }
    End();

    Render();
}

ImDrawData* GUI::drawData()
{
    return GetDrawData();
}

GUI::~GUI()
{
    ImGuiIO& io = IO();
    io.BackendRendererName = {};
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui_ImplGlfw_Shutdown();
    DestroyContext();
}