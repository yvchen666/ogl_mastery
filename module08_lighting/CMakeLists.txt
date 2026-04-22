cmake_minimum_required(VERSION 3.20)
project(module08_lighting)

add_executable(module08_lighting
    src/main.cpp
)
target_include_directories(module08_lighting PRIVATE include)
target_link_libraries(module08_lighting PRIVATE ogl_common)

add_custom_command(TARGET module08_lighting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module08_lighting>/shaders)
