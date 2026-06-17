#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cstdint>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// Fractal Flame frontend
// GUI to pick the mode -- see engines directory (./engines/) for the actual code to generate flames
#include "./engines/engine.h" // Inherit the abstract class 


struct UIConfig {
    uint64_t* global_histogram = nullptr;
    bool debugInfo = true;
    int selectedBackend = 0; // defaults to None when opening window for now
    
    float settingsPanelAlpha = 0.97f;
    float settingsPanelWidth = 0.20f; // 20% of screen width

    uint16_t window_width = 1920, window_height = 1080;
    int threadCount = 1;
    std::vector<const char*> supportedVariations;
};  

UIConfig UIConf; // global for ease of access

std::unique_ptr<Engine> selectBackend(int selection, int threadCount, fractalSettings& config) {
    switch(selection) {
        case 1: // Serial
            return std::unique_ptr<Engine>(createSerialEngine(config));
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



fractalSettings fractalConf;
GLuint flameTexture = 0; // global Texture id
void uploadHistogram();

int main(int argc, char **argv ) { 
    
    if (!glfwInit()) {
        printf("OpenGL unable to initialize.  Exiting...");
        return -1;
    }
    // Set OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(UIConf.window_width, UIConf.window_height, "Fractal Flame Engine", NULL, NULL);
    printf("Window created!\r\n");
    
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    

    glGenTextures(1, &flameTexture);
    glBindTexture(GL_TEXTURE_2D, flameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // mipmap stuff -- idrc
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexParameter.xhtml
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                fractalConf.global_histogram.width, fractalConf.global_histogram.height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // ImFont* font = io.Fonts->AddFontFromFileTTF("file.ttf", 14.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    std::unique_ptr<Engine> fractal_engine;

    bool settingsOpen = false; // starts closed by default

    const ImGuiWindowFlags flame_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize 
        |   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs
        |   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    
    const ImGuiWindowFlags button_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoNav;

    const ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_NoTabListScrollingButtons;
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
        
        uploadHistogram(); // update flameTexture

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("##flameview", nullptr, flame_flags);
        ImGui::Image((ImTextureID)(intptr_t)flameTexture, display);
        ImGui::End();
        ImGui::PopStyleVar();
        
        if(!settingsOpen) {
            ImGui::SetNextWindowPos({10, display.y - 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            ImGui::Begin("##gearButton", nullptr, button_flags);
            
            if(ImGui::Button("[S]")) { 
                settingsOpen = !settingsOpen;
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
                fractal_engine = selectBackend(selectedBackend, UIConf.threadCount, fractalConf);
                if(selectedBackend == 0) {
                    UIConf.supportedVariations = {};
                } else {
                    std::vector<variationDef> vars = fractal_engine->getSupportedVariations();
                    for(variationDef variation : vars) {
                        UIConf.supportedVariations.push_back(variation.name);
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
                    if(ImGui::Button("+ Add Transform")) {
                        Transform newTransform = {1.0, 0}; // New 
                        fractalConf.transforms.push_back(newTransform);
                        fractalConf.totalWeight = 0;
                        for(int i = 0; i < fractalConf.transforms.size(); i++) {
                            fractalConf.totalWeight += fractalConf.transforms[i].weight;
                        }
                    }

                    for(int i = 0; i < fractalConf.transforms.size(); i++) {
                        ImGui::PushID(i); // important -- gives each slider a unique ID
                        ImGui::Combo("##transformSelector", &fractalConf.transforms[i].variationIndex, UIConf.supportedVariations.data(), (int)UIConf.supportedVariations.size());
                        ImGui::Text("Weight:");
                        ImGui::SameLine();
                        ImGui::SliderFloat("##weight", &fractalConf.transforms[i].weight, 0.0f, 10.0f);
                        ImGui::SameLine();
                        if(ImGui::Button("X")) {
                            fractalConf.transforms.erase(fractalConf.transforms.begin() + i); 
                            i--;
                        }
                        ImGui::PopID();
                    }
                    ImGui::Separator();
                    static bool hasFinalTransform = false;
                    ImGui::Checkbox("Final transform", &hasFinalTransform);
                    if(hasFinalTransform) {
                        uint8_t i = fractalConf.transforms.size()+1;
                        ImGui::PushID(i); // important -- gives each slider a unique ID
                        ImGui::Combo("##transformSelector", &fractalConf.transforms[i].variationIndex, UIConf.supportedVariations.data(), (int)UIConf.supportedVariations.size());
                        ImGui::Text("Weight:");
                        ImGui::SameLine();
                        ImGui::SliderFloat("##weight", &fractalConf.transforms[i].weight, 0.0f, 10.0f);
                        ImGui::SameLine();
                        if(ImGui::Button("X")) {
                            fractalConf.transforms.pop_back(); // removes back element  
                        }
                        ImGui::PopID();
                    }


                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("UI")) {
                    // histogram width, height
                    // window width, height?
                    
                    ImGui::EndTabItem();
                }
                ImGui::PopFont();
                ImGui::EndTabBar();
            }

            ImGui::PopStyleVar(2);

            if(false) {

            }
            ImGui::End();
        }
        

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
    delete [] UIConf.global_histogram; // gonna add file saving options for the fractals later
    return 0;
}



uint64_t* reallocateHistogram() {
    std::unique_ptr<Histogram<uint64_t>> a; 

    return a->data;
}

void uploadHistogram() { // function to push the global_histogram to opengl
    uint64_t* hist = fractalConf.global_histogram.data;
    int width = fractalConf.global_histogram.getWidth(), height = fractalConf.global_histogram.getHeight();

    uint64_t maxVal = 0;
    for(int i = 0; i < width*height; i++) {
        if(hist[i] > maxVal) { // dereference with [i]
            maxVal = hist[i];
        }
    }
    if(maxVal == 0) return; // don't render when nothing has generated yet

    std::vector<uint8_t> pixels(width * height * 4); // OpenGL wants pixels as RGBA -- initialize as all 0s

    for(int i = 0; i < width*height; i++) {
        // 8 bits per pixel
        // linearly interoplate between 0 and maxVal
        uint8_t brightness = (uint8_t)((hist[i] * 255) / maxVal);
        pixels[i*4+0] = brightness;
        pixels[i*4+1] = brightness;
        pixels[i*4+2] = brightness;
        pixels[i*4+3] = 255;
    }
    glBindTexture(GL_TEXTURE_2D, flameTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}


