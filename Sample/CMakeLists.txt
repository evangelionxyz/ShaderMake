add_executable(Sample
    src/Sample.cpp
)

target_link_libraries(Sample PRIVATE ShaderMake)
target_include_directories(Sample PRIVATE ${SHADERMAKE_DIR}/include)

if(WIN32)
    target_compile_definitions(Sample PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()
