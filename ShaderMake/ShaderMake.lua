project "ShaderMake"
    kind "StaticLib"
    language "c++"
    cppdialect "c++17"

targetdir (OUTPUT_DIR)
objdir (INTOUTPUT_DIR)

files {
    "%{prj.location}/src/argparse.c",
    "%{prj.location}/src/Compiler.cpp",
    "%{prj.location}/src/Context.cpp",
    "%{prj.location}/src/ShaderBlob.cpp",

    "%{prj.location}/include/ShaderMake/argparse.h",
    "%{prj.location}/include/ShaderMake/Compiler.h",
    "%{prj.location}/include/ShaderMake/Context.h",
    "%{prj.location}/include/ShaderMake/ShaderBlob.h",
    "%{prj.location}/include/ShaderMake/ShaderMake.h",
    "%{prj.location}/include/ShaderMake/Timer.h",
}

includedirs {
    "%{prj.location}/src",
    "%{prj.location}/include/ShaderMake",
}

defines {
    "SHADERMAKE_COLORS"
}

filter "system:windows"
defines {
    "WIN32_LEAN_AND_MEAN",
    "NOMINMAX",
    "_CRT_SECURE_NO_WARNINGS"
}
links {
    "d3dcompiler", "dxcompiler", "delayimp"
}

filter "system:linux"
links {
    "pthread"
}


