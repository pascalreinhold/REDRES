cmake_minimum_required(VERSION 3.18)
project(gpu_driven_rcc VERSION 0.0.2)
set(APP_TARGET ${PROJECT_NAME})
include(CMakePrintHelpers)
#------------------------------------Settings--------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_options(-O3)
#add_compile_options(-march=native)
add_compile_options(-g)

option(PRINT_TIMINGS "Print the time for all database functions" OFF)
option(GUI_DEV_MODE "Enable dev mode" OFF)

if (GUI_DEV_MODE)
    add_compile_options(-DRCC_GUI_DEV_MODE)
endif()

if (PRINT_TIMINGS)
    add_definitions(-DRCC_ACTIVATE_PRINT_TIMING)
endif()

#------------------------------------My Files---------------------------------------------------------------------------

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)

set(SOURCES
        "${SOURCE_DIR}/engine.cpp"
        "${SOURCE_DIR}/pipeline.cpp"
        "${SOURCE_DIR}/mesh.cpp"
        "${SOURCE_DIR}/camera.cpp"
        "${SOURCE_DIR}/descriptors.cpp"
        "${SOURCE_DIR}/utils.cpp"
        "${SOURCE_DIR}/window.cpp"
        "${INCLUDE_DIR}/window.hpp"
        "${INCLUDE_DIR}/engine.hpp"
        "${SOURCE_DIR}/visualization_data.cpp"
        "${SOURCE_DIR}/visualization_data_loader.cpp"
        "${SOURCE_DIR}/scene.cpp"
        "${SOURCE_DIR}/gui.cpp"
        "${SOURCE_DIR}/swapchain.cpp"
        "${INCLUDE_DIR}/swapchain.hpp"
        "${INCLUDE_DIR}/mesh.hpp"
        "${INCLUDE_DIR}/camera.hpp"
        "${INCLUDE_DIR}/descriptors.hpp"
        "${INCLUDE_DIR}/utils.hpp"
        "${INCLUDE_DIR}/visualization_data.hpp"
        "${INCLUDE_DIR}/visualization_data_loader.hpp"
        "${INCLUDE_DIR}/scene.hpp"
        "${INCLUDE_DIR}/gui.hpp"
        "${INCLUDE_DIR}/vulkan_types.hpp"
)

add_executable(${APP_TARGET} main.cpp ${SOURCES})

target_include_directories(${APP_TARGET} PRIVATE ${INCLUDE_DIR})
target_include_directories(${APP_TARGET} PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)


#------------------------------------ImGui------------------------------------------------------------------------------

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui)
set(IMGUI_FILEDIALOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui_filedialog)
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
add_library(IMGUI STATIC ${IMGUI_SOURCES})
target_include_directories(IMGUI PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_FILEDIALOG_DIR})
target_link_libraries(${APP_TARGET} PRIVATE IMGUI)

#-------------------------------------VK-Bootstrap----------------------------------------------------------------------
add_subdirectory("./extern/vk-bootstrap")
#-------------------------------------Tiny-OBJ-Loader-------------------------------------------------------------------
include_directories("./extern/tinyobjloader/")
#-------------------------------------nlohmann-JSON-Reader--------------------------------------------------------------
include_directories("./extern/JSON/")
#-------------------------------------Eigen-----------------------------------------------------------------------------
include_directories("./extern/Eigen")
#-------------------------------------VK-Memory Allocator---------------------------------------------------------------
include_directories("./extern/vulkan_memory_allocator/include")
#-------------------------------------Meshoptimizer---------------------------------------------------------------------
add_subdirectory("./extern/meshoptimizer")
#-------------------------------------Link Libraries--------------------------------------------------------------------
target_link_libraries(${APP_TARGET} PRIVATE vulkan tbb sqlite3 glfw vk-bootstrap::vk-bootstrap meshoptimizer)

#-------------------------------------Installation----------------------------------------------------------------------
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/models")
set(FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts")

#set(OBJ_MODELS
#        "${ASSET_DIR}/models/cylinder.obj"
#        "${ASSET_DIR}/models/bond.obj ${ASSET_DIR}/models/pointer.obj"
#        "${ASSET_DIR}/models/sphere.obj")
#set(TTF_FONTS ${ASSET_DIR}/fonts/Inter-Regular)

#install(DIRECTORY ${SHADER_DIR} DESTINATION ${PROJECT_NAME}
#        PATTERN "*.vert")
#install(DIRECTORY ${SHADER_DIR} DESTINATION ${PROJECT_NAME}
#        PATTERN "*.frag")

install(TARGETS ${APP_TARGET} DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION /usr/share/${PROJECT_NAME})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/misc/${PROJECT_NAME}.desktop DESTINATION /usr/share/applications)

set(CPACK_GENERATOR "DEB")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6, libvulkan-dev,libglm-dev,libglfw3-dev,libsqlite3-dev,vulkan-validationlayers-dev,libtbb2")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")

# Description
set(CPACK_DEBIAN_PACKAGE_NAME ${APP_TARGET})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pascal Reinhold pascalreinhold42@gmail.com")
set(CPACK_DESCRIPTION_SUMMARY "This is a visualizer for reaction events in molecular dynamics simulation")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "This is a visualizer for reaction events in molecular dynamics simulation")

# Prints
cmake_print_variables(${PROJECT_NAME} ${PROJECT_BINARY_DIR})
cmake_print_variables(${CPACK_PACKAGING_INSTALL_PREFIX})

include(CPack)
