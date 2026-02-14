set(third_party_folder "ThirdParty")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ---- glfw ----
if(NOT TARGET glfw)
    option(GLFW_BUILD_EXAMPLES "" OFF)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    option(GLFW_INSTALL "" OFF)

    add_subdirectory(glfw)
    set_target_properties(glfw PROPERTIES FOLDER ${third_party_folder}/glfw)

    if(TARGET update_mappings)
        set_target_properties(update_mappings PROPERTIES FOLDER ${third_party_folder}/glfw)
    endif()
endif()

# ---- glad (OpenGL loader) ----
if(NOT TARGET glad)
    include(${CMAKE_CURRENT_LIST_DIR}/glad.cmake)
    set_target_properties(glad PROPERTIES FOLDER ${third_party_folder}/glad)
endif()

# ---- glm (header-only) ----
if(NOT TARGET glm)
    add_library(glm INTERFACE)
    target_include_directories(glm INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/glm
    )
    set_target_properties(glm PROPERTIES FOLDER ${third_party_folder}/glm)
endif()

# ---- imgui ----
if(NOT TARGET imgui)
    include(${CMAKE_CURRENT_LIST_DIR}/imgui.cmake)
    set_target_properties(imgui PROPERTIES FOLDER ${third_party_folder}/imgui)
endif()

# ---- stb ----
if(NOT TARGET stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/stb
    )
    set_target_properties(stb PROPERTIES FOLDER ${third_party_folder}/stb)
endif()

# ---- spdlog ----
if(NOT TARGET spdlog::spdlog AND NOT TARGET spdlog::spdlog_header_only)
    option(SPDLOG_BUILD_EXAMPLE "" OFF)
    option(SPDLOG_BUILD_TESTS "" OFF)
    option(SPDLOG_INSTALL "" OFF)

    add_subdirectory(spdlog)

    if(TARGET spdlog)
        set_target_properties(spdlog PROPERTIES FOLDER ${third_party_folder}/spdlog)
    endif()
    if(TARGET spdlog_header_only)
        set_target_properties(spdlog_header_only PROPERTIES FOLDER ${third_party_folder}/spdlog)
    endif()
endif()

# ---- entt (submodule) ----
if(NOT TARGET EnTT::EnTT AND NOT TARGET entt)
    add_subdirectory(entt)
endif()

# VS 分组
if(TARGET entt)
    set_target_properties(entt PROPERTIES FOLDER ${third_party_folder}/entt)
endif()
