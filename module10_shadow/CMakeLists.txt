add_executable(module10_shadow
    src/main.cpp
    src/shadow_map.cpp
)
target_include_directories(module10_shadow PRIVATE include)
target_link_libraries(module10_shadow PRIVATE ogl_common)
add_custom_command(TARGET module10_shadow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module10_shadow>/shaders)
