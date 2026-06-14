#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>



// Fractal Flame frontend
// GUI to pick the mode -- see engines directory (./engines/) for the actual code to generate flames
#include "./engines/engine.h" // Inherit the abstract class (might get rid of this later -- I don't think it has to be in the frontend)

struct UIConfig {
    bool debugInfo = true;
    float settingsPanelAlpha = 0.97f;
    float settingsPanelWidth = 0.20f;
};

int main(int argc, char **argv ) { 
    if (!glfwInit()) {
        printf("OpenGL unable to initialize.  Exiting...");
        return -1;
    }
    // Set OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fractal Flame Engine", NULL, NULL);
    printf("Window created!\r\n");
    
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool settingsOpen = false; // starts closed by default

    ImGuiWindowFlags button_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoNav;

    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

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
            ImGui::Begin("##gearbutton", nullptr, button_flags);
            
            if(ImGui::Button("[S]")) { 
                settingsOpen = !settingsOpen;
            }
            ImGui::End();
        } else {
            ImGui::SetNextWindowPos({10, 40}, ImGuiCond_Always); // settings positioning
            ImGui::SetNextWindowSize({200, display.y - 80}, ImGuiCond_Always);
            ImGui::Begin("Settings", &settingsOpen, settings_flags);
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
    return 0;
}

