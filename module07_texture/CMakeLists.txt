cmake_minimum_required(VERSION 3.20)
project(module07_texture)

add_executable(module07_texture
    src/main.cpp
    src/texture.cpp
)
target_include_directories(module07_texture PRIVATE include)
target_link_libraries(module07_texture PRIVATE ogl_common)
# stb_image 实现文件（single translation unit）
target_compile_definitions(module07_texture PRIVATE STB_IMAGE_IMPLEMENTATION)

add_custom_command(TARGET module07_texture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module07_texture>/shaders)

# Also copy assets if the directory exists
add_custom_command(TARGET module07_texture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:module07_texture>/assets)

# gen_test_textures helper (builds alongside)
add_executable(gen_test_textures07
    src/gen_test_textures.cpp
)
target_include_directories(gen_test_textures07 PRIVATE include)
target_link_libraries(gen_test_textures07 PRIVATE ogl_common)
# stb_image_write is also in stb repo
target_compile_definitions(gen_test_textures07 PRIVATE
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION)
