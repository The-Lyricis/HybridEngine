set(TARGET_NAME HybridRuntime)

# 收集源文件
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "*.cpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "Engine"
)

# MSVC: /permissive-
target_compile_options(${TARGET_NAME}
    PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->"
)

# 链接依赖
target_link_libraries(${TARGET_NAME}
    PUBLIC glfw
    PUBLIC glad
    PUBLIC glm
    PUBLIC imgui
    PUBLIC stb
    PUBLIC spdlog::spdlog
    PUBLIC EnTT::EnTT
    PUBLIC nlohmann_json::nlohmann_json
)

# 包含目录
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>
)

# 传递源码根目录给运行时（用于资源路径解析）
target_compile_definitions(${TARGET_NAME} PRIVATE HYBRID_PROJECT_ROOT_DIR="${PROJECT_ROOT_DIR}")
