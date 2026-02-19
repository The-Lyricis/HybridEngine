cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME HybridEditor)

# 建议：不要在子目录里 set 全局 CMAKE_CXX_STANDARD（会影响全工程）
# 但你原本就这么做了，这里保留，同时也在 target 上显式设置一次以确保一致
set(CMAKE_CXX_STANDARD 17)

# ------------------------------------------------------------
# Sources: 最新结构为 editor/ 下直接放 .cpp/.h，且有 panels 子目录
# ------------------------------------------------------------
file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

# ------------------------------------------------------------
# Piccolo 风格：显式把 .rc 资源编进可执行文件，给 exe 设定图标
# 资源目录在 editor/resource
# ------------------------------------------------------------
set(APP_ICON_RC  "${CMAKE_CURRENT_SOURCE_DIR}/resource/app_icon.rc")
set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resource/app_icon.ico")

if(WIN32 AND EXISTS "${APP_ICON_RC}" AND EXISTS "${APP_ICON_ICO}")
    set(EDITOR_RESOURCES "${APP_ICON_RC}")
else()
    set(EDITOR_RESOURCES "")
endif()

set(EDITOR_ALL_FILES
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
    ${EDITOR_RESOURCES}
)

# IDE 分组：按当前目录树展示
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${EDITOR_ALL_FILES})

add_executable(${TARGET_NAME} ${EDITOR_ALL_FILES})

target_compile_definitions(${TARGET_NAME} PRIVATE
    HYBRID_ROOT_DIR="${BINARY_ROOT_DIR}"
)

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    FOLDER "Engine"
)

# MSVC 下关闭“警告当错误”
target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")

target_link_libraries(${TARGET_NAME} PRIVATE HybridRuntime)

# 关键更新：include 根目录改为 editor 目录本身（包含 panels 子目录）
target_include_directories(${TARGET_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# -------------------------
# Post-build: 统一部署到 ${BINARY_ROOT_DIR}
# -------------------------

# 1) bin 目录 + 拷贝输出
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
            "${BINARY_ROOT_DIR}"
)

# 2) 拷贝 editor/resource（仅当存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resource"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/resource"
                "${BINARY_ROOT_DIR}/resource"
    )
endif()

# 3) 拷贝 engine/asset（仅当存在）
if(EXISTS "${ENGINE_ROOT_DIR}/asset")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${BINARY_ROOT_DIR}/asset"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_ROOT_DIR}/asset"
                "${BINARY_ROOT_DIR}/asset"
    )
endif()

# 4)（可选但通常需要）拷贝 engine/shader（仅当存在）
if(EXISTS "${ENGINE_ROOT_DIR}/shader")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${BINARY_ROOT_DIR}/shader"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_ROOT_DIR}/shader"
                "${BINARY_ROOT_DIR}/shader"
    )
endif()
