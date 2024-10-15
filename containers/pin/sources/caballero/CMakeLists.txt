cmake_minimum_required(VERSION 3.17.0)
project(CaballeroPinTool)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
find_package(SDE REQUIRED)

add_library(CaballeroPinTool SHARED src/caballero.cpp)
target_link_libraries(CaballeroPinTool PRIVATE SDE::SDE)
