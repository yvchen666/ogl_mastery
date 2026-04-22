add_executable(module04_linear_algebra
    src/main.cpp
    src/math_vis.cpp
)
target_include_directories(module04_linear_algebra PRIVATE include)
target_link_libraries(module04_linear_algebra PRIVATE ogl_common)
add_custom_command(TARGET module04_linear_algebra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module04_linear_algebra>/shaders)
