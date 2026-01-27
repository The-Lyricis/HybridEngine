set(TARGET_NAME TDA572Editor)

set(CMAKE_CXX_STANDARD 17)

file(GLOB EDITOR_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB EDITOR_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

# IDE 分组：按当前目录树展示
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${EDITOR_HEADERS} ${EDITOR_SOURCES})

add_executable(${TARGET_NAME} ${EDITOR_HEADERS} ${EDITOR_SOURCES})

# 让代码里能拿到“运行根目录”（顶层定义的 BINARY_ROOT_DIR）
# 注意：这里会变成一个字符串宏，比如 TDA572_ROOT_DIR=".../bin"
target_compile_definitions(${TARGET_NAME} PRIVATE TDA572_ROOT_DIR="${BINARY_ROOT_DIR}")

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    FOLDER "Engine"
)

# MSVC 下关闭“警告当错误
target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->")

target_link_libraries(${TARGET_NAME} PRIVATE TDA572Runtime)

target_include_directories(${TARGET_NAME}
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# -------------------------
# Post-build: 统一部署到 ${BINARY_ROOT_DIR}
# -------------------------
set(POST_BUILD_COMMANDS
    # 1) 确保 bin 目录存在
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"

    # 2) 把编译输出目录（exe/dll/pdb 等）复制到 bin
    #    这样可以直接从 ${BINARY_ROOT_DIR} 启动，不依赖 VS 的输出目录结构
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
            "${BINARY_ROOT_DIR}"

    # 3) 拷贝 editor 资源（如果有 engine/source/editor/resource）
    #    目标：${BINARY_ROOT_DIR}/resource
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resource"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resource"
            "${BINARY_ROOT_DIR}/resource"

    # 4) 拷贝引擎资产目录（engine/asset -> bin/asset）
    #    ENGINE_ASSET_DIR 在 engine 层设置为 "asset"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${BINARY_ROOT_DIR}/${ENGINE_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ENGINE_ROOT_DIR}/${ENGINE_ASSET_DIR}"
            "${BINARY_ROOT_DIR}/${ENGINE_ASSET_DIR}"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD ${POST_BUILD_COMMANDS})
