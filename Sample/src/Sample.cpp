#define SHADERMAKE_COLORS
#include <ShaderMake/ShaderMake.h>

#include <assert.h>
#include <initializer_list>
#include <iostream>

using namespace ShaderMake;

int main(int argc, char **argv)
{
    Options options;
    options.compilerType = CompilerType_DXC;
    options.optimizationLevel = 3;
    options.baseDirectory = "resources/shaders/";
    options.outputDir = "bin";
    options.verbose = false;
    options.platformType = PlatformType_DXIL;

    Context ctx(&options);

    if (!ctx.terminate)
    {
        ShaderContextDesc shaderDesc = ShaderContextDesc();
        bool forceRecompile = false;
        std::shared_ptr<ShaderContext> shaderA = std::make_shared<ShaderContext>("imgui.vertex.hlsl", ShaderType::Vertex, shaderDesc, forceRecompile);
        std::shared_ptr<ShaderContext> shaderB = std::make_shared<ShaderContext>("imgui.pixel.hlsl", ShaderType::Pixel, shaderDesc, false);
        std::shared_ptr<ShaderContext> shaderC = std::make_shared<ShaderContext>("test.vertex.hlsl", ShaderType::Vertex, shaderDesc, forceRecompile);
        std::shared_ptr<ShaderContext> shaderD = std::make_shared<ShaderContext>("test.pixel.hlsl", ShaderType::Pixel, shaderDesc, false);
        std::shared_ptr<ShaderContext> shaderE = std::make_shared<ShaderContext>("default.vertex.hlsl", ShaderType::Vertex, shaderDesc, forceRecompile);
        std::shared_ptr<ShaderContext> shaderF = std::make_shared<ShaderContext>("default.pixel.hlsl", ShaderType::Pixel, shaderDesc, false);
        std::shared_ptr<ShaderContext> shaderG = std::make_shared<ShaderContext>("default_2d.vertex.hlsl", ShaderType::Vertex, shaderDesc, false);
        std::shared_ptr<ShaderContext> shaderH = std::make_shared<ShaderContext>("default_2d.pixel.hlsl", ShaderType::Pixel, shaderDesc, forceRecompile);
    
        CompileStatus status = ctx.CompileShader({ shaderA, shaderB, shaderC, shaderD, shaderE, shaderG, shaderF, shaderH });
    
        // compile with .cfg file
        // TODO: get shader compilation result (blob)
    #if 0
        ctx.CompileConfigFile("Shader.cfg");
    #endif
    }

    return (ctx.terminate || ctx.failedTaskCount > 0) ? 1 : 0;
}
