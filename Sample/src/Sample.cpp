#define SHADERMAKE_COLORS
#include <ShaderMake/ShaderMake.h>

#include <assert.h>
#include <initializer_list>

using namespace ShaderMake;

#define COMPILE_DXIL 1

int main(int argc, char **argv)
{
    Options options;
    options.compilerType = CompilerType_DXC;
    options.optimizationLevel = 3;
    options.baseDirectory = "resources/shaders/";
    options.outputDir = "bin";

#if COMPILE_DXIL
    options.platformType = PlatformType_DXIL;
#else
    options.platformType = PlatformType_SPIRV;
#endif

    Context ctx(&options);

    ShaderContextDesc shaderDesc = ShaderContextDesc();
    bool forceRecompile = true;
    std::shared_ptr<ShaderContext> imguiVertexShader = std::make_shared<ShaderContext>("imgui.vertex.hlsl", ShaderType::Vertex, shaderDesc, forceRecompile);
    std::shared_ptr<ShaderContext> imguiPixelShader = std::make_shared<ShaderContext>("imgui.pixel.hlsl", ShaderType::Pixel, shaderDesc, forceRecompile);
    std::shared_ptr<ShaderContext> vertexShader = std::make_shared<ShaderContext>("test.vertex.hlsl", ShaderType::Vertex, shaderDesc, forceRecompile);
    std::shared_ptr<ShaderContext> pixelShader = std::make_shared<ShaderContext>("test.pixel.hlsl", ShaderType::Pixel, shaderDesc, forceRecompile);
    CompileStatus status = ctx.CompileOrGetShader({ imguiVertexShader, imguiPixelShader, vertexShader, pixelShader });

    // compile with .cfg file
    // TODO: get shader compilation result (blob)
#if 0
    ctx.CompileConfigFile("Shader.cfg");
#endif

    return (ctx.terminate || ctx.failedTaskCount > 0) ? 1 : 0;
}