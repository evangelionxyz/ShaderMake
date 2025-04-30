cmake_minimum_required (VERSION 3.15)

# Is submodule?
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    set (SHADERMAKE_IS_SUBMODULE OFF)
else ()
    set (SHADERMAKE_IS_SUBMODULE ON)
endif ()

# Cached
if (SHADERMAKE_IS_SUBMODULE)
    set (SHADERMAKE_BIN_OUTPUT_PATH "" CACHE STRING "")
else()
    set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "")
    set (SHADERMAKE_BIN_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin" CACHE STRING "")
endif()

set (SHADERMAKE_SEARCH_FOR_COMPILERS ${SHADERMAKE_IS_SUBMODULE} CACHE BOOL "Toggles whether to search for dxc.exe and fxc.exe")
option (SHADERMAKE_FIND_FXC "Toggles whether to search for FXC" ON)
option (SHADERMAKE_FIND_DXC "Toggles whether to search for DXC for DXIL" ON)
option (SHADERMAKE_FIND_DXC_SPIRV "Toggles whether to search for DXC for SPIR-V" ON)

project (ShaderMake LANGUAGES C CXX)

# Globals?
set_property (GLOBAL PROPERTY USE_FOLDERS ON)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_C_STANDARD 99)

if (MSVC)
    # Test the Windows SDK version to make sure that we can compile ShaderMake.
    # ShaderMake uses some relatively new APIs for DXC, and there's no straightforward way to test
    # if they're declared in the SDK headers. So, test it here to prevent obscure errors.
    set(_MIN_WINDOWS_SDK_VERSION_BUILD 20348)
    string(REGEX MATCH "^10\\.0\\.([0-9]+)\\.[0-9]+$" _WINDOWS_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    if (_WINDOWS_SDK_VERSION)
        set(_WINDOWS_SDK_VERSION_BUILD "${CMAKE_MATCH_1}")
        if (_WINDOWS_SDK_VERSION_BUILD LESS _MIN_WINDOWS_SDK_VERSION_BUILD)
            message(SEND_ERROR "ShaderMake requires Windows SDK version at least 10.0.${_MIN_WINDOWS_SDK_VERSION_BUILD}.0.")
        endif()
    else()
        message(WARNING "ShaderMake: Unknown Windows SDK version '${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}', errors may occur during build.")
    endif()
endif()

# Compile options
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
  set (SIMD -msse4.1)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set (COMPILE_OPTIONS ${SIMD} -Wextra)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set (COMPILE_OPTIONS ${SIMD} -Wextra)
elseif (MSVC)
    set (COMPILE_OPTIONS /W4 /WX /wd4324)
else ()
    message (WARNING "Unknown compiler!")
endif ()

add_subdirectory(ShaderMake)
add_subdirectory(ShaderCompiler)