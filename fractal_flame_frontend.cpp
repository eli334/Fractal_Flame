#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "./engines/engine.h" // Inherit the abstract class 

// Fractal Flame frontend
// GUI to pick the mode -- see engines directory (./engines/) for the actual code to generate flames

struct ColorState {
    int numColors = 2048; // const
    std::unique_ptr<Color[]> palette;
    std::vector<Color> funcColors;

    ColorState() {
        palette = std::make_unique<Color[]>(numColors);
    }

    void add(Color color) { // appends for now
        funcColors.push_back(color);
        buildPalette();
    }

    void removeFunc(int index) {
        funcColors.erase(funcColors.begin() + index);
        buildPalette();
    }

    void clearPalette() {
        for(int i = 0; i < numColors; i++) {
            palette[i] = Color(); // set entire array to black
        }
    }

    void buildPalette() {
        clearPalette();
        buildPalette(numColors);
    }

    void buildPalette(int numColors) {
        palette = std::make_unique<Color[]>(numColors);
        printf("There are %ld functions.\r\n", funcColors.size());
        std::vector<Color> lerpColors;
        
        if(funcColors.size() == 0) {
            lerpColors.push_back(Color());
            lerpColors.push_back(Color(255, 255, 255));
        } else if(funcColors.size() == 1) {
            lerpColors.push_back(Color());
            lerpColors.push_back(funcColors[0]);
        } else { // size 2 or greater
            lerpColors = funcColors;
        }

        for(int i = 0; i < numColors; i++) {
            double t = (double)i / (numColors - 1); // t is the lerp amount
            double segmentSize = 1.0 / (lerpColors.size() - 1); // size - 1 -> # of splits, all same size
            int segment = (int)(t / segmentSize);
            if(segment > lerpColors.size() - 2) {
                segment = lerpColors.size() - 2;
            }

            double localT = (t - segment * segmentSize) / segmentSize; // now lerp within the segment
            Color& a = lerpColors[segment]; // color to lerp from
            Color& b = lerpColors[segment + 1]; // color to lerp to

            palette[i].r = static_cast<uint8_t>(a.r + localT * (b.r - a.r));
            palette[i].g = static_cast<uint8_t>(a.g + localT * (b.g - a.g));
            palette[i].b = static_cast<uint8_t>(a.b + localT * (b.b - a.b));
        }

        printf("Palette made!\r\n");
    }

    const Color* getPalettePtr() {
        return palette.get();
    }

    ~ColorState() {
        clearPalette();
    }
    
};


struct UIState {
    int selectedBackend = 0; // defaults to None when opening window for now
    float settingsPanelAlpha = 0.97f;
    float settingsPanelWidth = 0.20f; // 20% of screen width
    uint16_t window_width = 1280, window_height = 720;
    ColorState color;

    int threadCount = 1;
    std::vector<const char*> supportedVariations;
};  

std::unique_ptr<Engine> selectBackend(int selection, int threadCount) {
    switch(selection) {
        case 1: // Serial
            return std::unique_ptr<Engine>(createSerialEngine());
            break;
        case 2: // OpenMP
            //return std::unique_ptr<Engine>(createOpenMPEngine(config, threadCount));
            return nullptr;
            break;
        //case 3: // CUDA
        default: // None
            return nullptr;

    }
}
GLuint flameTexture = 0; // global Texture id

void uploadHistogram(std::unique_ptr<Engine>& fractal_engine, const Color* palette, int paletteSize);

struct Preset {
    const char* displayName; // name for the preset
    EngineState engineState;
    std::vector<Color> funcColors;
};

const Preset DragonCurve = {
    "Dragon Curve",
    {
    0, 
    {-10, 10, -10, 10},
    { 
        Transform{4, 0, VariationDef(), Affine(0.824074, 0.281428, -1.88229, -0.212346, 0.864198, -0.110607)}, 
        Transform{1, 1, VariationDef(), Affine(0.088272, 0.520988, 0.785360, -0.463889, -0.377778, 8.095795)}
    }
    },
    {
        {255, 0, 0}, {255, 50, 0} // two colors, one for each Transform above
    }
};

const Preset presets[] = 
{ // https://paulbourke.net/fractals/ifs/ - copied these directly, just so I have a few options for displaying 
    DragonCurve
};

int main(int argc, char **argv ) { 
    UIState ui;
    

    if (!glfwInit()) {
        return -1;
    }
    // Set OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(ui.window_width, ui.window_height, "Fractal Flame Engine", NULL, NULL);
    
    if (!window) { glfwTerminate(); printf("glfwTerminate called. \r\n"); return -1; }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    

    glGenTextures(1, &flameTexture);
    glBindTexture(GL_TEXTURE_2D, flameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // mipmap stuff -- idrc
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexParameter.xhtml
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                1000, 1000,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // ImFont* font = io.Fonts->AddFontFromFileTTF("file.ttf", 14.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    std::unique_ptr<Engine> fractal_engine;

    const ImGuiWindowFlags flame_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize 
        |   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs
        |   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    
    const ImGuiWindowFlags button_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoNav;

    const ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    
    const ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_NoTabListScrollingButtons;

    bool settingsOpen = false;
    static bool wasSettingsOpen = false;


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame(); // {Link: Recited in GH #9112 https://github.com/ocornut/imgui/issues/9112}
        ImGui::NewFrame();

        ////                    ////
        ////    FRONTEND CODE   ////
        ////                    ////
        
        ImVec2 display = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(display);
        ImGui::SetNextWindowBgAlpha(0.0f);
        
        uploadHistogram(fractal_engine, ui.color.getPalettePtr(), ui.color.numColors); // update flameTexture

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("##flameview", nullptr, flame_flags);
        ImGui::Image((ImTextureID)(intptr_t)flameTexture, display);
        ImGui::End();
        ImGui::PopStyleVar();

        if(!settingsOpen) {
            
            ImGui::SetNextWindowPos({10, display.y - 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            ImGui::Begin("##gearButton", nullptr, button_flags);
            
            if(fractal_engine && fractal_engine->getStatus()) {
                ImGui::BeginDisabled();
            }

            if(ImGui::Button("[S]")) { 
                settingsOpen = !settingsOpen;
            }

            if(fractal_engine && fractal_engine->getStatus()) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            ImGui::End();
            
            ImGui::SetNextWindowPos({60, display.y - 40}, ImGuiCond_Always); // play/pause positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            ImGui::Begin("##playPause", nullptr, button_flags);
            if(fractal_engine) {
                if(fractal_engine->getStatus()) {
                    if(ImGui::Button("[||]")) { // if it's running, show pause button
                        fractal_engine->stop();
                    }
                } else { // if it's not running, show play button
                    if(ImGui::Button("[>]")) { 
                        fractal_engine->start();
                    }
                }
            }
            ImGui::End();
        } else {
            ImGui::SetNextWindowPos({10, 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({500, display.y - 80}, ImGuiCond_Always);
            ImGui::Begin("Settings", &settingsOpen, settings_flags); // Contents of the Settings menu:
            
            const char* backends[] = {"None","Serial", "OpenMP"
            #ifdef HAS_CUDA
                , "CUDA"
            #endif 
            }; // this is hilarious and also accomplishes exactly what I want 

            static int selectedBackend = 0;

            ImGui::PushFont(NULL, 18.0f);
            ImGui::Text("Backend:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1); // fill available width
            if(ImGui::Combo("##backendSelector", &selectedBackend, backends, IM_ARRAYSIZE(backends))) {
                fractal_engine = selectBackend(selectedBackend, ui.threadCount);
                if(selectedBackend == 0) { // None
                    ui.supportedVariations = {};
                } else {
                    std::vector<VariationDef> vars = fractal_engine->getSupportedVariations();
                    for(VariationDef variation : vars) {
                        ui.supportedVariations.push_back(variation.name);
                    }
                }
            }

            ImGui::PopFont();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_TabBarBorderSize, 0.0f);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f); // move blue tab line down by 12 pixels
            if(ImGui::BeginTabBar("##settingsTabs", ImGuiTabBarFlags_None)) {
                ImGui::PushFont(NULL, 20.0f); 
                if(ImGui::BeginTabItem("Transforms")) {
                    
                    if(!fractal_engine) {
                        ImGui::BeginDisabled();
                    }                     
                    
                    if(ImGui::Button("+ Add Transform")) {
                        Transform newTransform = {1.0, 0}; // New 
                        fractal_engine->addTransform(newTransform);
                        ui.color.add(Color(0, 255, 0));
                    }

                    if(!fractal_engine) {
                        ImGui::EndDisabled();
                    } 

                    std::vector<Transform> dupe;
                    
                    int numTransforms = 0;
                    if(fractal_engine) {
                        numTransforms = fractal_engine->getTransforms().size();
                        dupe = fractal_engine->getTransforms();
                    }
                    for(int i = 0; i < numTransforms; i++) { // 0 if engine is not initialized -- engine is safe to call in loop also
                        ImGui::PushID(i);
                        int variationIndex = dupe[i].variation.index;

                        if(ImGui::Combo("##transformSelector", &variationIndex, ui.supportedVariations.data(), ui.supportedVariations.size())) {
                            dupe[i].variation.index = variationIndex; // change the duped index
                            fractal_engine->setTransform(i, dupe[i]);
                        }
                        ImGui::SameLine();

                        
                        static std::vector<float> transformColors;
                        transformColors.resize(numTransforms * 3, 1);
                        if(ImGui::ColorEdit3("##transformColor", &transformColors[i*3], ImGuiColorEditFlags_NoInputs)) {
                            printf("Color changed! ");
                            ui.color.funcColors[i] = Color(&transformColors[i*3]);
                            printf("New color; transform %d is %u, %u, %u\r\n", i, ui.color.funcColors[i][0], ui.color.funcColors[i][1], ui.color.funcColors[i][2]);
                        }

                        ImGui::Text("Weight:");
                        ImGui::SameLine();
                        if(ImGui::DragFloat("##weight", &dupe[i].weight, 0.1f, 0.0f, 10.0f, "%.1f")) {
                            printf("Weight of formula %d changed to %.1f.\r\n", i, dupe[i].weight);
                            fractal_engine->setTransform(i, dupe[i]);
                        }
                        ImGui::SameLine();
                        if(ImGui::Button("X")) {
                            dupe.erase(dupe.begin() + i);
                        }

                        static std::vector<double> affineCoeffs;
                        affineCoeffs.resize(dupe.size() * 6);
                        if(ImGui::CollapsingHeader("Affine")) {
                            bool affineChanged = false;
                            affineChanged |= ImGui::InputDouble("a", &affineCoeffs[i*6+0], 0.01, 0.1, "%.3f");
                            affineChanged |= ImGui::InputDouble("b", &affineCoeffs[i*6+1], 0.01, 0.1, "%.3f");
                            affineChanged |= ImGui::InputDouble("c", &affineCoeffs[i*6+2], 0.01, 0.1, "%.3f");
                            affineChanged |= ImGui::InputDouble("d", &affineCoeffs[i*6+3], 0.01, 0.1, "%.3f");
                            affineChanged |= ImGui::InputDouble("e", &affineCoeffs[i*6+4], 0.01, 0.1, "%.3f");
                            affineChanged |= ImGui::InputDouble("f", &affineCoeffs[i*6+5], 0.01, 0.1, "%.3f");

                            if(affineChanged) {
                                dupe[i].coeffs.a = affineCoeffs[i*6+0];
                                dupe[i].coeffs.b = affineCoeffs[i*6+1];
                                dupe[i].coeffs.c = affineCoeffs[i*6+2];
                                dupe[i].coeffs.d = affineCoeffs[i*6+3];
                                dupe[i].coeffs.e = affineCoeffs[i*6+4];
                                dupe[i].coeffs.f = affineCoeffs[i*6+5];
                                fractal_engine->setTransform(i, dupe[i]);
                            }
                        }

                        ImGui::PopID();
                    }
                    ImGui::Separator();
                    // static bool hasFinalTransform = false;
                    // ImGui::Checkbox("Final transform", &hasFinalTransform);
                    // if(hasFinalTransform) {;
                    //     ImGui::Combo("##finalTransformSelector", &fractalConf.finalTransform.variationIndex, UIConf.supportedVariations.data(), (int)UIConf.supportedVariations.size());
                    //     ImGui::Text("Weight:");
                    //     ImGui::SameLine();
                    //     ImGui::DragFloat("##weight", &fractalConf.finalTransform.weight, 0.1f, 0.0f, 10.0f, "%.1f");
                    //     ImGui::SameLine();
                    // }
                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("UI")) {
                    // histogram width, height
                    // viewport width, height
                    
                    ImGui::Text("Seed");
                    ImGui::SameLine();
                    static int seed = 0;
                    ImGui::InputInt("##seed", &seed, 1, 10000);

                    static float viewport[4] = {-1.0, 1.0, -1.0, 1.0};
                    
                    if(!fractal_engine || (fractal_engine && !fractal_engine->getStatus())) {
                        ImGui::BeginDisabled();
                    }

                    ImGui::Text("minX, maxX, minY, maxY");
                    if(ImGui::DragFloat4("##viewport", viewport)) {
                        fractal_engine->setViewport(Viewport(viewport));
                    };

                    ImGui::SameLine();
                    

                    if(ImGui::Button("Auto Viewport")) {
                        fractal_engine->calculateViewport(); // fractal_engine is guaranteed to exist; it would be disabled otherwise
                    }

                    if(fractal_engine && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {        // i love ImGui
                       ImGui::SetTooltip("Engine must be paused to change the viewport!");  // they added ImGuiHoveredFlags_AllowWhenDisabled for this exact use case:
                    }                                                                       // showing why something is disabled

                    if(!fractal_engine || (fractal_engine && !fractal_engine->getStatus())) {
                        ImGui::EndDisabled();
                    }

                    ImGui::Separator();
                    ImGui::Text("Note: RAM use is W*H*(64+32)/8 bytes");
                    
                    
                    if(fractal_engine) {
                        bool histDimChanged = false;
                        const Histogram<PixelData>* readOnlyHistogram = fractal_engine->getHistogram(); 
                        int histWidth = fractal_engine->getHistogram()->getWidth();
                        int histHeight = fractal_engine->getHistogram()->getHeight();
                        ImGui::Text("Current RAM use: %.2f GB", (histWidth*histHeight*(64 + 32))/ (double)8E9);
                        ImGui::Text("Histogram Width");
                        ImGui::SameLine();
                        histDimChanged |= ImGui::InputInt("##histWidth", &histWidth, 1, 100);
                        ImGui::Text("Histogram Height");
                        ImGui::SameLine();
                        histDimChanged |= ImGui::InputInt("##histHeight", &histHeight, 1, 100);
                     
                        if(histDimChanged && (histWidth > 0 && histHeight > 0)) { // width and height are > 0; if == 0, then no malloc (new) happens
                            fractal_engine->resize(histWidth, histHeight);
                        }
                        glBindTexture(GL_TEXTURE_2D, flameTexture);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, histWidth, histHeight, // resize texture to new histogram size
                                    0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }
                    ImGui::EndTabItem();
                }
            if(fractal_engine && !(fractal_engine->getStatus()) && ImGui::BeginTabItem("Presets")) {
                int numPresets = sizeof(presets) / sizeof(presets[0]);

                std::vector<const char*> presetVector = {" "}; // empty
                for(int i = 0; i < numPresets; i++) {
                    presetVector.push_back(presets[i].displayName);
                }

                static int chosenPreset = 0;
                if(ImGui::Combo("##presetSelector", &chosenPreset, presetVector.data(), numPresets + 1)) { // +1 for " "
                    //loadPreset(chosenPreset, fractal_engine);    
                }
                ImGui::EndTabItem();
            }

            ImGui::PopFont();    
            ImGui::EndTabBar();
            }

        ImGui::PopStyleVar(2);      
        ImGui::End();
        }
        
        if(wasSettingsOpen && !settingsOpen) { // if it was open and is no longer
            // if(fractal_engine) {
            //     fractal_engine->setTransforms(fractalConf.transforms);
            // } // shouldn't be needed anymore
        }            
        wasSettingsOpen = settingsOpen;

        // Render
        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT); // stops Garry's Mod clipping out of bounds effect
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    printf("Exiting gracefully...");
    return 0;
}

void uploadHistogram(std::unique_ptr<Engine>& fractal_engine, const Color* palette, int paletteSize) { // function to push the global_histogram to opengl
    if(!fractal_engine) return;
    static std::vector<uint8_t> pixels;
    int width = fractal_engine->getHistogram()->getWidth();
    int height = fractal_engine->getHistogram()->getHeight();
    pixels.resize(width * height * 4);
    if(fractal_engine->fillPixelBuffer(pixels.data(), palette, paletteSize)) { // if the pixel buffer was updated, reupload
        glBindTexture(GL_TEXTURE_2D, flameTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        printf("Histogram uploaded!\r\n");
    }
}

void applySettings() {

}