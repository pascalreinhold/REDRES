#cmake_minimum_required(VERSION 3.19)
#project(lab)
#
#if (MSVC)
#    # warning level 4
#    add_compile_options(/W4)
#else()
#    # additional warnings
#    add_compile_options(-Wall -Wextra -Wpedantic)
#endif()
#
#set(CMAKE_CXX_STANDARD 20)
#set(LAB_BINARY_DIR "${PROJECT_BINARY_DIR}/out")
#
#add_subdirectory(third_party)
#add_subdirectory(render_engine)
#add_subdirectory(shaders)
#add_subdirectory(app)

cmake_minimum_required(VERSION 3.18)
project(gpu_driven_rcc VERSION 0.0.2)

#------------------------------------Settings--------------------------------------------------------------------------
set(ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(OUTPUT_DIR "${PROJECT_BINARY_DIR}/out")

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

add_subdirectory(shaders)
add_subdirectory(extern)
add_subdirectory(render_engine)
add_subdirectory(app)

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

#install(TARGETS ${APP_TARGET} DESTINATION bin)
#install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION /usr/share/${PROJECT_NAME})
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/misc/${PROJECT_NAME}.desktop DESTINATION /usr/share/applications)
#
#set(CPACK_GENERATOR "DEB")
#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/docs/LICENCE")
#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/docs/README.md")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS
#        "libc6, libvulkan-dev,libglm-dev,libglfw3-dev,libsqlite3-dev,vulkan-validationlayers-dev,libtbb2")
#set(CPACK_PACKAGING_INSTALL_PREFIX "/")
#
## Description
#set(CPACK_DEBIAN_PACKAGE_NAME ${APP_TARGET})
#set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
#set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pascal Reinhold pascalreinhold42@gmail.com")
#set(CPACK_DESCRIPTION_SUMMARY "This is a visualizer for reaction events in molecular dynamics simulation")
#set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "This is a visualizer for reaction events in molecular dynamics simulation")
#
## Prints
#cmake_print_variables(${PROJECT_NAME} ${PROJECT_BINARY_DIR})
#cmake_print_variables(${CPACK_PACKAGING_INSTALL_PREFIX})
#
#include(CPack)


