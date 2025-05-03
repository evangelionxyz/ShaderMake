workspace "ShaderMake"
architecture "x64"
configurations {
    "Debug",
    "Release"
}

flags { 
    "MultiProcessorCompile"
}

BUILD_DIR = "%{wks.location}/bin"

OUTPUT_DIR = "%{BUILD_DIR}/%{cfg.buildcfg}"
INTOUTPUT_DIR = "%{wks.location}/bin-int/%{prj.name}"


include "Sample/Sample.lua"
include "ShaderMake/ShaderMake.lua"
