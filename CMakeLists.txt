#[[
Before running, run this command to install dependencies on Ubuntu (CUDA has to be installed seperately -- it will still compile without):
sudo apt update && sudo apt install -y libwayland-dev build-essential wayland-protocols libxkbcommon-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
]]

cmake_minimum_required(VERSION 3.16)
project(FractalFlame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Compiler flags ---
if(MSVC)
    # If you ever use Windows/MSVC, these are the equivalent flags
    add_compile_options(/W4 /WX) 
else()
    # Debug build flags
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined -Wall -Wextra")
    
    # Release build flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native -flto")

    # Ultra speed flags
    set(CMAKE_CXX_FLAGS_SPEED "${CMAKE_CXX_FLAGS_SPEED} -Ofast -DNDEBUG -march=native -flto")
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()


find_package(CUDAToolkit QUIET)

if(CUDAToolkit_FOUND)
    message(STATUS "CUDA found -- building with GPU support")
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_ARCHITECTURES native)
else()
    message(STATUS "CUDA not found -- building CPU-only")
endif()

# Enable OpenMP
find_package(OpenMP REQUIRED)

# Include the CMake download engine
include(FetchContent)

# Automatically Fetch GLFW from Github
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
# Disable compilation of GLFW's unnecessary sample files
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")


# Automatically Fetch GLAD (OpenGL 3.3 Core Profile)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36 # Stable v1 branch for OpenGL C/C++
)
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
set(GLAD_GENERATOR "c" CACHE STRING "Language to generate binding for")

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8 # Target a stable version
)

# Pull and prepare libraries
FetchContent_MakeAvailable(glfw glad imgui)

add_library(imgui_lib STATIC
    # ImGui logic files
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp 
)
target_link_libraries(imgui_lib PUBLIC glfw)

# Add each engine as a library to executable
add_library(serial_engine OBJECT engines/serial_engine.cpp)
target_compile_options(serial_engine PRIVATE ${OpenMP_CXX_FLAGS})

add_library(openmp_engine OBJECT engines/openmp_engine.cpp)
target_compile_options(openmp_engine PRIVATE ${OpenMP_CXX_FLAGS})

# CUDA support
if(CUDAToolkit_FOUND)
    add_library(cuda_engine OBJECT engines/cuda_engine.cu)
    set_target_properties(cuda_engine PROPERTIES CUDA_STANDARD 17)
endif()

# Define your executable (Only your own source code file remains!)
add_executable(fractal_flame 
    ./fractal_flame_frontend.cpp    
)

target_link_libraries(fractal_flame PRIVATE
    $<TARGET_OBJECTS:serial_engine>
    $<TARGET_OBJECTS:openmp_engine>
)

if(CUDAToolkit_FOUND)
    target_link_libraries(fractal_flame PRIVATE
        $<TARGET_OBJECTS:cuda_engine>
        CUDA::cudart
    )
    target_compile_definitions(fractal_flame PRIVATE HAS_CUDA)
endif()


# Expose ImGui and Backend headers to your compiler search path
target_include_directories(imgui_lib PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)


# Link everything (CMake tracks include headers and system graphics libs automatically!)
target_link_libraries(fractal_flame PRIVATE
    imgui_lib
    OpenMP::OpenMP_CXX 
    glfw 
    glad
    ${CMAKE_DL_LIBS} # Required for GLAD to interact with GPU drivers
)



