# =====================================================================
#               touchtest - Manual dmft5336 Device Test Tool
# =====================================================================
# Added via add_subdirectory() from the top-level CMakeLists.txt, so it
# shares dmft5336's own dmod fetch/configuration - same pattern as
# dmspi/tools/spitest.

# Path to the DMR file for this module - produces a ready-to-publish
# release package under build/packages/touchtest alongside dmft5336.
set(DMOD_DMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/touchtest.dmr)

dmod_add_executable(touchtest ${DMOD_MODULE_VERSION}
    main.c
)

# The panel is used only through Dmod_FileOpen/_FileRead/_Ioctl (dmod SAL);
# dmdrvi: headers only - the standard input state/info types and
# DMDRVI_IOCTL_INPUT_* (2.3+). dmft5336_if: the FT5336 chip info ioctl.
dmod_link_modules(touchtest
    dmdrvi@>=2.3
)

target_link_libraries(touchtest dmft5336_if)
