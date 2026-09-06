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
hybrid_collect(HYBRID_RENDER_FILES modules/render modules/window platform)
hybrid_collect(HYBRID_ENGINE_FILES runtime)

foreach(group_files IN ITEMS HYBRID_CORE_FILES HYBRID_RUNTIME_FILES HYBRID_RENDER_FILES HYBRID_ENGINE_FILES)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${${group_files}})
endforeach()

add_library(HybridCore STATIC ${HYBRID_CORE_FILES})
add_library(HybridRuntime STATIC ${HYBRID_RUNTIME_FILES})
add_library(HybridRender STATIC ${HYBRID_RENDER_FILES})
add_library(HybridEngine STATIC ${HYBRID_ENGINE_FILES})

foreach(target IN ITEMS HybridCore HybridRuntime HybridRender HybridEngine)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "Engine")
    target_compile_options(${target} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)
endforeach()

target_link_libraries(HybridCore PUBLIC glm spdlog::spdlog)
target_link_libraries(HybridRuntime
    PUBLIC HybridCore glm EnTT::EnTT
    PRIVATE stb tinyobjloader nlohmann_json::nlohmann_json)
target_link_libraries(HybridRender
    PUBLIC HybridRuntime glfw glm
    PRIVATE glad stb nlohmann_json::nlohmann_json)
target_link_libraries(HybridEngine PUBLIC HybridRender)

target_compile_definitions(HybridRuntime PRIVATE
    HYBRID_PROJECT_ROOT_DIR="${HYBRID_PROJECT_ROOT_DIR_CMAKE}"
    HYBRID_BINARY_ROOT_DIR="${HYBRID_BINARY_ROOT_DIR_CMAKE}")
target_compile_definitions(HybridRender PRIVATE
    HYBRID_PROJECT_ROOT_DIR="${HYBRID_PROJECT_ROOT_DIR_CMAKE}"
    HYBRID_BINARY_ROOT_DIR="${HYBRID_BINARY_ROOT_DIR_CMAKE}")
