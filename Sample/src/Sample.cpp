#define SHADERMAKE_COLORS
#include <ShaderMake/ShaderMake.h>

#include <assert.h>
#include <initializer_list>

using namespace ShaderMake;

int main(int argc, char **argv)
{
    /*
        DXIL         : ShaderMake.exe --platform DXIL --binary -O3 -c "Shader.cfg" -o "bin" --compiler "%VULKAN_SDK%\Bin\dxc.exe"  --tRegShift 0  --sRegShift 128  --bRegShift 256 --uRegShift 384 --useAPI
        VULKAN_SPIRV : ShaderMake.exe --platform SPIRV --binary -O3 -c "Shader.cfg" -o "bin" --compiler "%VULKAN_SDK%\Bin\dxc.exe"  --tRegShift 0  --sRegShift 128  --bRegShift 256 --uRegShift 384 --useAPI -D SPIRV -D TARGET_VULKAN
    */

    // the sift define is default
    Options options;

    options.compilerType = CompilerType_DXC;
    options.optimizationLevel = 3;
    options.baseDirectory = "resources/shaders/";
    options.outputDir = "bin";
#if 0
    options.platformType = PlatformType_DXIL;
#else
    options.platformType = PlatformType_SPIRV;
#endif

    Context ctx(&options);

    ShaderContextDesc shaderDesc = ShaderContextDesc();
    bool forceRecompile = true;
    std::shared_ptr<ShaderContext> vertexShader = std::make_shared<ShaderContext>("imgui.vertex.hlsl", ShaderType::Vertex, shaderDesc, forceRecompile);
    std::shared_ptr<ShaderContext> pixelShader = std::make_shared<ShaderContext>("imgui.pixel.hlsl", ShaderType::Pixel, shaderDesc, forceRecompile);

    CompileStatus status = ctx.CompileShader({ vertexShader, pixelShader });

    //ctx.CompileConfigFile("Shader.cfg");

    return (ctx.terminate || ctx.failedTaskCount > 0) ? 1 : 0;
}