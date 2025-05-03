# ShaderMake library
add_library(ShaderMake STATIC
    src/argparse.c
    src/ShaderBlob.cpp
    src/Compiler.cpp
    src/Context.cpp
    include/ShaderMake/argparse.h
    include/ShaderMake/ShaderBlob.h
    include/ShaderMake/Timer.h
    include/ShaderMake/Compiler.h
    include/ShaderMake/Context.h
    include/ShaderMake/ShaderMake.h)

target_compile_options (ShaderMake PRIVATE ${COMPILE_OPTIONS})
target_include_directories(ShaderMake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderMake)
target_compile_definitions(ShaderMake PRIVATE SHADERMAKE_COLORS) # PRINTF Colors

# Set the output paths if SHADERMAKE_BIN_OUTPUT_PATH is specified.
# If not, use default CMake paths.
if (SHADERMAKE_BIN_OUTPUT_PATH)
    set_target_properties (ShaderMake PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY                "${SHADERMAKE_BIN_OUTPUT_PATH}/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${SHADERMAKE_BIN_OUTPUT_PATH}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${SHADERMAKE_BIN_OUTPUT_PATH}/MinSizeRel"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${SHADERMAKE_BIN_OUTPUT_PATH}/Release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${SHADERMAKE_BIN_OUTPUT_PATH}/RelWithDebInfo"
    )
endif()

# set_property (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ShaderMake)
set_property (TARGET ShaderMake PROPERTY FOLDER ShaderMake)

if (MSVC)
    target_compile_definitions (ShaderMake PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_link_options (ShaderMake PRIVATE "/DELAYLOAD:dxcompiler.dll")
    target_link_libraries (ShaderMake d3dcompiler dxcompiler delayimp)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR APPLE)
    target_link_libraries (ShaderMake pthread)
else ()
    target_link_libraries (ShaderMake stdc++fs pthread)
endif ()

if (SHADERMAKE_SEARCH_FOR_COMPILERS)
    # Finding FXC/DXC
    if (WIN32)
        if (SHADERMAKE_FIND_FXC OR SHADERMAKE_FIND_DXC)
            # On Windows - FXC and DXC are part of WindowsSDK and there's also DXC in VulkanSDK which supports SPIR-V
            if (DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
                set (WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
            elseif (DEFINED ENV{WindowsSDKLibVersion})
                string (REGEX REPLACE "\\\\$" "" WINDOWS_SDK_VERSION "$ENV{WindowsSDKLibVersion}")
            else ()
                message (FATAL_ERROR "WindowsSDK is not installed (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION is not defined; WindowsSDKLibVersion is '$ENV{WindowsSDKLibVersion}')!")
            endif ()

            get_filename_component (WINDOWS_SDK_ROOT "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
            set (WINDOWS_SDK_BIN "${WINDOWS_SDK_ROOT}/bin/${WINDOWS_SDK_VERSION}/x64")

            if (SHADERMAKE_FIND_FXC)
                find_program (FXC_PATH "${WINDOWS_SDK_BIN}/fxc")
                if (NOT FXC_PATH)
                    message (FATAL_ERROR "Can't find FXC in WindowsSDK: ${WINDOWS_SDK_BIN}")
                endif ()
            endif()

            if (SHADERMAKE_FIND_DXC)
                find_program (DXC_PATH "${WINDOWS_SDK_BIN}/dxc")
                if (NOT DXC_PATH)
                    message (FATAL_ERROR "Can't find DXC in WindowsSDK: ${WINDOWS_SDK_BIN}")
                endif ()
            endif()
        endif()

        if (SHADERMAKE_FIND_DXC_SPIRV)
            find_program (DXC_SPIRV_PATH "$ENV{VULKAN_SDK}/Bin/dxc")
        endif()
    else ()
        if (SHADERMAKE_FIND_DXC_SPIRV)
            # On Linux - VulkanSDK does not set VULKAN_SDK, but DXC can be called directly
            find_program (DXC_SPIRV_PATH "dxc")
        endif()
    endif ()

    if (SHADERMAKE_FIND_DXC_SPIRV AND NOT DXC_SPIRV_PATH)
        find_program (DXC_SPIRV_PATH "dxc" "${DXC_CUSTOM_PATH}")
    endif ()

    if (SHADERMAKE_FIND_DXC_SPIRV AND NOT DXC_SPIRV_PATH)
        message (FATAL_ERROR "Can't find DXC: Specify custom path using 'DXC_CUSTOM_PATH' parameter or install VulkanSDK!")
    endif ()

    if (SHADERMAKE_FIND_FXC)
        message (STATUS "Setting 'FXC_PATH' to '${FXC_PATH}'")
    endif()
    if (SHADERMAKE_FIND_DXC)
        message (STATUS "Setting 'DXC_PATH' to '${DXC_PATH}'")
    endif()
    if (SHADERMAKE_FIND_DXC_SPIRV)
        message (STATUS "Setting 'DXC_SPIRV_PATH' to '${DXC_SPIRV_PATH}'")
    endif()
endif()
