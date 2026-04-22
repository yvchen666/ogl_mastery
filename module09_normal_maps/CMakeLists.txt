cmake_minimum_required(VERSION 3.20)
project(module09_normal_maps)

add_executable(module09_normal_maps
    src/main.cpp
    src/tangent_calculator.cpp
    src/texture.cpp
)
target_include_directories(module09_normal_maps PRIVATE include)
target_link_libraries(module09_normal_maps PRIVATE ogl_common)
target_compile_definitions(module09_normal_maps PRIVATE STB_IMAGE_IMPLEMENTATION)

add_custom_command(TARGET module09_normal_maps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module09_normal_maps>/shaders)

add_custom_command(TARGET module09_normal_maps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:module09_normal_maps>/assets)
