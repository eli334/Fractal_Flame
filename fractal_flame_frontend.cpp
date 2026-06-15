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
    
    float settingsPanelAlpha = 0.97f;
    float settingsPanelWidth = 0.20f;

    uint16_t window_width = 1920, window_height = 1080;
};  

UIConfig UIConf; // global for ease of access
fractalSettings fractalConf;
std::vector<Transform> transforms;

GLuint flameTexture = 0; // global Texture

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
                fractalConf.histogram_width, fractalConf.histogram_height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // ImFont* font = io.Fonts->AddFontFromFileTTF("file.ttf", 14.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool settingsOpen = false; // starts closed by default

    ImGuiWindowFlags button_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoNav;

    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_NoTabListScrollingButtons;

    fractalConf.transforms = &transforms;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame(); // {Link: Recited in GH #9112 https://github.com/ocornut/imgui/issues/9112}
        ImGui::NewFrame();

        ////                    ////
        ////    FRONTEND CODE   ////
        ////                    ////
        
        ImVec2 display = ImGui::GetIO().DisplaySize;

        
        if(!settingsOpen) {
            ImGui::SetNextWindowPos({10, display.y - 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({50, 40}, ImGuiCond_Always);
            ImGui::Begin("##gearButton", nullptr, button_flags);
            
            if(ImGui::Button("[S]")) { 
                settingsOpen = !settingsOpen;
            }
            ImGui::End();
        } else {
            ImGui::SetNextWindowPos({10, 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({500, display.y - 80}, ImGuiCond_Always);
            ImGui::Begin("Settings", &settingsOpen, settings_flags); // Contents of the Settings menu:
            
            const char* backends[] = { "Serial", "OpenMP", "CUDA" }; 
            static int selectedBackend = 0;
            
            ImGui::PushFont(NULL, 18.0f);
            ImGui::Text("Backend:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1); // fill available width
            ImGui::Combo("##backendSelector", &selectedBackend, backends, IM_ARRAYSIZE(backends));
            ImGui::PopFont();



            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_TabBarBorderSize, 0.0f);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f); // move blue tab line down by 12 pixels
            if(ImGui::BeginTabBar("##settingsTabs", ImGuiTabBarFlags_None)) {
                
                ImGui::PushFont(NULL, 20.0f); 
                if(ImGui::BeginTabItem("Transforms")) {
                    if(ImGui::Button("+")) {
                        transforms.push_back(Transform{});
                    }

                    for(int i = 0; i < transforms.size(); i++) {
                        ImGui::PushID(i); // important -- gives each slider a unique ID
                        ImGui::SliderFloat("Weight", &transforms[i].weight, 0.0f, 1.0f);
                        ImGui::SameLine();
                        if(ImGui::Button("X")) {
                            transforms.erase(transforms.begin() + i); // removes 
                            i--;
                        }
                        ImGui::PopID();
                    }
                    ImGui::Separator();
                    static bool hasFinalTransform = false;
                    ImGui::Checkbox("Final transform", &hasFinalTransform);
                    if(hasFinalTransform) {
                        uint8_t i = transforms.size()+1;
                        ImGui::PushID(i); // important -- gives each slider a unique ID
                        ImGui::SliderFloat("Weight", &transforms[i].weight, 0.0f, 1.0f);
                        ImGui::SameLine();
                        if(ImGui::Button("X")) {
                            transforms.erase(transforms.begin() + i); // removes 
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

