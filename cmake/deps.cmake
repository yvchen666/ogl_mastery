# cmake/deps.cmake
include(FetchContent)

# ── GLFW ─────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.9
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ── GLAD（预生成源文件，放在项目内 glad_src/）────────────────────────────────
# 已通过 python3 -m glad --api gl:core=4.6 --reproducible c 预先生成
# 无需 Python 运行时
add_library(glad_lib STATIC ${CMAKE_SOURCE_DIR}/glad_src/src/gl.c)
target_include_directories(glad_lib PUBLIC ${CMAKE_SOURCE_DIR}/glad_src/include)

# ── GLM ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)

# ── stb ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)

# ── Dear ImGui ───────────────────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.1
)

# ── tinygltf ─────────────────────────────────────────────────────────────────
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.8.17
)

FetchContent_MakeAvailable(glfw glm stb imgui tinygltf)

# stb 接口库（header-only）
add_library(stb_lib INTERFACE)
target_include_directories(stb_lib INTERFACE ${stb_SOURCE_DIR})

# ImGui 库
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui_lib STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# 公共依赖接口库（大多数模块都用）
add_library(ogl_common INTERFACE)
target_link_libraries(ogl_common INTERFACE
    glfw
    glad_lib
    glm
    stb_lib
    ${CMAKE_DL_LIBS}
)
find_package(OpenGL REQUIRED)
target_link_libraries(ogl_common INTERFACE OpenGL::GL)
