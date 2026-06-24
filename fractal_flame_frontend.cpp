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

struct Preset;

struct ColorState {
    int numColors = 128; // const
    std::unique_ptr<Color[]> palette;
    std::vector<Color> funcColors;
    std::vector<float> floats; // used for ColorEdit3
    

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

        floats.erase(floats.begin() + 3*index, floats.begin() + 3*index+3);
    }

    void resizeVectors(int numTransforms) {
        funcColors.resize(numTransforms);
        floats.resize(numTransforms*3);
    }

    void clearPalette() {
        for(int i = 0; i < numColors; i++) {
            palette[i] = Color(); // set entire array to black
        }
    }

    void forceUIColors() {
        
    }

    void randomizeColors(int numTransforms, int seed) {
        std::uniform_real_distribution<double> satDist(0.7f, 1.0f);
		std::uniform_real_distribution<double> valDist(0.7f, 1.0f);
		std::mt19937 rng(seed); // pseudorandom -- good enough
		funcColors.resize(numTransforms);
		for(int i = 0; i < numTransforms; i++) {
			double hue = (double)i / numTransforms; // evenly spaced hues [0, 1]
			double saturation = satDist(rng);
			double value = valDist(rng);
			funcColors[i] = Color::hsvToRGB(hue, saturation, value);
            printf("funcColors[i] = {%u, %u, %u}\r\n", funcColors[i].r, funcColors[i].g, funcColors[i].b);
		}
		buildPalette();
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
            size_t segment = static_cast<size_t>(t / segmentSize);
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
        
        floats.resize(funcColors.size()*3);
        for(size_t i = 0; i < funcColors.size(); i++) {
            floats[3*i+0] = funcColors[i].r / 255.0; 
            floats[3*i+1] = funcColors[i].g / 255.0;
            floats[3*i+2] = funcColors[i].b / 255.0;
        }
        printf("Floats changed!\r\n");
    }

    const Color* getPalettePtr() {
        return palette.get();
    }

    ~ColorState() {
        clearPalette();
    }
    
};


struct UIState {
    int selectedBackend = 1; // defaults to Serial when opening window for now
    float settingsPanelAlpha = 0.97f;
    float settingsPanelWidth = 0.20f; // 20% of screen width
    uint16_t window_width = 1280, window_height = 720;
    ColorState color;

    int threadCount = 1;
    std::vector<std::string> supportedVariations;

    void applyPreset(std::vector<Color> newColors) {
        color.funcColors = newColors;
        color.buildPalette();
    }

    void resize(int newSize) {
        color.resizeVectors(newSize);   
    }

    void renderSettingsButton(std::unique_ptr<Engine> &fractal_engine, bool &settingsOpen);

    void renderPlayPaused(std::unique_ptr<Engine> &fractal_engine);

    void renderRandomizeButton(std::unique_ptr<Engine> &fractal_engine);

    bool renderUITab(std::unique_ptr<Engine> &fractal_engine, GLuint &flameTexture);

    bool renderTransformTab(std::unique_ptr<Engine> &fractal_engine, UIState &ui);

    void renderPresetsTab(std::unique_ptr<Engine> &fractal_engine, UIState &ui, std::vector<Preset> &presets);
    
    void renderRandomTab(std::unique_ptr<Engine> &fractal_engine);
    
};  

std::unique_ptr<Engine> selectBackend(int selection, int /*threadCount*/) {
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
    std::string displayName = "None"; // name for the preset - displayed in ImGui
    EngineState engineState;
    std::vector<Color> funcColors;

    void applyPreset(std::unique_ptr<Engine>& fractal_engine, UIState& ui) {
        ui.applyPreset(funcColors);
        fractal_engine->applyPreset(engineState);
    }
};

const Preset DragonCurve = {
    "Dragon Curve",
    {
    0,                  // seed
    {-10, 10, -10, 10}, // viewport
    { 
        Transform{4, 0, VariationDef(), Affine(0.824074, 0.281428, -1.88229, -0.212346, 0.864198, -0.110607)}, 
        Transform{1, 1, VariationDef(), Affine(0.088272, 0.520988, 0.785360, -0.463889, -0.377778, 8.095795)}
    },
    false, // hasFinalTransform
    Transform()
    },
    {
        {255, 0, 0}, {0, 0, 255} // two colors, one for each Transform above
    }
};

std::vector<Preset> presets = {DragonCurve}; // https://paulbourke.net/fractals/ifs/ - copied these directly, just so I have a few options for displaying 

int main() { 
    UIState ui;
    
    constexpr bool OpenGLDebug = true;


    if (!glfwInit()) {
        printf("glfwInit failed. Exiting...\r\n");
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
    
    if constexpr (OpenGLDebug) {
        printf("Window created.\r\n");
    }


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

    if constexpr (OpenGLDebug) {
        printf("Texture bound.\r\n");
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // ImFont* font = io.Fonts->AddFontFromFileTTF("file.ttf", 14.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    std::unique_ptr<Engine> fractal_engine = createSerialEngine();

    const ImGuiWindowFlags flame_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize 
        |   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs
        |   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    
    const ImGuiWindowFlags button_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoNav;

    const ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    
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
            ImGui::SetNextWindowPos({10, display.y - 80}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            
            ImGui::Begin("##randomizeButton", nullptr, button_flags);
            ui.renderRandomizeButton(fractal_engine);
            ImGui::End();


            ImGui::SetNextWindowPos({10, display.y - 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            
            ImGui::Begin("##settingsButton", nullptr, button_flags);
            ui.renderSettingsButton(fractal_engine, settingsOpen);
            ImGui::End();


            ImGui::SetNextWindowPos({60, display.y - 40}, ImGuiCond_Always); // play/pause positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            
            ImGui::Begin("##playPause", nullptr, button_flags);
            ui.renderPlayPaused(fractal_engine);
            ImGui::End();

        } else { // if settings is open:
            ImGui::SetNextWindowPos({10, 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({500, display.y - 80}, ImGuiCond_Always);
            ImGui::Begin("Settings", &settingsOpen, settings_flags); // Contents of the Settings menu:
            
            const char* backends[] = {"None","Serial", "OpenMP"
            #ifdef HAS_CUDA
                , "CUDA"
            #endif 
            }; // this is hilarious and also accomplishes exactly what I want 

            static int selectedBackend = 1;

            ImGui::PushFont(NULL, 18.0f);
            
            ImGui::Text("Backend:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100); // 100 px
            if(ImGui::Combo("##backendSelector", &selectedBackend, backends, IM_ARRAYSIZE(backends))) {
                fractal_engine = selectBackend(selectedBackend, ui.threadCount);
                if(selectedBackend == 0) { // None
                    ui.supportedVariations = {};
                } else {
                    std::vector<VariationDef> vars = fractal_engine->getSupportedVariations();
                    ui.supportedVariations.resize(vars.size());
                    for(size_t i = 0; i < vars.size(); i++) {
                        ui.supportedVariations[i] = vars[i].name; 
                    }
                }
            }

            ImGui::PopFont();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_TabBarBorderSize, 0.0f);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f); // move blue tab line down by 12 pixels
            if(ImGui::BeginTabBar("##settingsTabs", ImGuiTabBarFlags_None)) {
                ImGui::PushFont(NULL, 20.0f); 
                ui.renderTransformTab(fractal_engine, ui);
                
                ui.renderUITab(fractal_engine, flameTexture);
                
                if(fractal_engine && !(fractal_engine->getStatus())) {
                    ui.renderPresetsTab(fractal_engine, ui, presets);
                    ui.renderRandomTab(fractal_engine);
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
    if(fractal_engine) {
        fractal_engine->stop();
        fractal_engine.reset(); // unique_ptr method -- deletes the object, which SHOULD free everything...
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    printf("Exiting gracefully...\r\n");
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
        // printf("Histogram uploaded!\r\n");
    }
}


/////////////////////////
//////// UI TABS ////////
/////////////////////////


void UIState::renderSettingsButton(std::unique_ptr<Engine> &fractal_engine, bool &settingsOpen) {
    if(fractal_engine && fractal_engine->getStatus()) {
        ImGui::BeginDisabled();
    }

    if(ImGui::Button("[S]")) { 
        settingsOpen = !settingsOpen;
    }

    if(fractal_engine && fractal_engine->getStatus()) {
        ImGui::EndDisabled();
    }
}

void UIState::renderPlayPaused(std::unique_ptr<Engine> &fractal_engine) {
    if(fractal_engine) {
        if(fractal_engine->getStatus()) {
            if(ImGui::Button("[||]")) { // if it's running, show pause button
                fractal_engine->stop();
            }
        } else { // if it's not running, show play button
            if(ImGui::Button("[>]")) { 
                printf("Attempting to start...");
                fractal_engine->start();
                if(!fractal_engine->getStatus()) {
                    printf("Engine failed to start.\r\n");
                } else {
                    printf("Engine started.\r\n");
                }
            }
        }
    }

}

void UIState::renderRandomizeButton(std::unique_ptr<Engine> &fractal_engine) {
    if(fractal_engine && fractal_engine->getStatus()) {
        ImGui::BeginDisabled();
    }

    if(ImGui::Button("[R]")) {
        int colorSeed = fractal_engine->randomize();
        color.randomizeColors(fractal_engine->getTransforms().size(), colorSeed);
    }

    if(fractal_engine && fractal_engine->getStatus()) {
        ImGui::EndDisabled();
    }
}

bool UIState::renderUITab(std::unique_ptr<Engine> &fractal_engine, GLuint &flameTexture) {
    if(ImGui::BeginTabItem("UI")) {
        // histogram width, height
        // viewport width, height
        
        ImGui::Text("Seed");
        ImGui::SameLine();
        static int seed = 0;
        if(ImGui::InputInt("##seed", &seed, 1, 10000)) {
            printf("Seed changing not implemented yet\r\n");
        }

        static float viewport[4] = {-1.0, 1.0, -1.0, 1.0};
        
        if(!fractal_engine || (fractal_engine && fractal_engine->getStatus())) {
            ImGui::BeginDisabled();
        }

        ImGui::Text("minX, maxX, minY, maxY");
        if(ImGui::DragFloat4("##viewport", viewport)) {
            fractal_engine->setViewport(Viewport(viewport));
        }   

        ImGui::SameLine();
        

        if(ImGui::Button("Auto Viewport")) {
            fractal_engine->calculateViewport(); // fractal_engine is guaranteed to exist; it would be disabled otherwise
            Viewport v = fractal_engine->getViewport();
            viewport[0] = static_cast<float>(v.minX);
            viewport[1] = static_cast<float>(v.maxX);
            viewport[2] = static_cast<float>(v.minY);
            viewport[3] = static_cast<float>(v.maxY);
        }

        if(fractal_engine && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {        // i love ImGui
            ImGui::SetTooltip("Engine must be paused to change the viewport!");  // they added ImGuiHoveredFlags_AllowWhenDisabled for this exact use case:
        }                                                                       // showing why something is disabled

        if(!fractal_engine || (fractal_engine && fractal_engine->getStatus())) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();
        ImGui::Text("Note: RAM use is W*H*(64+32)/8 bytes");
        
        
        if(fractal_engine) {
            bool histDimChanged = false;
            const Histogram<PixelData>* readOnlyHistogram = fractal_engine->getHistogram(); 
            int histWidth = readOnlyHistogram->getWidth();
            int histHeight = readOnlyHistogram->getHeight();
            ImGui::Text("Current RAM use: %.2f GB", ((double)histWidth*histHeight*(64 + 32))/ (double)8E9);
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
        return true;
    } else {
        return false;
    }
}

bool UIState::renderTransformTab(std::unique_ptr<Engine> &fractal_engine, UIState &ui) {
    if(ImGui::BeginTabItem("Transforms")) {
        if(!fractal_engine) {
            ImGui::BeginDisabled();
        }                     
        
        if(ImGui::Button("+ Add Transform")) {
            Transform newTransform;
            fractal_engine->addTransform(newTransform);
            color.add(Color(0, 255, 0));
        }

        if(!fractal_engine) {
            ImGui::EndDisabled();
        } 

        std::vector<Transform> dupe;
        
        int numTransforms = 0;
        if(fractal_engine) {
            numTransforms = fractal_engine->getTransforms().size();
            dupe = fractal_engine->getTransforms();

            color.resizeVectors(numTransforms);
        }
        
        for(int i = 0; i < numTransforms; i++) { // 0 if engine is not initialized -- engine is safe to call in loop also
            ImGui::PushID(i);
            int variationIndex = dupe[i].variation.index;

            std::vector<const char*> supportedVariations;
            for(size_t i = 0; i < ui.supportedVariations.size(); i++) {
                supportedVariations.push_back(ui.supportedVariations[i].c_str()); // rebuild every frame -- supportedVariations leaves scope every frame, but it can't be static because ui could change 
            } // this is an ImGui limitation tbh

            if(ImGui::Combo("##transformSelector", &variationIndex, supportedVariations.data(), supportedVariations.size())) {
                dupe[i].variation.index = variationIndex; // change the duped index
                fractal_engine->setTransform(i, dupe[i]);
            }
            ImGui::SameLine();

            std::vector<float> transformColors(numTransforms);
            
            transformColors = color.floats; // Apparently this does a deep copy! Neat!

            if(ImGui::ColorEdit3("##transformColor", transformColors.data() + i*3, ImGuiColorEditFlags_NoInputs)) {
                printf("Color changed! ");
                color.funcColors[i] = Color(transformColors.data() + i*3);
                color.buildPalette();
            }

            ImGui::Text("Weight:");
            ImGui::SameLine();
            if(ImGui::DragFloat("##weight", &dupe[i].weight, 0.1f, 0.0f, 10.0f, "%.1f")) {
                printf("Weight of formula %d changed to %.1f.\r\n", i, dupe[i].weight);
                fractal_engine->setTransform(i, dupe[i]);
            }
            ImGui::SameLine();
            if(ImGui::Button("X")) {
                fractal_engine->removeTransform(i);
                color.removeFunc(i);
            }

            std::vector<double*> affineCoeffs = dupe[i].coeffs.toPtrVector();
            if(ImGui::CollapsingHeader("Affine")) {
                bool affineChanged = false;
                affineChanged |= ImGui::InputDouble("a", affineCoeffs[0], 0.01, 0.1, "%.3f");
                affineChanged |= ImGui::InputDouble("b", affineCoeffs[1], 0.01, 0.1, "%.3f");
                affineChanged |= ImGui::InputDouble("c", affineCoeffs[2], 0.01, 0.1, "%.3f");
                affineChanged |= ImGui::InputDouble("d", affineCoeffs[3], 0.01, 0.1, "%.3f");
                affineChanged |= ImGui::InputDouble("e", affineCoeffs[4], 0.01, 0.1, "%.3f");
                affineChanged |= ImGui::InputDouble("f", affineCoeffs[5], 0.01, 0.1, "%.3f");

                if(affineChanged) {
                    dupe[i].coeffs.a = *affineCoeffs[0];
                    dupe[i].coeffs.b = *affineCoeffs[1];
                    dupe[i].coeffs.c = *affineCoeffs[2];
                    dupe[i].coeffs.d = *affineCoeffs[3];
                    dupe[i].coeffs.e = *affineCoeffs[4];
                    dupe[i].coeffs.f = *affineCoeffs[5];
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
        return true;
    } else {
        return false;
    }
}

void UIState::renderPresetsTab(std::unique_ptr<Engine> &fractal_engine, UIState &ui, std::vector<Preset> &presets) {
    if(ImGui::BeginTabItem("Presets")) {
        size_t numPresets = presets.size();

        static std::vector<const char*> presetTitles = {" "};
    
        for(size_t i = 0; i < numPresets; i++) {
            presetTitles.push_back(presets[i].displayName.c_str());
        }

        static int chosenPreset = 0;
        if(ImGui::Combo("##presetSelector", &chosenPreset, presetTitles.data(), numPresets + 1)) { // +1 for " "
            presets.at(chosenPreset - 1).applyPreset(fractal_engine, ui);
        }
        ImGui::EndTabItem();
    }; 
    
}

void UIState::renderRandomTab(std::unique_ptr<Engine> &fractal_engine) {
    if(ImGui::BeginTabItem("Randomize")) {
        if(ImGui::Button("Randomize!")) {
            int colorSeed = fractal_engine->randomize();
            color.randomizeColors(fractal_engine->getTransforms().size(), colorSeed);
        }
        
        static int userSeed = 0; // get the user's seed
        ImGui::Text("Seed");
        ImGui::SameLine();
        ImGui::InputInt("##userSeedPicker", &userSeed, 0, 0);
        ImGui::SameLine();
        if(ImGui::Button("Use Seed")) {
            fractal_engine->randomize(userSeed);
        }
    ImGui::EndTabItem();
    }
}       



