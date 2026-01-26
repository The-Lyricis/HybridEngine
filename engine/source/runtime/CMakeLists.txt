set(TARGET_NAME TDA572Runtime)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(${TARGET_NAME} PROPERTIES 
    CXX_STANDARD 17
    FOLDER "Engine"
)

# 链接依赖
target_link_libraries(${TARGET_NAME} PUBLIC glfw)
target_link_libraries(${TARGET_NAME} PUBLIC glm)
target_link_libraries(${TARGET_NAME} PUBLIC imgui)

# 包含目录
target_include_directories(${TARGET_NAME} 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)