cmake_minimum_required(VERSION 3.18.4)
project(MemoryInstructionsProfiler)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
find_package(SDE REQUIRED)

add_library(MemoryInstructionsProfiler SHARED src/main.cpp)
target_link_libraries(MemoryInstructionsProfiler PRIVATE SDE::SDE)

# Testing
find_program(LIT NAMES llvm-lit lit lit.py)
find_program(FILECHECK NAMES FileCheck)

if(LIT AND FILECHECK)
    # For x86, we need to invoke gcc/g++ with '-m32'.
    # We also pass the current target architecture using
    # the PIN_TARGET_ARCH_... #defines.
    if(Pin_TARGET_ARCH STREQUAL "x64")
        set(TEST_COMPILER_ARGS "-DPIN_TARGET_ARCH_X64")
    else()
        set(TEST_COMPILER_ARGS "-m32 -DPIN_TARGET_ARCH_X86")
    endif()

    configure_file(test/lit.cfg.in test/lit.cfg)

    add_custom_target(check
        COMMAND ${LIT} -sv ${CMAKE_BINARY_DIR}/test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test
        USES_TERMINAL
        )

    add_dependencies(check MemoryInstructionsProfiler)
else()
    message(WARNING "'check' target disabled: lit and/or FileCheck was not found.")
endif()
