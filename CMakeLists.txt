cmake_minimum_required(VERSION 3.20)

project(AirFlower LANGUAGES C CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLFW (installed on the system)
find_package(glfw3 REQUIRED)

add_executable(AirFlower
    Source.cpp

    Libraries/glad.c

    source/error_wrapper.cpp
    source/graphics_API.cpp
    source/graphics_API_extension.cpp

    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(AirFlower PRIVATE
    source
    imgui
    imgui/backends
    glm
    include
    ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(AirFlower PRIVATE
    glfw
    OpenGL::GL
)

# Windows-specific libraries
if (WIN32)
    target_link_libraries(AirFlower PRIVATE opengl32)
endif()