add_executable(module06_camera
    src/main.cpp
    src/camera.cpp
    src/frustum.cpp
)
target_include_directories(module06_camera PRIVATE include)
target_link_libraries(module06_camera PRIVATE ogl_common)
add_custom_command(TARGET module06_camera POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module06_camera>/shaders)
