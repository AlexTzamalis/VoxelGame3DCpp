cmake_minimum_required(VERSION 3.16...4.1)
project(VoxelGame3D LANGUAGES CXX C)

# Allow deprecation warning for FetchContent_Populate to be suppressed in CMake > 3.28
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# Prevent timestamp warning when downloading GLEW via URL
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

# Dependencies via FetchContent
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# GLEW (OpenGL loader) - use build/cmake subdir
FetchContent_Declare(
    glew
    URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
)
FetchContent_Populate(glew)
set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
add_subdirectory(${glew_SOURCE_DIR}/build/cmake ${glew_BINARY_DIR})
set_target_properties(glew_s PROPERTIES POSITION_INDEPENDENT_CODE ON)

# stb (stb_image.h for texture loading)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)

# Executable
add_executable(VoxelGame3D
    src/core/Main.cpp
    src/core/Config.cpp
    src/core/stb_image_impl.cpp
    src/renderer/Shader.cpp
    src/renderer/Camera.cpp
    src/renderer/Texture.cpp
    src/renderer/TextureAtlas.cpp
    src/renderer/PlayerRenderer.cpp
    src/world/Chunk.cpp
    src/world/ChunkManager.cpp
    src/world/WorldManager.cpp
    src/world/Biome.cpp
    src/world/BlockRegistry.cpp
    src/game/Inventory.cpp
    src/game/InputManager.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(VoxelGame3D PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
    ${glew_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(VoxelGame3D PRIVATE
    glfw
    glew_s
    OpenGL::GL
)

# Statically link MinGW runtime (libstdc++, libgcc, libwinpthread) so no DLLs are needed
if(MINGW)
    target_link_options(VoxelGame3D PRIVATE -static-libgcc -static-libstdc++ -static -lpthread)
endif()

# Copy assets and shaders to build dir so paths work when running from build
add_custom_command(
    TARGET VoxelGame3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets
)
