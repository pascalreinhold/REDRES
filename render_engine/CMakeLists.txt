# build render_engine_lib
set(RENDER_ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(RENDER_ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(RENDER_ENGINE_SOURCES
        "${RENDER_ENGINE_SOURCE_DIR}/engine.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/pipeline.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/mesh.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/camera.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/descriptors.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/utils.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/window.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/buffer.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/visualization_data.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/visualization_data_loader.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/vma_implementation.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/scene.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/gui.cpp"
        "${RENDER_ENGINE_SOURCE_DIR}/swapchain.cpp"
)

set(RENDER_ENGINE_HEADERS
        "${RENDER_ENGINE_INCLUDE_DIR}/window.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/engine.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/buffer.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/swapchain.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/mesh.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/camera.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/descriptors.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/utils.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/visualization_data.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/visualization_data_loader.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/vulkan_include.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/scene.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/gui.hpp"
        "${RENDER_ENGINE_INCLUDE_DIR}/vulkan_types.hpp"
)


add_library(render_engine_lib STATIC "${RENDER_ENGINE_SOURCES}")
target_include_directories(render_engine_lib PUBLIC "${RENDER_ENGINE_INCLUDE_DIR}")
target_compile_definitions(render_engine_lib PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
set_target_properties(render_engine_lib PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${RENDER_ENGINE_BINARY_DIR}"
        CXX_STANDARD 20
)

# copy assets to build directory
add_custom_command(TARGET render_engine_lib POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "${OUTPUT_DIR}/assets"
)

# vulkan [SDK]
find_package(Vulkan REQUIRED)
target_link_libraries(render_engine_lib PUBLIC Vulkan::Vulkan)

# glfw [submodule]
target_link_libraries(render_engine_lib PUBLIC glfw)

# Vulkan Memory Allocator [submodule]
target_link_libraries(render_engine_lib PUBLIC VulkanMemoryAllocator)

# glm [submodule]
target_link_libraries(render_engine_lib PUBLIC glm)

# meshoptimizer [submodule]
target_link_libraries(render_engine_lib PUBLIC meshoptimizer)

# imgui + imgui_file_dialogs [submodule]
target_link_libraries(render_engine_lib PRIVATE imgui)

# eigen [submodule] [header only]
target_include_directories(render_engine_lib PUBLIC "${CMAKE_SOURCE_DIR}/extern/Eigen_3_4_1")

# sqlite3 [submodule] [amalgamation]
target_link_libraries(render_engine_lib PUBLIC sqlite3)
target_include_directories(render_engine_lib PUBLIC "${CMAKE_SOURCE_DIR}/extern/sqlite3_amalgamation")

# json [submodule] [header only]
target_include_directories(render_engine_lib PUBLIC "${CMAKE_SOURCE_DIR}/extern/nlohmann_json/single_include")

# tiny obj loader [submodule] [header only]
target_include_directories(render_engine_lib PUBLIC "${CMAKE_SOURCE_DIR}/extern/tiny_obj_loader")

# vk-bootstrap [submodule]
target_link_libraries(render_engine_lib PUBLIC vk-bootstrap::vk-bootstrap)

#-------------------------------------Link Libraries--------------------------------------------------------------------
#target_link_libraries(${APP_TARGET} PRIVATE tbb)

