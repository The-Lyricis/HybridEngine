cmake_minimum_required(VERSION 3.20)

project(Hybrid VERSION 0.1.0)

option(HYBRID_BUILD_EDITOR "Build the Windows editor" ${WIN32})
option(HYBRID_BUILD_PLAYER "Build the standalone player" ON)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 禁止源内构建
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed!")
endif()

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# 让用户可通过 -DCMAKE_INSTALL_PREFIX=... 覆盖，默认放到 <repo>/bin
set(CMAKE_INSTALL_PREFIX "${PROJECT_ROOT_DIR}/bin" CACHE PATH "Install prefix" FORCE)
set(BINARY_ROOT_DIR "${CMAKE_INSTALL_PREFIX}")

add_subdirectory(engine)
