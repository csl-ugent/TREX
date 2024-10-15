cmake_minimum_required(VERSION 3.17.0)
project(GenericDeobfuscationPinTool)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
find_package(SDE REQUIRED)

add_library(GenericDeobfuscationPinTool SHARED src/InstructionTrace.cpp)
target_link_libraries(GenericDeobfuscationPinTool PRIVATE SDE::SDE)
