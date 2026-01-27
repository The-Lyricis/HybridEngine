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

# ---- imgui ----
if(NOT TARGET imgui)
    include(${CMAKE_CURRENT_LIST_DIR}/imgui.cmake)
    set_target_properties(imgui PROPERTIES FOLDER ${third_party_folder}/imgui)
endif()

