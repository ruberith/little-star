#pragma once

#include <imgui.h>
#include <string>
#include <vector>

class Engine;
struct ImageData;

// GUI system
class GUI
{
  private:
    // engine parent
    Engine& engine;
    // title font
    ImFont* titleFont{};
    // heading font
    ImFont* headingFont{};
    // text font
    ImFont* textFont{};
    // description font
    ImFont* descriptionFont{};
    // credit text
    struct Credit
    {
        std::string heading{};
        std::string text{};
        std::string description{};
    };
    // list of credits
    std::vector<Credit> credits{
        {"A demo by", "Robin Rademacher", "ruberith"},
        {"Soundtrack by", "Robin Rademacher", "ruberith"},
        {"Caveat font by", "Pablo Impallari", "Licensed under OFL 1.1"},
        {"Astronaut model by", "LasquetiSpice", "Licensed under CC BY 4.0"},
        {"Book Pattern texture by", "Rob Tuytel", "Licensed under CC0 1.0"},
        {"Brown Leather texture by", "Rob Tuytel", "Licensed under CC0 1.0"},
        {"Star Map texture by", "Ernie Wright, Ian Jones, Laurence Schuler",
         "NASA/Goddard Space Flight Center Scientific Visualization Studio"},
        {"Moon texture by", "Ernie Wright, Noah Petro", "NASA's Scientific Visualization Studio"},
    };

    // Create a text widget.
    void Text(const std::string& text, const ImVec2& position = {0.5f, 0.5f}, float opacity = 1.0f);
    // Create the intro GUI.
    void Intro();
    // Create the finale GUI.
    void Finale();
    // Create the credits GUI.
    void Credits();

  public:
    // Construct the GUI object given the engine.
    GUI(Engine& engine);
    // Destruct the GUI object.
    ~GUI();
    // Return the IO data structure.
    static ImGuiIO& IO();
    // Return the image data of the font texture.
    static ImageData fontTexture();
    // Create the GUI depending on the engine state.
    void create();
    // Return the draw data.
    static ImDrawData* drawData();
};