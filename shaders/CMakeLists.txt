SET(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
SET(SHADER_BINARY_DIR "${ASSET_DIR}/shaders/")

# print all the different binary dirs
message(STATUS "PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")
message(STATUS "SHADER_BINARY_DIR: ${SHADER_BINARY_DIR}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")

# collect all the shaders
file(GLOB_RECURSE SHADER_SOURCES FILES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.comp"
)

# compile all the shaders
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${SHADER_BINARY_DIR}/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND glslc ${SHADER} -o ${SHADER_OUTPUT} -I${SHADER_SOURCE_DIR}
            DEPENDS ${SHADER} ${SHADER_BINARY_DIR}
    )
    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach(SHADER)

add_custom_target(
        shaders
        DEPENDS ${SHADER_OUTPUTS}
)