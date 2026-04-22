add_executable(module05_transforms
    src/main.cpp
    src/shader.cpp
    src/mesh.cpp
)
target_include_directories(module05_transforms PRIVATE include)
target_link_libraries(module05_transforms PRIVATE ogl_common)
add_custom_command(TARGET module05_transforms POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module05_transforms>/shaders)
