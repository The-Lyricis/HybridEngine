cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME HybridEditor)

set(ENGINE_RESOURCES_DIR "${ENGINE_ROOT_DIR}/resources")
set(ENGINE_EDITOR_RESOURCES_DIR "${ENGINE_RESOURCES_DIR}/editor")
set(ENGINE_ASSET_RESOURCES_DIR "${ENGINE_RESOURCES_DIR}/assets")
set(EDITOR_RESOURCES_SOURCE_DIR "${ENGINE_EDITOR_RESOURCES_DIR}")

# ------------------------------------------------------------
# Sources：最新结构为 editor/ 下直接放 .cpp/.h，并且有 panels 子目录
# ------------------------------------------------------------
file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

set(HYBRID_IMGUI_OPENGL_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/render/imgui/opengl/imgui_render_backend_opengl.cpp")
list(REMOVE_ITEM EDITOR_SOURCES ${HYBRID_IMGUI_OPENGL_SOURCE})

if(APPLE)
    file(GLOB_RECURSE EDITOR_OBJCXX_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
    )
    list(APPEND EDITOR_SOURCES ${EDITOR_OBJCXX_SOURCES})
endif()

# Platform implementations are mutually exclusive. Keeping them out of the
# source list also makes accidental platform-header regressions fail early.
if(WIN32)
    list(FILTER EDITOR_HEADERS EXCLUDE REGEX "/platform/macos/")
    list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/platform/macos/")
elseif(APPLE)
    list(FILTER EDITOR_HEADERS EXCLUDE REGEX "/platform/windows/")
    list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/platform/windows/")
endif()

# ------------------------------------------------------------
# Piccolo 风格：显式把 .rc 资源编进可执行文件，给 exe 设定图标
# ------------------------------------------------------------
set(APP_ICON_RC  "${EDITOR_RESOURCES_SOURCE_DIR}/win/app_icon.rc")
set(APP_ICON_ICO "${EDITOR_RESOURCES_SOURCE_DIR}/win/app_icon.ico")
set(APP_ICON_ICNS "${EDITOR_RESOURCES_SOURCE_DIR}/macos/HybridEditor.icns")

if(WIN32 AND EXISTS "${APP_ICON_RC}" AND EXISTS "${APP_ICON_ICO}")
    set(EDITOR_RESOURCES "${APP_ICON_RC}")
else()
    set(EDITOR_RESOURCES "")
endif()

# macOS bundles do not inherit a process icon from GLFW. The .icns file must
# both be listed as a target source and explicitly placed in Contents/Resources.
if(APPLE AND EXISTS "${APP_ICON_ICNS}")
    list(APPEND EDITOR_RESOURCES "${APP_ICON_ICNS}")
    set_source_files_properties("${APP_ICON_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

set(EDITOR_ALL_FILES
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
    ${EDITOR_RESOURCES}
)

# IDE 分组：源码按 editor 目录树展示，外部资源单独归组
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${EDITOR_HEADERS} ${EDITOR_SOURCES})
if(EDITOR_RESOURCES)
    source_group("Resources" FILES ${EDITOR_RESOURCES})
endif()

add_executable(${TARGET_NAME} ${EDITOR_ALL_FILES})

add_library(HybridImGui_OpenGL STATIC ${HYBRID_IMGUI_OPENGL_SOURCE})
set_target_properties(HybridImGui_OpenGL PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "Engine/Editor")
target_include_directories(HybridImGui_OpenGL PUBLIC
    $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)
target_link_libraries(HybridImGui_OpenGL
    PUBLIC HybridEngine imgui
    PRIVATE glad glfw)

target_compile_definitions(${TARGET_NAME} PRIVATE
    HYBRID_ROOT_DIR="${BINARY_ROOT_DIR}"
    HYBRID_EDITOR_RESOURCES_DIR="${BINARY_ROOT_DIR}/resources/editor"
    HYBRID_ENGINE_ASSETS_DIR="${BINARY_ROOT_DIR}/resources/assets"
)

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    FOLDER "Engine"
)

# MSVC 下关闭“警告当错误”
target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")

target_link_libraries(${TARGET_NAME} PRIVATE
    HybridEngine HybridImGui_OpenGL imgui stb tinyobjloader nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE Ole32 Shell32)
elseif(APPLE)
    find_library(HYBRID_COCOA_FRAMEWORK Cocoa REQUIRED)
    target_link_libraries(${TARGET_NAME} PRIVATE "${HYBRID_COCOA_FRAMEWORK}")
    set_target_properties(${TARGET_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        RUNTIME_OUTPUT_DIRECTORY "${BINARY_ROOT_DIR}"
        MACOSX_BUNDLE_BUNDLE_NAME "Hybrid Editor"
        MACOSX_BUNDLE_ICON_FILE "HybridEditor.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.hybridengine.editor"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# 关键更新：include 根目录改为 editor 目录本身（包含 panels 子目录）
target_include_directories(${TARGET_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>
)

# -------------------------
# Post-build：统一部署到 ${BINARY_ROOT_DIR}
# -------------------------

# 1) bin 目录 + 拷贝输出
if(NOT APPLE)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
                "${BINARY_ROOT_DIR}"
    )
endif()

# 2) 拷贝 editor resources
if(EXISTS "${EDITOR_RESOURCES_SOURCE_DIR}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/editor"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${EDITOR_RESOURCES_SOURCE_DIR}"
                "${BINARY_ROOT_DIR}/resources/editor"
    )
endif()

# 3) 拷贝 engine built-in assets（仅当存在）
if(EXISTS "${ENGINE_ASSET_RESOURCES_DIR}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_ASSET_RESOURCES_DIR}"
                "${BINARY_ROOT_DIR}/resources/assets"
    )
endif()

# 4) 拷贝 engine/shader（仅当存在）
if(EXISTS "${ENGINE_ROOT_DIR}/shader")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${BINARY_ROOT_DIR}/shader"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_ROOT_DIR}/shader"
                "${BINARY_ROOT_DIR}/shader"
    )
endif()

# ---- ImGuizmo（Editor-only）----
target_include_directories(${TARGET_NAME} PRIVATE
    ${ENGINE_ROOT_DIR}/3rdparty/ImGuizmo
)

target_sources(${TARGET_NAME} PRIVATE
    ${ENGINE_ROOT_DIR}/3rdparty/ImGuizmo/ImGuizmo.cpp
)
