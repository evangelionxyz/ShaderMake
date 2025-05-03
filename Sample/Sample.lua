project "Sample"
    kind "ConsoleApp"
    language "c++"
    cppdialect "c++20"

targetdir (OUTPUT_DIR)
objdir (INTOUTPUT_DIR)

files {
    "%{prj.location}/src/**.cpp",
    "%{prj.location}/src/**.h",
}

includedirs {
    "%{prj.location}/src",
    "%{wks.location}/ShaderMake/include",
}

links {
    "ShaderMake"
}

filter "system:windows"
defines {
    "WIN32_LEAN_AND_MEAN",
    "NOMINMAX",
    "_CRT_SECURE_NO_WARNINGS"
}

filter "configurations:Debug"
runtime "Debug"
symbols "on"

filter "configurations:Release"
runtime "Release"
symbols "off"
