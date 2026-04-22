cmake_minimum_required(VERSION 3.20)

add_executable(module16_yuv_renderer
    src/main.cpp
    src/yuv_renderer.cpp
)
target_include_directories(module16_yuv_renderer PRIVATE include)
target_link_libraries(module16_yuv_renderer PRIVATE ogl_common)
add_custom_command(TARGET module16_yuv_renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module16_yuv_renderer>/shaders)
