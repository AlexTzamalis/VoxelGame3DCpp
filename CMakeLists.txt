cmake_minimum_required(VERSION 3.16)
project(VoxelGame3D LANGUAGES CXX C)

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
    GIT_REPOSITORY https://github.com/nigels-com/glew.git
    GIT_TAG glew-2.2.0
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

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)

target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
)

# Executable
add_executable(VoxelGame3D
    src/core/Main.cpp
    src/core/Config.cpp
    src/core/stb_image_impl.cpp

    src/renderer/Camera.cpp
    src/renderer/Shader.cpp
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
)

target_include_directories(VoxelGame3D PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    ${glm_SOURCE_DIR}
    ${glew_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(VoxelGame3D PRIVATE
    glfw
    glew_s
    OpenGL::GL
    imgui
)

# Copy shaders (and optionally assets) to build dir so paths work when running from build
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
