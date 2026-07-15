#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// #include "./engines/engine.h" // Inherit the abstract class 
// #include "./engines/serial_engine.cpp"
#include "./engines/openmp_engine.cpp"

#ifdef HAS_CUDA
#include "./engines/cuda_engine.cuh"
#endif

// Fractal Flame frontend
// GUI to pick the mode -- see engines directory (./engines/) for the actual code to generate flames

struct UIState {
    int selectedBackend = 2; // defaults to OpenMP when opening window for now
    float settingsPanelAlpha = 0.97f;
    float settingsPanelWidth = 0.20f; // 20% of screen width
    uint16_t window_width = 1280, window_height = 720;
    ColorState color;
    Camera view;

    int threadCount = 1;
    std::vector<std::string> supportedVariations;

    void applyPreset(std::vector<Color> newColors) {
        color.funcColors = newColors;
        color.buildPalette();
    }

    void resize(int newSize) {
        color.resizeVectors(newSize);   
    }

    void renderDebug(std::unique_ptr<Engine> &fractal_engine);
    bool resetIPS = false;
    bool debugMenu = false; // defaults to off -- prints affines / parametric coeffs

    void renderSettingsButton(std::unique_ptr<Engine> &fractal_engine, bool &settingsOpen);

    void renderQuickEdit(std::unique_ptr<Engine> &fractal_engine, UIState &ui, ImVec2 buttonPos);

    void renderPresetsWindow(std::unique_ptr<Engine> &fractal_engine, std::vector<Preset> &presets);

    void renderPlayPaused(std::unique_ptr<Engine> &fractal_engine);

    void renderRandomizeButton(std::unique_ptr<Engine> &fractal_engine, ImVec2 buttonPos);

    bool renderUITab(std::unique_ptr<Engine> &fractal_engine, GLuint &flameTexture);

    bool renderTransformTab(std::unique_ptr<Engine> &fractal_engine);
    
    void renderRandomTab(std::unique_ptr<Engine> &fractal_engine);

    // refactor this in the future
    const ImGuiWindowFlags button_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoNav;

    const ImGuiWindowFlags flame_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize 
    |   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    const ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    
    const ImGuiWindowFlags ips_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoNav;

    ImVec2 toFractal(ImVec2 screen, Viewport &vp, ImVec2 display) {
        double normalizedX = screen.x / display.x;
        double normalizedY = screen.y / display.y;
        double histoX = (normalizedX - 0.5) / view.zoom + view.centerX;
        double histoY = (normalizedY - 0.5) / view.zoom + view.centerY;
        double fractalX = histoX * (vp.maxX - vp.minX) + vp.minX;
        double fractalY = (1.0 - histoY) * (vp.maxY - vp.minY) + vp.minY;
        return {(float) fractalX, (float) fractalY};
    }

    void addDashedRect(ImVec2 topLeft, ImVec2 bottomRight, ImU32 color, float dashLength) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 corners[4] = {
            topLeft,
            {bottomRight.x, topLeft.y},
            bottomRight,
            {topLeft.x, bottomRight.y}
        };
        for(int i = 0; i < 4; i++) {
            ImVec2 start = corners[i];
            ImVec2 end = corners[(i + 1) % 4];
            float width = end.x - start.x;
            float height = end.y - start.y;
            float length = sqrtf(width*width + height*height);
            float unitX = width / length;
            float unitY = height / length;
            float pos = 0;
            bool draw = true;
            while(pos < length) {
                float segEnd = std::min(pos + dashLength, length);
                if(draw) {
                    drawList->AddLine(
                        {start.x + unitX*pos, start.y + unitY*pos},
                        {start.x + unitX*segEnd, start.y + unitY*segEnd},
                        color, 1.0f);
                }
                pos = segEnd;
                draw = !draw;
            }
        }
    }
};  

std::unique_ptr<Engine> selectBackend(int selection, int numThreads = std::max(1, omp_get_num_procs() - 8)) {
    switch(selection) {
        case 1: { // Serial
            return std::make_unique<Serial_Engine>();
        }
        case 2: { // OpenMP
            return std::make_unique<OpenMP_Engine>(numThreads);
        }
        #ifdef HAS_CUDA
        case 3: {
            std::unique_ptr<CUDA_Engine> engine = std::make_unique<CUDA_Engine>();
            // return engine;
        }   
        #endif
        //case 3: // CUDA
        default: // None
            return nullptr;

    } 
}
GLuint flameTexture = 0; // global Texture id

void uploadHistogram(std::unique_ptr<Engine>& fractal_engine, UIState &ui, double gamma);

struct Preset {
    std::string displayName = "None"; // name for the preset - displayed in ImGui
    EngineState engineState;
    std::vector<Color> funcColors;

    void applyPreset(std::unique_ptr<Engine>& fractal_engine, UIState& ui) {
        ui.applyPreset(funcColors);
        fractal_engine->applyPreset(engineState);
    }
};

// const Preset DragonCurve = {
//     "Dragon Curve",
//     {
//     0,                  // seed
//     {-10.0f, 10.0f, -10.0f, 10.0f}, // viewport
//     { 
//         Transform{4, 0, VariationDef(), Affine(0.824074, 0.281428, -1.88229, -0.212346, 0.864198, -0.110607), Parametric()}, 
//         Transform{1, 1, VariationDef(), Affine(0.088272, 0.520988, 0.785360, -0.463889, -0.377778, 8.095795), Parametric()}
//     },
//     false, // hasFinalTransform
//     Transform()
//     },
//     {
//         {255, 0, 0}, {0, 0, 255} // two colors, one for each Transform above
//     }
// };

std::vector<Preset> presets;// = {DragonCurve}; // https://paulbourke.net/fractals/ifs/ - copied these directly, just so I have a few options for displaying 

int main() { 
    UIState ui;
    
    constexpr bool OpenGLDebug = false;


    if (!glfwInit()) {
        printf("glfwInit failed. Exiting...\r\n");
        return -1;
    }
    // Set OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(ui.window_width, ui.window_height, "Fractal Flame Engine", NULL, NULL);
    glfwSetWindowPos(window, 100, 100);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if constexpr (OpenGLDebug) {
        printf("bits: \r\nred: %d, green: %d, blue: %d\r\n", mode->redBits, mode->greenBits, mode->blueBits);    
    }
    

    int centerX = (mode->width - ui.window_width) / 2;
    int centerY = (mode->height - ui.window_height) / 2;
    glfwSetWindowPos(window, centerX, centerY);
    
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
    std::vector<uint8_t> blackPixels(1000 * 1000 * 4, 0);
    for(size_t i = 3; i < blackPixels.size(); i += 4) {
        blackPixels[i] = 255;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                1000, 1000,
                0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixels.data());
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
    
    std::unique_ptr<Engine> fractal_engine = selectBackend(ui.selectedBackend);

    std::vector<VariationDef> vars = fractal_engine->getSupportedVariations();
    ui.supportedVariations.resize(vars.size());
    for(size_t i = 0; i < vars.size(); i++) {
        ui.supportedVariations[i] = vars[i].name; 
    }

    
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
        
        uploadHistogram(fractal_engine, ui, ui.color.gamma); // update flameTexture

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("##flameview", nullptr, ui.flame_flags);
        ImGui::Image((ImTextureID)(intptr_t)flameTexture, display);


        auto toScreen = [&](double fractalX, double fractalY, Viewport &vp) -> ImVec2 {
            // fractal coords -> normalized histogram coords [0,1]
            double histoX = (fractalX - vp.minX) / (vp.maxX - vp.minX);
            double histoY = 1.0 - (fractalY - vp.minY) / (vp.maxY - vp.minY); // Y flip
            
            // normalized histogram coords -> screen coords (inverse of fillPixelBuffer)
            double normalizedX = (histoX - ui.view.centerX) * ui.view.zoom + 0.5;
            double normalizedY = (histoY - ui.view.centerY) * ui.view.zoom + 0.5;
            
            return { (float)(normalizedX * display.x), (float)(normalizedY * display.y) };
        };


        if(fractal_engine) {
            Viewport vp = fractal_engine->getViewport();
            // printf("Viewport:\r\n%s", vp.toString());
            // printf("centerX: %.2f, centerY: %.2f, zoom: %.2f\r\n", ui.view.centerX, ui.view.centerY, ui.view.zoom);
            ImVec2 topLeft     = toScreen(vp.minX, vp.minY, vp);
            ImVec2 bottomRight = toScreen(vp.maxX, vp.maxY, vp);

            ImU32 borderColor;
            if(fractal_engine->done()) {
                borderColor = IM_COL32(0, 255, 0, 255);
            } else {
                borderColor = IM_COL32(255, 255, 255, 255);
            }

            ImGui::GetWindowDrawList()->AddRect(topLeft, bottomRight, borderColor, 0.0f, 0, 1.0f);
            // ImGui::GetWindowDrawList()->AddCircle(toScreen(0, 0, vp), 5.0f, IM_COL32(255, 0, 255, 255));
        }
        

        if(ImGui::IsItemHovered()) {
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 rightStartPos = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Right];
            ImVec2 currentPos = ImGui::GetIO().MousePos;

            rightStartPos.x -= windowPos.x;
            rightStartPos.y -= windowPos.y;
            currentPos.x -= windowPos.x;
            currentPos.y -= windowPos.y;
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                ui.view.centerX -= (delta.x / display.x) / ui.view.zoom;
                ui.view.centerY -= (delta.y / display.y) / ui.view.zoom;
            } else if(ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                ImGui::GetWindowDrawList()->AddRect(rightStartPos, currentPos, IM_COL32(255, 255, 0, 255), 0.0f, 0, 1.0f);
            }

            if(ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                if(ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] > 0) {
                    Viewport vp = fractal_engine->getViewport();
                    ImVec2 f1 = ui.toFractal(rightStartPos, vp, display);
                    ImVec2 f2 = ui.toFractal(currentPos, vp, display);
                    fractal_engine->setViewport(Viewport(f1.x, f1.y, f2.x, f2.y));
                    ui.view = Camera(); // reset camera
                    fractal_engine->start();
                }
            }        
        }
            

        float wheel = ImGui::GetIO().MouseWheel;
        if(wheel != 0) {
            ui.view.zoom *= (1.0 + wheel * 0.1);
        }
        
        ImGui::End();
        ImGui::PopStyleVar();

        if(!settingsOpen) {
            if(fractal_engine && fractal_engine->getStatus()) {
                ImGui::SetNextWindowPos({10, 20}, ImGuiCond_Always);
                ImGui::SetNextWindowSize({400, 150}, ImGuiCond_Always);
                ImGui::Begin("##ips", nullptr, ui.ips_flags);
                ui.renderDebug(fractal_engine);
                ImGui::End();
            }
            


            
            ui.renderRandomizeButton(fractal_engine, {10, display.y - 80});

            ui.renderQuickEdit(fractal_engine, ui, {60, display.y - 80});
            


            ImGui::SetNextWindowPos({10, display.y - 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            
            
            ui.renderSettingsButton(fractal_engine, settingsOpen);


            ImGui::SetNextWindowPos({60, display.y - 40}, ImGuiCond_Always); // play/pause positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            
            
            ui.renderPlayPaused(fractal_engine);
            

        } else { // if settings is open:
            const char* backends[] = {"None","Serial", "OpenMP"
            #ifdef HAS_CUDA
                , "CUDA"
            #endif 
            }; // this is hilarious and also accomplishes exactly what I want 

            static int selectedBackend = ui.selectedBackend;


            ImGui::SetNextWindowPos({10, 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({500, display.y - 80}, ImGuiCond_Always);
            
            ImGui::Begin("Settings", &settingsOpen, ui.settings_flags); // Contents of the Settings menu:
            ImGui::PushFont(NULL, 18.0f);
            
            ImGui::Text("Backend:");
            ImGui::SameLine();
            
            ImGui::SetNextItemWidth(100); // 100 px
            if(ImGui::Combo("##backendSelector", &selectedBackend, backends, IM_ARRAYSIZE(backends))) {
                fractal_engine = selectBackend(selectedBackend);
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
                ui.renderTransformTab(fractal_engine);
                
                ui.renderUITab(fractal_engine, flameTexture);
                
                if(fractal_engine && !(fractal_engine->getStatus())) {
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
    glfwTerminate(); // There are still 40 kb allocated in GLFW.  This is a memory leak that they won't fix because the OS reclaims it immediately anyway.  Cool!

    printf("Exiting gracefully...\r\n");
    return 0;
}

void uploadHistogram(std::unique_ptr<Engine> &fractal_engine, UIState &ui, double gamma) { // function to push the global_histogram to opengl
    if(!fractal_engine) return;
    static std::vector<uint8_t> pixels;
    int width = fractal_engine->getHistogram().getWidth();
    int height = fractal_engine->getHistogram().getHeight();
    pixels.resize(width * height * 4);
    if(fractal_engine->fillPixelBuffer(pixels.data(), ui.color.palette.get(), ui.color.numColors, gamma, ui.view)) { // if the pixel buffer was updated, reupload bool fillPixelBuffer(uint8_t* pixels, const Color* palette, int paletteSize, double gamma)
        // ImVec2 imageMin = ImGui::GetItemRectMin();
        // ImVec2 imageMax = ImGui::GetItemRectMax();
        // ImGui::GetWindowDrawList()->AddRect(imageMin, imageMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f); // 1 px white rectangle around image
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

void UIState::renderDebug(std::unique_ptr<Engine> &fractal_engine){
    if(!fractal_engine) return; // guard just in case

    const double trackingTime = 5.0; // 5 seconds - might be changable by user in future
    const double updateRate = 20.0;  // 20 Hz

    const double updatePeriod = (1.0 / updateRate);  

    static std::vector<std::pair<double, uint64_t>> buffer;

    if(buffer.empty()) {
        buffer.reserve(updateRate * trackingTime); // 5 seconds of data
    }

    if(resetIPS) {
        buffer.clear();
        resetIPS = false;
    }
    
    static double lastUpdate = 0.0;
    
    std::chrono::steady_clock::time_point nowPoint = std::chrono::steady_clock::now();
    double now = std::chrono::duration<double>(nowPoint.time_since_epoch()).count();

    uint64_t totalIterations = fractal_engine->getTotalIterations();

    while(buffer.size() > 1 && now - buffer.front().first > trackingTime) {
        buffer.erase(buffer.begin());
    }
    
    double timeSinceLast = now - lastUpdate;
    
    if(timeSinceLast > updatePeriod) {
        buffer.emplace_back(now, totalIterations);
        lastUpdate = now;
    }
    double iterPerSec; 
    if(buffer.size() >= 2) {
        iterPerSec = ((double) buffer.back().second - (double) buffer.front().second) / (buffer.back().first - buffer.front().first); 
    } else {
        iterPerSec = 0; // 0 for .1 sec.. oh well
    }

    std::vector<Transform> transforms = fractal_engine->getTransforms();
    
    ImGui::Text("%.3e iter/sec", iterPerSec);
    ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
    ImGui::Text("Total Iterations: %.2e", (double)totalIterations);
    ImGui::Text("Max Hits:         %.2e", (double)fractal_engine->getMaxHits());
    if(debugMenu) {
        ImGui::Text("Transforms:");
        for(size_t i = 0; i < transforms.size(); i++) {
            ImGui::Text("%d: %s", (int) i+1, transforms[i].toString(i));
        }
    }
}

void UIState::renderSettingsButton(std::unique_ptr<Engine> &fractal_engine, bool &settingsOpen) {
    ImGui::Begin("##settingsButton", nullptr, button_flags);
    if(fractal_engine && fractal_engine->getStatus()) {
        ImGui::BeginDisabled();
    }

    if(ImGui::Button("[S]")) { 
        settingsOpen = !settingsOpen;
    }

    if(fractal_engine && fractal_engine->getStatus()) {
        ImGui::EndDisabled();
    }
    ImGui::End();
}

void UIState::renderQuickEdit(std::unique_ptr<Engine> &fractal_engine, UIState &ui, ImVec2 buttonPos) {
    static bool quickEditOpen = false;
    
    if(!fractal_engine) return; // guard against fractal_engine not existing

    if(!quickEditOpen) {
        ImGui::SetNextWindowPos(buttonPos, ImGuiCond_Always); // quickEdit positioning
        ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
        ImGui::Begin("##QEButton", nullptr, button_flags);
        if(ImGui::Button("[Q]")) { 
            quickEditOpen = !quickEditOpen;
        }
        ImGui::End();
    } else  {
        ImGui::SetNextWindowPos({600, 80}, ImGuiCond_Appearing); // quickEdit positioning
        ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Appearing);
        ImGui::Begin("Quick Edit", &quickEditOpen, ImGuiWindowFlags_NoFocusOnAppearing);
        float gammaSlider = (float) color.gamma;
        if(ImGui::SliderFloat("Gamma", &gammaSlider, 0.1, 10, "%.1f")) {
            color.gamma = (double) gammaSlider;
        }

        double minPanX = 0.0, maxPanX = 1.0;
        double minPanY = 0.0, maxPanY = 1.0;
        double minZoom = 0.1, maxZoom = 10.0;
        ImGui::SliderScalar("CameraX", ImGuiDataType_Double, &ui.view.centerX, &minPanX, &maxPanX, "%.2f");

        ImGui::SliderScalar("CameraY", ImGuiDataType_Double, &ui.view.centerY, &minPanY, &maxPanY, "%.2f");
        
        ImGui::SliderScalar("Zoom", ImGuiDataType_Double, &ui.view.zoom, &minZoom, &maxZoom, "%.1f");

        if(fractal_engine->getMaxThreads() != 1) {
            ImGui::SliderInt("Thread Count", &ui.threadCount, 1, fractal_engine->getMaxThreads());
            
            // printf("getMaxThreads() = %d\r\n", fractal_engine->getMaxThreads());
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                printf("IsItemDeactivatedAfterEdit fired\r\n");
                fractal_engine->setThreads(ui.threadCount);
            }
        }


        

        ImGui::End();
    }
}

void UIState::renderPlayPaused(std::unique_ptr<Engine> &fractal_engine) {
    ImGui::Begin("##playPause", nullptr, button_flags);
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
    ImGui::End();
}

void UIState::renderRandomizeButton(std::unique_ptr<Engine> &fractal_engine, ImVec2 buttonPos) {
    // if(fractal_engine && fractal_engine->getStatus()) {
    //     ImGui::BeginDisabled();
    // }

    ImGui::SetNextWindowPos(buttonPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);

    ImGui::Begin("##randomizeButton", nullptr, button_flags);
    if(ImGui::Button("[R]")) {
        if(fractal_engine->getStatus()) fractal_engine->stop();
        int colorSeed = fractal_engine->randomize();
        color.randomizeColors(fractal_engine->getTransforms().size(), colorSeed);
        resetIPS = true;
        fractal_engine->start();
    }
    ImGui::End();

    // if(fractal_engine && fractal_engine->getStatus()) {
    //     ImGui::EndDisabled();
    // }
}

bool UIState::renderUITab(std::unique_ptr<Engine> &fractal_engine, GLuint &flameTexture) {
    if(ImGui::BeginTabItem("UI")) {
        // histogram width, height
        // viewport width, height

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
            const Histogram<PixelData>& readOnlyHistogram = fractal_engine->getHistogram();
            int histWidth = readOnlyHistogram.getWidth();
            int histHeight = readOnlyHistogram.getHeight();
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
        ImGui::Separator();
        ImGui::Checkbox("##debugMenuSet", &debugMenu);

        ImGui::EndTabItem();
        

        return true;
    } else {
        return false;
    }
}

bool UIState::renderTransformTab(std::unique_ptr<Engine> &fractal_engine) {
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

            std::vector<const char*> supportedVars;
            for(size_t i = 0; i < supportedVariations.size(); i++) {
                supportedVars.push_back(supportedVariations[i].c_str()); // rebuild every frame -- supportedVariations leaves scope every frame, but it can't be static because ui could change 
            } // this is an ImGui limitation tbh

            if(ImGui::Combo("##transformSelector", &variationIndex, supportedVars.data(), supportedVars.size())) {
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
            if(ImGui::DragScalar("##weight", ImGuiDataType_Double, &dupe[i].weight, 0.1f, nullptr, nullptr, "%.2f")) {
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
        bool hasFinalTransform = fractal_engine->isThereAFinalTransform(); // Is there a final transform?
        if(ImGui::Checkbox("Final transform", &hasFinalTransform)) {
            fractal_engine->stop();
            if(hasFinalTransform) {
                fractal_engine->setFinalTransform(Transform());
            } else {
                fractal_engine->clearFinalTransform();
            }
        }

        if(hasFinalTransform) {
            static int finalTransformIndex = 0;

            std::vector<const char*> supportedVars;
            for(size_t i = 0; i < supportedVariations.size(); i++) {
                supportedVars.push_back(supportedVariations[i].c_str()); // rebuild every frame -- supportedVariations leaves scope every frame, but it can't be static because ui could change 
            }

            if(ImGui::Combo("##transformSelector", &finalTransformIndex, supportedVars.data(), supportedVars.size())) {
                Transform temp;
                temp.variation = fractal_engine->getSupportedVariations()[finalTransformIndex];
                temp.color = 0.5;
                fractal_engine->setFinalTransform(temp);
            }
        }
        ImGui::EndTabItem();
        return true;
    } else {
        return false;
    }
}

void UIState::renderPresetsWindow(std::unique_ptr<Engine> &fractal_engine, std::vector<Preset> &presets) {
    if(ImGui::BeginTabItem("Presets")) {
        size_t numPresets = presets.size();

        static std::vector<const char*> presetTitles;
    
        for(size_t i = 0; i < numPresets; i++) {
            presetTitles.push_back(presets[i].displayName.c_str());
        }

        static int chosenPreset = 0;
        if(ImGui::Combo("##presetSelector", &chosenPreset, presetTitles.data(), numPresets)) {
            presets.at(chosenPreset).applyPreset(fractal_engine, *this);
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



