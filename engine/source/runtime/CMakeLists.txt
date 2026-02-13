set(TARGET_NAME HybridRuntime)

# 头/源文件收集
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "*.cpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "Engine"
)

# MSVC：更严格的标准一致性
target_compile_options(${TARGET_NAME}
    PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->"
)

# 链接依赖（是否 PUBLIC/PRIVATE 取决于对外头文件是否暴露这些库）
target_link_libraries(${TARGET_NAME}
    PUBLIC glfw
    PUBLIC glad
    PUBLIC glm
    PUBLIC imgui
    PUBLIC spdlog::spdlog
    PUBLIC EnTT::EnTT
)
# 包含目录
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>
)
