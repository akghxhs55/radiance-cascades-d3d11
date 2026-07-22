cmake_minimum_required(VERSION 4.2)
project(RadianceCascades LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RadianceCascades
        src/main.cpp
        src/Renderer.h
        src/Renderer.cpp
        src/Scene.h
        src/D3DUtils.h
        src/Vertex.h

        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_win32.cpp
        external/imgui/backends/imgui_impl_dx11.cpp
)

target_compile_definitions(RadianceCascades PRIVATE UNICODE _UNICODE)

target_include_directories(RadianceCascades PRIVATE
        external/imgui
        external/imgui/backends
)

target_link_libraries(RadianceCascades PRIVATE
        d3d11
        dxgi
        d3dcompiler
        dwmapi
)

file(GLOB_RECURSE ShaderFiles CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*"
)

add_custom_target(CopyShaders ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:RadianceCascades>/shaders"
        DEPENDS ${ShaderFiles}
        COMMENT "Copying shader files"
        VERBATIM
)

add_dependencies(RadianceCascades CopyShaders)
