cmake_minimum_required(VERSION 3.20)

add_executable(module15_particles
    src/main.cpp
    src/particle_system.cpp
)
target_include_directories(module15_particles PRIVATE include)
target_link_libraries(module15_particles PRIVATE ogl_common)
add_custom_command(TARGET module15_particles POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:module15_particles>/shaders)
