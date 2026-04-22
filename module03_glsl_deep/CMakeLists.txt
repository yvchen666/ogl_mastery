add_executable(module03_glsl_deep
    src/main.cpp
    src/ubo_helper.cpp
)
target_include_directories(module03_glsl_deep PRIVATE include)
target_link_libraries(module03_glsl_deep PRIVATE ogl_common)

# 构建后将 shaders/ 目录复制到可执行文件旁边
add_custom_command(TARGET module03_glsl_deep POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module03_glsl_deep>/shaders)
