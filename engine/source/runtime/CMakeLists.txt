set(HYBRID_RUNTIME_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
file(TO_CMAKE_PATH "${PROJECT_ROOT_DIR}" HYBRID_PROJECT_ROOT_DIR_CMAKE)
file(TO_CMAKE_PATH "${BINARY_ROOT_DIR}" HYBRID_BINARY_ROOT_DIR_CMAKE)

function(hybrid_collect target_var)
    set(collected)
    foreach(path IN LISTS ARGN)
        file(GLOB_RECURSE path_files CONFIGURE_DEPENDS
            "${HYBRID_RUNTIME_SOURCE_ROOT}/${path}/*.h"
            "${HYBRID_RUNTIME_SOURCE_ROOT}/${path}/*.hpp"
            "${HYBRID_RUNTIME_SOURCE_ROOT}/${path}/*.cpp"
            "${HYBRID_RUNTIME_SOURCE_ROOT}/${path}/*.cxx")
        list(APPEND collected ${path_files})
    endforeach()
    set(${target_var} ${collected} PARENT_SCOPE)
endfunction()

hybrid_collect(HYBRID_CORE_FILES core)
hybrid_collect(HYBRID_RUNTIME_FILES
    modules/asset modules/input modules/physics modules/project modules/scene resources)
hybrid_collect(HYBRID_RHI_FILES modules/render/rhi)
hybrid_collect(HYBRID_RENDER_PUBLIC_FILES modules/render/public)
hybrid_collect(HYBRID_RENDER_OPENGL_FILES modules/render/backend/opengl)
hybrid_collect(HYBRID_RENDER_BACKEND_FACTORY_FILES modules/render/backend/factory)
hybrid_collect(HYBRID_RENDER_RUNTIME_FILES modules/render/runtime)
hybrid_collect(HYBRID_PLATFORM_GLFW_FILES platform/glfw)
hybrid_collect(HYBRID_ENGINE_FILES runtime)

foreach(group_files IN ITEMS
        HYBRID_CORE_FILES HYBRID_RUNTIME_FILES HYBRID_RHI_FILES HYBRID_RENDER_PUBLIC_FILES
        HYBRID_RENDER_OPENGL_FILES HYBRID_RENDER_BACKEND_FACTORY_FILES HYBRID_RENDER_RUNTIME_FILES
        HYBRID_PLATFORM_GLFW_FILES HYBRID_ENGINE_FILES)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${${group_files}})
endforeach()

add_library(HybridCore STATIC ${HYBRID_CORE_FILES})
add_library(HybridRuntime STATIC ${HYBRID_RUNTIME_FILES})
add_library(HybridRHI STATIC ${HYBRID_RHI_FILES})
add_library(HybridRHI_OpenGL STATIC ${HYBRID_RENDER_PUBLIC_FILES} ${HYBRID_RENDER_OPENGL_FILES})
add_library(HybridRenderBackendFactory STATIC ${HYBRID_RENDER_BACKEND_FACTORY_FILES})
add_library(HybridPlatform_GLFW STATIC ${HYBRID_PLATFORM_GLFW_FILES})
add_library(HybridRender STATIC ${HYBRID_RENDER_RUNTIME_FILES})
add_library(HybridEngine STATIC ${HYBRID_ENGINE_FILES})

foreach(target IN ITEMS HybridCore HybridRuntime HybridRHI HybridRHI_OpenGL HybridRenderBackendFactory HybridPlatform_GLFW HybridRender HybridEngine)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "Engine")
    target_compile_options(${target} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)
endforeach()

target_link_libraries(HybridCore PUBLIC glm spdlog::spdlog)
target_link_libraries(HybridRuntime
    PUBLIC HybridCore glm EnTT::EnTT
    PRIVATE stb tinyobjloader nlohmann_json::nlohmann_json)
target_link_libraries(HybridRHI PUBLIC HybridCore glm)
target_link_libraries(HybridRHI_OpenGL
    PUBLIC HybridRHI HybridRuntime
    PRIVATE glad glfw stb nlohmann_json::nlohmann_json)
target_link_libraries(HybridRenderBackendFactory
    PUBLIC HybridRHI
    PRIVATE HybridRHI_OpenGL)
target_link_libraries(HybridPlatform_GLFW
    PUBLIC HybridCore
    PRIVATE glfw)
target_link_libraries(HybridRender
    PUBLIC HybridRuntime HybridRHI
    PRIVATE HybridRHI_OpenGL stb nlohmann_json::nlohmann_json)
target_link_libraries(HybridEngine
    PUBLIC HybridRender
    PRIVATE HybridPlatform_GLFW HybridRenderBackendFactory)

target_compile_definitions(HybridRuntime PRIVATE
    HYBRID_PROJECT_ROOT_DIR="${HYBRID_PROJECT_ROOT_DIR_CMAKE}"
    HYBRID_BINARY_ROOT_DIR="${HYBRID_BINARY_ROOT_DIR_CMAKE}")
target_compile_definitions(HybridRender PRIVATE
    HYBRID_PROJECT_ROOT_DIR="${HYBRID_PROJECT_ROOT_DIR_CMAKE}"
    HYBRID_BINARY_ROOT_DIR="${HYBRID_BINARY_ROOT_DIR_CMAKE}")
target_compile_definitions(HybridRHI_OpenGL PRIVATE
    HYBRID_PROJECT_ROOT_DIR="${HYBRID_PROJECT_ROOT_DIR_CMAKE}"
    HYBRID_BINARY_ROOT_DIR="${HYBRID_BINARY_ROOT_DIR_CMAKE}")
target_compile_definitions(HybridEngine PRIVATE
    HYBRID_BINARY_ROOT_DIR="${HYBRID_BINARY_ROOT_DIR_CMAKE}")
