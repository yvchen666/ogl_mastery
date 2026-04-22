cmake_minimum_required(VERSION 3.20)

add_executable(module18_raytracing_intro src/main.cpp)
target_include_directories(module18_raytracing_intro PRIVATE include)
target_link_libraries(module18_raytracing_intro PRIVATE ogl_common)
add_custom_command(TARGET module18_raytracing_intro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module18_raytracing_intro>/shaders)
