add_executable(module11_framebuffer
    src/main.cpp
    src/framebuffer.cpp
)
target_include_directories(module11_framebuffer PRIVATE include)
target_link_libraries(module11_framebuffer PRIVATE ogl_common)
target_compile_definitions(module11_framebuffer PRIVATE STB_IMAGE_IMPLEMENTATION)
add_custom_command(TARGET module11_framebuffer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module11_framebuffer>/shaders)
