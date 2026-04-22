add_executable(module14_advanced_gl
    src/main.cpp
    src/instance_renderer.cpp
    src/indirect_draw.cpp
    src/ubo_manager.cpp
)
target_include_directories(module14_advanced_gl PRIVATE include)
target_link_libraries(module14_advanced_gl PRIVATE ogl_common imgui_lib)
add_custom_command(TARGET module14_advanced_gl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module14_advanced_gl>/shaders)
