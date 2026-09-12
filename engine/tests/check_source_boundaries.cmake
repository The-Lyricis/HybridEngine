if(NOT DEFINED HYBRID_SOURCE_ROOT)
    message(FATAL_ERROR "HYBRID_SOURCE_ROOT is required")
endif()

file(GLOB_RECURSE HYBRID_BOUNDARY_SOURCES
    "${HYBRID_SOURCE_ROOT}/*.h"
    "${HYBRID_SOURCE_ROOT}/*.hpp"
    "${HYBRID_SOURCE_ROOT}/*.cpp"
    "${HYBRID_SOURCE_ROOT}/*.cxx"
    "${HYBRID_SOURCE_ROOT}/*.mm")

set(HYBRID_BOUNDARY_ERRORS)
foreach(source IN LISTS HYBRID_BOUNDARY_SOURCES)
    file(READ "${source}" contents)
    file(TO_CMAKE_PATH "${source}" normalized_source)

    string(REGEX MATCH "GLFWwindow|#[ \t]*include[ \t]*[<\"]GLFW/" has_glfw "${contents}")
    if(has_glfw AND
       NOT normalized_source MATCHES "/runtime/platform/glfw/" AND
       NOT normalized_source MATCHES "/editor/render/imgui/")
        list(APPEND HYBRID_BOUNDARY_ERRORS "GLFW leak: ${normalized_source}")
    endif()

    string(REGEX MATCH "#[ \t]*include[ \t]*[<\"]glad/|(^|[^A-Za-z0-9_])gl[A-Z][A-Za-z0-9_]*[ \t]*\\(|(^|[^A-Za-z0-9_])GL_[A-Z0-9_]+" has_opengl "${contents}")
    if(has_opengl AND
       NOT normalized_source MATCHES "/runtime/modules/render/backend/opengl/" AND
       NOT normalized_source MATCHES "/editor/render/imgui/opengl/")
        list(APPEND HYBRID_BOUNDARY_ERRORS "OpenGL leak: ${normalized_source}")
    endif()

    string(REGEX MATCH "get(Color|Depth)AttachmentRendererID|m_[A-Za-z0-9_]*RendererID" has_native_renderer_id "${contents}")
    if(has_native_renderer_id AND
       NOT normalized_source MATCHES "/runtime/modules/render/backend/opengl/")
        list(APPEND HYBRID_BOUNDARY_ERRORS "Native renderer ID leak: ${normalized_source}")
    endif()
endforeach()

if(HYBRID_BOUNDARY_ERRORS)
    list(JOIN HYBRID_BOUNDARY_ERRORS "\n" formatted_errors)
    message(FATAL_ERROR "Source boundary check failed:\n${formatted_errors}")
endif()

message(STATUS "Source boundary check passed")
