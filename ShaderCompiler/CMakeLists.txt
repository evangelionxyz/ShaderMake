add_executable(ShaderCompiler
    src/ShaderCompiler.cpp
)

target_link_libraries(ShaderCompiler PRIVATE ShaderMake)
target_include_directories(ShaderCompiler PRIVATE ${SHADERMAKE_DIR}/include)

if(WIN32)
    target_compile_definitions(ShaderCompiler PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()
