add_executable(module12_deferred
    src/main.cpp
    src/gbuffer.cpp
    src/ssao.cpp
)
target_include_directories(module12_deferred PRIVATE include)
target_link_libraries(module12_deferred PRIVATE ogl_common)
add_custom_command(TARGET module12_deferred POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module12_deferred>/shaders)
