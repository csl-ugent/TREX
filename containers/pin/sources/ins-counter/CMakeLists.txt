cmake_minimum_required(VERSION 3.17.0)
project(InstructionCounterPinTool)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
find_package(SDE REQUIRED)

add_library(InstructionCounterPinTool SHARED src/counter.cpp)
target_link_libraries(InstructionCounterPinTool PRIVATE SDE::SDE)
