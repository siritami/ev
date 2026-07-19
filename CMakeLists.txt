cmake_minimum_required(VERSION 3.15)
project(EVKey LANGUAGES C RC)

# =============================================================================
# EVKey - Vietnamese Keyboard Input Method Editor (Open Source)
#
# Open-source reimplementation based on reverse engineering of EVKey64.exe.
# Pure C implementation using Win32 API (no MFC dependency).
#
# Usage:
#   mkdir build && cd build
#   cmake .. -G "Visual Studio 17 2022" -A x64
#   cmake --build . --config Release
# =============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source directory
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# All source files
set(SOURCES
    ${SRC_DIR}/evk_main.c
    ${SRC_DIR}/evk_hook.c
    ${SRC_DIR}/evk_input.c
    ${SRC_DIR}/evk_charset.c
    ${SRC_DIR}/evk_settings.c
    ${SRC_DIR}/evk_tray.c
    ${SRC_DIR}/evk_dialog.c
    ${SRC_DIR}/evk_macro.c
    ${SRC_DIR}/evk_util.c
)

set(HEADERS
    ${SRC_DIR}/evk.h
    ${SRC_DIR}/evk_table.h
)

set(RESOURCES
    ${SRC_DIR}/evk.rc
)

# Common Windows libraries
set(COMMON_LIBS
    user32
    gdi32
    kernel32
    shell32
    comctl32
    comdlg32
    ole32
    oleaut32
    advapi32
    shlwapi
    imm32
    winmm
    version
)

# =============================================================================
# MSVC-specific settings
# =============================================================================
if(MSVC)
    add_compile_options(
        /W0                 # Suppress all warnings (decompiled code)
        /EHsc               # Standard C++ exception handling
        /GS-                # Disable security checks for decompiled code
        /GF                 # String pooling
        /O2                 # Optimize for speed
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        WIN32
        _WINDOWS
        NDEBUG
        _UNICODE
        UNICODE
    )
endif()

# =============================================================================
# MinGW / GCC settings
# =============================================================================
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR MINGW)
    add_compile_options(
        -w                  # Suppress all warnings
        -O2
        -msse2
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        WIN32
        _WINDOWS
        NDEBUG
        _UNICODE
        UNICODE
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_compile_definitions(WIN64 _WIN64)
    endif()
    link_libraries(${COMMON_LIBS})
endif()

# =============================================================================
# 64-bit Build Target
# =============================================================================
add_executable(EVKey64 WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)
target_include_directories(EVKey64 PRIVATE ${SRC_DIR})
target_link_libraries(EVKey64 PRIVATE ${COMMON_LIBS})
set_target_properties(EVKey64 PROPERTIES
    OUTPUT_NAME "EVKey64"
    SUFFIX ".exe"
)

# Note: 32-bit build (EVKey32.c) removed per user request.
# Only the 64-bit target above is built.
