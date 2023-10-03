# build glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/glfw") # the libraries target name is glfw

# build vulkan memory allocator
# disable tests
set(VMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/vma")

# build meshoptimizer
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mesh_optimizer")

# build glm
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/glm")

# build sqlite
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/sqlite3_amalgamation")

# build vk-bootstrap
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/vk_bootstrap")

# build imgui and imgui_filedialog
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_FILEDIALOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui_filedialog)
set(IMGUI_SOURCES
        "${IMGUI_DIR}/imgui.h"
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_demo.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_FILEDIALOG_DIR}/ImGuiFileDialog.cpp"
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_FILEDIALOG_DIR})
find_package(Vulkan REQUIRED)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
