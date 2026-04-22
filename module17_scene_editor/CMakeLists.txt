cmake_minimum_required(VERSION 3.20)

add_executable(module17_scene_editor
    src/main.cpp
    src/gltf_loader.cpp
    src/picking.cpp
)
target_include_directories(module17_scene_editor PRIVATE include)
target_link_libraries(module17_scene_editor PRIVATE ogl_common imgui_lib tinygltf)
add_custom_command(TARGET module17_scene_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module17_scene_editor>/shaders)
