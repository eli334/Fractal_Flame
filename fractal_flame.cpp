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



// Fractal Flame generation (CPU only)
// Has a GUI at the start to pick the mode -- see other files for implementations
// Maximizes performance with OpenMP, if it's worth splitting the work into multiple cores


class FractalFlameRenderer { // one renderer per thread
    public:
        FractalFlameRenderer(int randomSeed) : seed(randomSeed){};
        void step(); // iterate renderer once


        ~FractalFlameRenderer();
    private:
        int seed;
        int* local_histogram; // stored locally
        int* global_histogram; // the one used for display (not sure if needed yet? almost definitely yes though)
};


int main(int argc, char **argv ) { 
    bool headless = true;
    if(argc == 2) {
        if(std::string(argv[1]) == "--gui") {
            headless = false;
        }
    }
    
    // args follow format "<>" (undecided)
    if(headless) {
        // handle args if headless is true
        // start generator
        printf("Headless mode detected -- opening directly to renderer...");
        printf("argv[1]: %s", argv[1]);
    } else {
        if (!glfwInit()) return -1;
        // Set OpenGL 3.3 Core Profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(640, 480, "Configure Settings", NULL, NULL);
        printf("Window created!\r\n");
        
        if (!window) { glfwTerminate(); return -1; }
        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame(); // {Link: Recited in GH #9112 https://github.com/ocornut/imgui/issues/9112}
            ImGui::NewFrame();

            // code to modify contents of window goes here
            ImGui::Begin("Fractal Flame Engine");

            
            ImGui::End();
            // Render
            ImGui::Render();
            glClear(GL_COLOR_BUFFER_BIT); // stops Garry's Mod out of bounds effect
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    printf("Exiting gracefully...");
    return 0;
}

