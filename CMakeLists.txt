cmake_minimum_required(VERSION 3.18)

# dmft5336 itself has no hardware port, but dmi2c (whose headers it uses)
# publishes them only inside its per-CPU-family packages - the headers are
# the same for every family, any published one will do.
set(DMOD_CPU_FAMILY "stm32f7" CACHE STRING "CPU family used to fetch the dmi2c headers")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED DMOD_MODULE_VERSION)
    set(DMOD_MODULE_VERSION "0.1" CACHE STRING "DMOD module version")
endif()

# If a local dmod checkout is provided, use it instead of fetching from git
if(DEFINED DMOD_DIR AND NOT "${DMOD_DIR}" STREQUAL "")
    set(FETCHCONTENT_SOURCE_DIR_DMOD "${DMOD_DIR}" CACHE PATH "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    dmod
    GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
    GIT_TAG        develop
)

set(DMOD_MODE "DMOD_MODULE" CACHE STRING "DMOD build mode")
set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")

FetchContent_MakeAvailable(dmod)

project(dmft5336 VERSION ${DMOD_MODULE_VERSION}
    DESCRIPTION "dmft5336 DMOD library module"
    LANGUAGES C CXX)

set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")
set(DMOD_SCRIPTS_DIR ${DMOD_DIR}/scripts CACHE PATH "DMOD scripts directory")
include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# Name of the module
set(DMOD_MODULE_NAME        dmft5336)

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Path to the .dmr file - enables release package creation in build/packages/
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/dmft5336.dmr)

# List of MAL interfaces implemented by this module (leave empty if none)
set(DMOD_MAL_IMPLS          )

# List of DIF interfaces implemented by this module (leave empty if none)
set(DMOD_DIF_IMPLS          )

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    src/dmft5336.c
    src/chip.c
)

# dmdrvi:  the driver interface dmft5336 implements (src/dmft5336.c).
# dmini:   .ini configuration.
# dmi2c:   headers only - the chip is reached through the dmi2c bus node
#          (Dmod_FileOpen/Dmod_Ioctl), using its message/transfer types and
#          ioctl numbers.
# dmhaman: the INT pin's dmgpio interrupt handler is dispatched by name.
# dmosi:   event semaphore, lock, sleep and tick count.
dmod_link_modules(${DMOD_MODULE_NAME}
    dmdrvi@>=2.0
    dmini
    dmi2c
    dmhaman
    dmosi
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_subdirectory(tests)

# ======================================================================
#               touchtest Tool
# ======================================================================
# On-target application - prints touches reported by the device node
# dmdevfs creates for a configured panel (see tools/touchtest/README.md).
# Not part of the ctest suite: it needs a finger on a real (or emulated) panel.
add_subdirectory(tools/touchtest)
