# engine/3rdparty/glad.cmake
set(glad_SOURCE_DIR_ ${CMAKE_CURRENT_LIST_DIR}/glad)

file(GLOB glad_c_sources CONFIGURE_DEPENDS
    "${glad_SOURCE_DIR_}/src/*.c"
)

add_library(glad STATIC ${glad_c_sources})

target_include_directories(glad PUBLIC
    $<BUILD_INTERFACE:${glad_SOURCE_DIR_}/include>
)

if(WIN32)
    target_link_libraries(glad PUBLIC opengl32)
endif()
