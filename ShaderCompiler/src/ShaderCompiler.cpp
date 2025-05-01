#define SHADERMAKE_COLORS
#include <ShaderMake/ShaderMake.h>
#include <assert.h>

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
    options.configFile = "Shader.cfg";
    options.forceCompile = false; // recompile if the binary already exists
#if 0
    options.platformType = PlatformType_DXIL;
#else
    options.platformType = PlatformType_SPIRV;
#endif

    Context ctx(&options);
    SMResult result = ctx.Compile();
    assert(result == SMResult_Success);

    return (ctx.terminate || ctx.failedTaskCount > 0) ? 1 : 0;
}