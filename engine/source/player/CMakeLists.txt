add_executable(HybridPlayer main.cpp)

set_target_properties(HybridPlayer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "Engine")

target_link_libraries(HybridPlayer PRIVATE HybridEngine)
target_include_directories(HybridPlayer PRIVATE $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)

add_custom_command(TARGET HybridPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:HybridPlayer>"
            "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:HybridPlayer>")

if(EXISTS "${ENGINE_ROOT_DIR}/resources/assets")
    add_custom_command(TARGET HybridPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_ROOT_DIR}/resources/assets"
                "${BINARY_ROOT_DIR}/resources/assets")
endif()

if(EXISTS "${ENGINE_ROOT_DIR}/shader")
    add_custom_command(TARGET HybridPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_ROOT_DIR}/shader"
                "${BINARY_ROOT_DIR}/shader")
endif()
