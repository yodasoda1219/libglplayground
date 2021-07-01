cmake_minimum_required(VERSION 3.10)
set(IMGUI_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
file(GLOB IMGUI_CORE_SOURCE "${IMGUI_DIRECTORY}/*.cpp")
file(GLOB IMGUI_CORE_HEADERS "${IMGUI_DIRECTORY}/*.h")
file(GLOB IMGUI_OPENGL_BACKEND "${IMGUI_DIRECTORY}/backends/imgui_impl_opengl3.*")
file(GLOB IMGUI_GLFW_BACKEND "${IMGUI_DIRECTORY}/backends/imgui_impl_glfw.*")
file(GLOB IMGUI_STDLIB "${IMGUI_DIRECTORY}/misc/cpp/imgui_stdlib.*")
set(MANIFEST ${IMGUI_CORE_SOURCE} ${IMGUI_CORE_HEADERS} ${IMGUI_OPENGL_BACKEND} ${IMGUI_GLFW_BACKEND} ${IMGUI_STDLIB})
add_library(imgui STATIC ${MANIFEST})
target_link_libraries(imgui PUBLIC glfw glad)
target_include_directories(imgui PUBLIC ${IMGUI_DIRECTORY})