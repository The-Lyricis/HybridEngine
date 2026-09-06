set(ENGINE_ROOT_DIR "${PROJECT_ROOT_DIR}/engine")
set(THIRD_PARTY_DIR "${ENGINE_ROOT_DIR}/3rdparty")
set(ENGINE_ASSET_DIR "asset")

if(MSVC)
    add_compile_options("/MP")
endif()

# Shaders 子目录 用到时再打开
#add_subdirectory(shader)

add_subdirectory(3rdparty)
add_subdirectory(source/runtime)

if(HYBRID_BUILD_EDITOR)
    if(NOT WIN32)
        message(WARNING "HYBRID_BUILD_EDITOR is Windows-only; skipping HybridEditor")
    else()
        add_subdirectory(source/editor)
        if(MSVC)
            set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HybridEditor)
        endif()
    endif()
endif()

if(HYBRID_BUILD_PLAYER)
    add_subdirectory(source/player)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
