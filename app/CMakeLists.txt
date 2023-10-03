set(CMAKE_CURRENT_BINARY_DIR ${OUTPUT_DIR})
add_executable(render_engine_app main.cpp)
target_link_libraries(render_engine_app render_engine_lib)
add_dependencies(render_engine_app shaders)
set_target_properties(render_engine_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)