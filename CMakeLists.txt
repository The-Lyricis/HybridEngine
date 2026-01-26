cmake_minimum_required(VERSION 3.20)

project(TDA572 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 禁止源内构建
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed!")
endif()

set(ENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/engine")
set(CMAKE_INSTALL_PREFIX "${ENGINE_ROOT_DIR}/../bin")

add_subdirectory(engine)