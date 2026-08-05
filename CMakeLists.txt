cmake_minimum_required(VERSION 3.16)
project(FractalFlame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug Release RelWithDebInfo MinSizeRel Profiling Speed)

# --- Compiler flags ---

# Profiling flags
set(CMAKE_CXX_FLAGS_PROFILING "${CMAKE_CXX_FLAGS_PROFILING} -O2 -Wall -pg")

# Debug build flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Og -Wall -Wextra")

# Release / Speed -- native-only flags gated behind NOT EMSCRIPTEN (harmful for WASM)
if(NOT EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native -flto=auto -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_SPEED   "${CMAKE_CXX_FLAGS_SPEED} -Ofast -DNDEBUG -march=native -flto=auto")
else()
    # WASM: no -march=native; emscripten picks the target
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_SPEED   "${CMAKE_CXX_FLAGS_SPEED} -O3 -DNDEBUG")
endif()

## Emscripten is a WASM compiler -- I want to get this working in-browser.
## link (if it's working) is https://fractal.emahizer.net
##
## This branch is deliberately kept separate from (and much smaller than) the
## native build below: no GLFW/GLAD/ImGui/OpenMP/CUDA, since the web target
## renders straight to a <canvas> 2D context via JS glue (see web/web_main.cpp)
## and runs single-threaded (see engines/web_engine.cpp -- Web_Engine extends
## Engine directly, not Serial_Engine, specifically so nothing here needs
## std::thread / -pthread / SharedArrayBuffer / COOP+COEP headers).

if(EMSCRIPTEN)
    add_executable(flame_web web/web_main.cpp)
    target_link_options(flame_web PRIVATE
        # -sALLOW_MEMORY_GROWTH=1 # this breaks the randomization 
        -sINITIAL_MEMORY=268435456
        -sEXPORTED_FUNCTIONS=_main,_apply_seed,_set_quality,_commit_view
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    )
    set_target_properties(flame_web PROPERTIES 
                        SUFFIX ".html"
                        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/shell.html")

else() 

# Native build (GLFW/OpenGL/ImGui + Serial/OpenMP/CUDA engines).
# Unchanged from before, just now explicitly skipped for Emscripten
# configures -- OpenMP and GLFW's own CMake don't need to be touched
# (or FetchContent-ed) at all for the web target.
       
# I have a Github Action to update https://fractal.emahizer.net when I run the action.


find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()


find_package(CUDAToolkit 13.0 QUIET)

if(CUDAToolkit_FOUND)
    message(STATUS "CUDA found -- building with GPU support")

    find_program(CMAKE_CUDA_COMPILER nvcc
        PATHS /usr/local/cuda/bin
        NO_DEFAULT_PATH  # don't use system PATH, prefer explicit path
    )
    if(NOT CMAKE_CUDA_COMPILER)
        find_program(CMAKE_CUDA_COMPILER nvcc) # fallback to system PATH
    endif()

    set(CMAKE_CUDA_ARCHITECTURES native)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
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
add_executable(frontend 
    ./fractal_flame_frontend.cpp    
)

# target is "frontend", on-disk binary stays fractal_flame
set_target_properties(frontend PROPERTIES OUTPUT_NAME fractal_flame)

add_executable(benchmark
    ./benchmark.cpp
)

target_compile_options(benchmark PRIVATE ${OpenMP_CXX_FLAGS})

# ASan+UBSan on host targets only, Debug only. nvcc rejects -fsanitize, so
# skip cuda_engine (use compute-sanitizer for device).
# CUDA-enabled Debug runs may need ASAN_OPTIONS=protect_shadow_gap=0
set(HOST_SANITIZE_TARGETS serial_engine openmp_engine frontend benchmark)
foreach(sanitize_target IN LISTS HOST_SANITIZE_TARGETS)
    target_compile_options(${sanitize_target} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endforeach()
# link options only matter for the executables (object libs are never linked)
target_link_options(frontend  PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(benchmark PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)



target_link_libraries(frontend PRIVATE
    $<TARGET_OBJECTS:serial_engine>
    $<TARGET_OBJECTS:openmp_engine>
)

target_link_libraries(benchmark PRIVATE
        $<TARGET_OBJECTS:serial_engine>
        $<TARGET_OBJECTS:openmp_engine>
        OpenMP::OpenMP_CXX
    )

if(CUDAToolkit_FOUND)
    target_link_libraries(frontend PRIVATE
        $<TARGET_OBJECTS:cuda_engine>
        CUDA::cudart_static
    )
    
    target_link_libraries(benchmark PRIVATE
        $<TARGET_OBJECTS:cuda_engine>
        CUDA::cudart_static
    )
    target_compile_definitions(frontend PRIVATE HAS_CUDA)
    target_compile_definitions(benchmark PRIVATE HAS_CUDA)
endif()


# Expose ImGui and Backend headers to your compiler search path
target_include_directories(imgui_lib PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)


# Link everything (CMake tracks include headers and system graphics libs automatically!)
target_link_libraries(frontend PRIVATE
    imgui_lib
    OpenMP::OpenMP_CXX 
    glfw 
    glad
    ${CMAKE_DL_LIBS} # Required for GLAD to interact with GPU drivers
)

endif() # EMSCRIPTEN