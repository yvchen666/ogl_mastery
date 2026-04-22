add_executable(module13_pbr
    src/main.cpp
    src/cubemap.cpp
    src/brdf_lut.cpp
    src/stb_impl.cpp
)
target_include_directories(module13_pbr PRIVATE include)
target_link_libraries(module13_pbr PRIVATE ogl_common imgui_lib)
add_custom_command(TARGET module13_pbr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module13_pbr>/shaders)
