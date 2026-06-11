# Unapologetically using Google AI to write this 

cmake_minimum_required(VERSION 3.16)
project(FractalFlame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Enable OpenMP (For CPU Parallel loops)
find_package(OpenMP REQUIRED)

# Include the CMake download engine
include(FetchContent)

# 2. Automatically Fetch GLFW from Github
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.4
)
# Disable compilation of GLFW's unnecessary sample files
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")

# 3. Automatically Fetch GLAD (OpenGL 3.3 Core Profile)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36 # Stable v1 branch for OpenGL C/C++
)
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
set(GLAD_GENERATOR "c" CACHE STRING "Language to generate binding for")

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        v1.92.8 # Target a stable version
)

# 4. Pull and prepare both libraries
FetchContent_MakeAvailable(glfw glad imgui)

# 5. Define your executable (Only your own source code file remains!)
add_executable(fractal_flame ./fractal_flame.cpp
    # Core Dear ImGui logic files
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    
    # Specific implementation backend bindings for your environment
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# 6. Expose ImGui and Backend headers to your compiler search path
target_include_directories(fractal_flame PRIVATE 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)


# 7. Link everything (CMake tracks include headers and system graphics libs automatically!)
target_link_libraries(fractal_flame PRIVATE 
    OpenMP::OpenMP_CXX 
    glfw 
    glad
    ${CMAKE_DL_LIBS} # Required for GLAD to interact with GPU drivers
)
