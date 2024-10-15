find_package(PkgConfig)
pkg_check_modules(PC_SDE QUIET SDE)

find_path(SDE_ROOT_DIR sde.sig)

set(SDE_VERSION ${PC_SDE_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDE
    FOUND_VAR SDE_FOUND
    REQUIRED_VARS
        SDE_ROOT_DIR
    VERSION_VAR SDE_VERSION
)

# SDE requires compilation with g++, not clang++ or other compilers.
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported. Use g++ instead.")
endif()

# Set the Pin architecture if not set by the user.
if (NOT Pin_TARGET_ARCH)
    message(STATUS "Pin architecture not specified. Using default 'x64'.")
    set(Pin_TARGET_ARCH x64)
endif()

# Check if Pin architecture is valid.
if (NOT (Pin_TARGET_ARCH STREQUAL "x64" OR Pin_TARGET_ARCH STREQUAL "x86"))
    message(FATAL_ERROR "Invalid Pin architecture ${Pin_TARGET_ARCH}. Valid values are: 'x86', 'x64'.")
endif()

# Create SDE target.
if(SDE_FOUND AND NOT TARGET SDE::SDE)
    # Include directories.
    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/source/include/pin")
    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/source/include/pin/gen")
    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/components/include")

    if (SDE_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/xed-intel64/include/xed")
    else()
        list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/xed-ia32/include/xed")
    endif()

    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/source/tools/Utils")
    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/source/tools/InstLib")
    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/sde-example/include")
    list(APPEND SDE_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/pinplay/include")

    # System include directories.
    list(APPEND SDE_SYSTEM_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/cxx/include")
    list(APPEND SDE_SYSTEM_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/crt/include")

    if (Pin_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_SYSTEM_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/crt/include/arch-x86_64")
    else()
        list(APPEND SDE_SYSTEM_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/crt/include/arch-x86")
    endif()

    list(APPEND SDE_SYSTEM_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/crt/include/kernel/uapi")
    list(APPEND SDE_SYSTEM_INCLUDE_DIRS "${SDE_ROOT_DIR}/pinkit/extras/crt/include/kernel/uapi/asm-x86")

    # Compile definitions.
    list(APPEND SDE_COMPILE_DEFINITIONS "__PIN__=1")
    list(APPEND SDE_COMPILE_DEFINITIONS "PIN_CRT=1")

    if (Pin_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_COMPILE_DEFINITIONS "TARGET_IA32E")
        list(APPEND SDE_COMPILE_DEFINITIONS "HOST_IA32E")
    else()
        list(APPEND SDE_COMPILE_DEFINITIONS "TARGET_IA32")
        list(APPEND SDE_COMPILE_DEFINITIONS "HOST_IA32")
    endif()

    list(APPEND SDE_COMPILE_DEFINITIONS "TARGET_LINUX")
    list(APPEND SDE_COMPILE_DEFINITIONS "SDE_INIT")
    list(APPEND SDE_COMPILE_DEFINITIONS "PINPLAY")

    # Compile options.
    list(APPEND SDE_COMPILE_OPTIONS "-Wall")
    list(APPEND SDE_COMPILE_OPTIONS "-Werror")
    list(APPEND SDE_COMPILE_OPTIONS "-Wno-unknown-pragmas")
    list(APPEND SDE_COMPILE_OPTIONS "-fno-stack-protector")
    list(APPEND SDE_COMPILE_OPTIONS "-fno-exceptions")
    list(APPEND SDE_COMPILE_OPTIONS "-funwind-tables")
    list(APPEND SDE_COMPILE_OPTIONS "-fasynchronous-unwind-tables")
    list(APPEND SDE_COMPILE_OPTIONS "-fno-rtti")
    list(APPEND SDE_COMPILE_OPTIONS "-fabi-version=2")
    list(APPEND SDE_COMPILE_OPTIONS "-faligned-new")
    list(APPEND SDE_COMPILE_OPTIONS "-O3")
    list(APPEND SDE_COMPILE_OPTIONS "-fomit-frame-pointer")
    list(APPEND SDE_COMPILE_OPTIONS "-fno-strict-aliasing")

    if (Pin_TARGET_ARCH STREQUAL "x86")
        list(APPEND SDE_COMPILE_OPTIONS "-m32")
    endif()

    # Link directories.
    if (Pin_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/intel64/runtime/pincrt")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/intel64/lib")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/intel64/lib-ext")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/extras/xed-intel64/lib")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/pinplay/intel64")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/sde-example/lib/intel64")
    else()
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/ia32/runtime/pincrt")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/ia32/lib")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/ia32/lib-ext")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/extras/xed-ia32/lib")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/pinplay/ia32")
        list(APPEND SDE_LINK_DIRECTORIES "${SDE_ROOT_DIR}/pinkit/sde-example/lib/ia32")
    endif()

    # Link libraries.
    if (Pin_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_LINK_LIBRARIES "${SDE_ROOT_DIR}/pinkit/intel64/runtime/pincrt/crtbeginS.o")
    else()
        list(APPEND SDE_LINK_LIBRARIES "${SDE_ROOT_DIR}/pinkit/ia32/runtime/pincrt/crtbeginS.o")
    endif()

    if (Pin_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_LINK_LIBRARIES "pinplay")
        list(APPEND SDE_LINK_LIBRARIES "sde")
        list(APPEND SDE_LINK_LIBRARIES "pinplay")
        list(APPEND SDE_LINK_LIBRARIES "sde")
        list(APPEND SDE_LINK_LIBRARIES "bz2")
        list(APPEND SDE_LINK_LIBRARIES "zlib")
    else()
        list(APPEND SDE_LINK_LIBRARIES "pinplay")
        list(APPEND SDE_LINK_LIBRARIES "sde")
        list(APPEND SDE_LINK_LIBRARIES "pinplay")
        list(APPEND SDE_LINK_LIBRARIES "sde")
        list(APPEND SDE_LINK_LIBRARIES "bz2")
        list(APPEND SDE_LINK_LIBRARIES "zlib")
    endif()

    list(APPEND SDE_LINK_LIBRARIES "pin")
    list(APPEND SDE_LINK_LIBRARIES "xed")

    if (Pin_TARGET_ARCH STREQUAL "x64")
        list(APPEND SDE_LINK_LIBRARIES "${SDE_ROOT_DIR}/pinkit/intel64/runtime/pincrt/crtendS.o")
    else()
        list(APPEND SDE_LINK_LIBRARIES "${SDE_ROOT_DIR}/pinkit/ia32/runtime/pincrt/crtendS.o")
    endif()

    list(APPEND SDE_LINK_LIBRARIES "pindwarf")
    list(APPEND SDE_LINK_LIBRARIES "dwarf")
    list(APPEND SDE_LINK_LIBRARIES "dl-dynamic")
    list(APPEND SDE_LINK_LIBRARIES "m-dynamic")
    list(APPEND SDE_LINK_LIBRARIES "c-dynamic")
    list(APPEND SDE_LINK_LIBRARIES "unwind-dynamic")
    list(APPEND SDE_LINK_LIBRARIES "c++")
    list(APPEND SDE_LINK_LIBRARIES "c++abi")

    # Link options.
    list(APPEND SDE_LINK_OPTIONS "-Wl,--hash-style=sysv")
    list(APPEND SDE_LINK_OPTIONS "-Wl,-Bsymbolic")
    list(APPEND SDE_LINK_OPTIONS "-Wl,--version-script=${SDE_ROOT_DIR}/pinkit/source/include/pin/pintool.ver")
    list(APPEND SDE_LINK_OPTIONS "-fabi-version=2")
    list(APPEND SDE_LINK_OPTIONS "-nostdlib")

    if (Pin_TARGET_ARCH STREQUAL "x86")
        list(APPEND SDE_LINK_OPTIONS "-m32")
    endif()

    # Create target.
    add_library(SDE::SDE INTERFACE IMPORTED)

    # Set options for target.
    target_include_directories(SDE::SDE INTERFACE "${SDE_INCLUDE_DIRS}")
    target_include_directories(SDE::SDE SYSTEM INTERFACE "${SDE_SYSTEM_INCLUDE_DIRS}")
    target_compile_definitions(SDE::SDE INTERFACE "${SDE_COMPILE_DEFINITIONS}")
    target_compile_options(SDE::SDE INTERFACE "${SDE_COMPILE_OPTIONS}")
    target_link_directories(SDE::SDE INTERFACE "${SDE_LINK_DIRECTORIES}")
    target_link_libraries(SDE::SDE INTERFACE "${SDE_LINK_LIBRARIES}")
    target_link_options(SDE::SDE INTERFACE "${SDE_LINK_OPTIONS}")
endif()

mark_as_advanced(
    SDE_ROOT_DIR
    Pin_TARGET_ARCH
)
