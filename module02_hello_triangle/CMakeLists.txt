add_executable(module02_hello_triangle src/main.cpp)
target_link_libraries(module02_hello_triangle PRIVATE ogl_common)

# 构建后将 shaders/ 目录复制到可执行文件旁边
# 这样着色器路径 "shaders/triangle.vert" 相对于工作目录可以正确找到
add_custom_command(TARGET module02_hello_triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module02_hello_triangle>/shaders)
