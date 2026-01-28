set(ENGINE_ROOT_DIR "${PROJECT_ROOT_DIR}/engine")
set(THIRD_PARTY_DIR "${ENGINE_ROOT_DIR}/3rdparty")
set(ENGINE_ASSET_DIR "asset")

if(MSVC)
    add_compile_options("/MP")
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HybridEditor)
endif()

# Shaders 子目录 用到时再打开
#add_subdirectory(shader)

add_subdirectory(3rdparty)
add_subdirectory(source/runtime)
add_subdirectory(source/editor)