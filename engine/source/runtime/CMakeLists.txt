set(TARGET_NAME HybridRuntime)

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "*.cpp")
set(ALL_RUNTIME_FILES ${HEADER_FILES} ${SOURCE_FILES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ALL_RUNTIME_FILES})

set(RENDER_PUBLIC_FILES  ${ALL_RUNTIME_FILES})
set(RENDER_RUNTIME_FILES ${ALL_RUNTIME_FILES})
set(RENDER_BACKEND_FILES ${ALL_RUNTIME_FILES})

list(FILTER RENDER_PUBLIC_FILES  INCLUDE REGEX ".*[\\\\/]+modules[\\\\/]+render[\\\\/]+public[\\\\/]+.*")
list(FILTER RENDER_RUNTIME_FILES INCLUDE REGEX ".*[\\\\/]+modules[\\\\/]+render[\\\\/]+runtime[\\\\/]+.*")
list(FILTER RENDER_BACKEND_FILES INCLUDE REGEX ".*[\\\\/]+modules[\\\\/]+render[\\\\/]+backend[\\\\/]+.*")

source_group("Runtime/Render/Public"  FILES ${RENDER_PUBLIC_FILES})
source_group("Runtime/Render/Runtime" FILES ${RENDER_RUNTIME_FILES})
source_group("Runtime/Render/Backend" FILES ${RENDER_BACKEND_FILES})

add_library(${TARGET_NAME} STATIC ${ALL_RUNTIME_FILES})

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "Engine"
)

target_compile_options(${TARGET_NAME}
    PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->"
)

target_link_libraries(${TARGET_NAME}
    PUBLIC glfw glad glm imgui stb
    PUBLIC spdlog::spdlog EnTT::EnTT nlohmann_json::nlohmann_json
)

target_include_directories(${TARGET_NAME}
    PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>
)

target_compile_definitions(${TARGET_NAME}
    PRIVATE HYBRID_PROJECT_ROOT_DIR=\"$<SHELL_PATH:${PROJECT_ROOT_DIR}>\"
)