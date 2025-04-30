#define SHADERMAKE_COLORS
#include <ShaderMake/ShaderMake.h>

#include <assert.h>

using namespace ShaderMake;

int main(int argc, char **argv)
{
    // ShaderMake.exe - p DXIL --binary - O3 - c "Shader.cfg" - o "dxil" --compiler "%VULKAN_SDK%\Bin\dxc.exe" --colorize

    // Init timer
    Timer timer;
    Options options;

#if 0
    options.platformType = PlatformType_DXIL;
    options.compilerType = CompilerType_DXC;
    options.optimizationLevel = 3;
    options.outputDir = "resources/shaders/bin/dxil";
    options.configFile = "resources/shaders/Shader.cfg";
#else
    options.platformType = PlatformType_SPIRV;
    options.compilerType = CompilerType_DXC;
    options.optimizationLevel = 3;
    options.outputDir = "resources/shaders/bin/spirv";
    options.configFile = "resources/shaders/Shader.cfg";
#endif

    Context ctx(&options);

    {
        // Gather shader permutations
        assert(std::filesystem::exists(ctx.options->configFile), "Could not found the file %s", ctx.options->configFile);

        std::filesystem::file_time_type configTime = std::filesystem::last_write_time(ctx.options->configFile);
        configTime = std::max(configTime, std::filesystem::last_write_time(argv[0]));

        std::ifstream configStream(ctx.options->configFile);

        std::string line;
        line.reserve(256);

        std::vector<bool> blocks;
        blocks.push_back(true);

        for (uint32_t lineIndex = 0; getline(configStream, line); lineIndex++)
        {
            Utils::TrimConfigLine(line);

            // Skip an empty or commented line
            if (line.empty() || line[0] == '\n' || (line[0] == '/' && line[1] == '/'))
                continue;

            // TODO: preprocessor supports "#ifdef MACRO / #if 1 / #if 0", "#else" and "#endif"
            size_t pos = line.find("#ifdef");
            if (pos != std::string::npos)
            {
                pos += 6;
                pos += line.substr(pos).find_first_not_of(' ');

                std::string define = line.substr(pos);
                bool state = blocks.back() && find(ctx.options->defines.begin(), ctx.options->defines.end(), define) != ctx.options->defines.end();

                blocks.push_back(state);
            }
            else if (line.find("#if 1") != std::string::npos)
                blocks.push_back(blocks.back());
            else if (line.find("#if 0") != std::string::npos)
                blocks.push_back(false);
            else if (line.find("#endif") != std::string::npos)
            {
                if (blocks.size() == 1)
                    Utils::Printf(RED "%s(%u,0): ERROR: Unexpected '#endif'!\n", Utils::PathToString(ctx.options->configFile).c_str(), lineIndex + 1);
                else
                    blocks.pop_back();
            }
            else if (line.find("#else") != std::string::npos)
            {
                if (blocks.size() < 2)
                    Utils::Printf(RED "%s(%u,0): ERROR: Unexpected '#else'!\n", Utils::PathToString(ctx.options->configFile).c_str(), lineIndex + 1);
                else if (blocks[blocks.size() - 2])
                    blocks.back() = !blocks.back();
            }
            else if (blocks.back())
            {
                if (!ctx.ExpandPermutations(lineIndex, line, configTime))
                    return 1;
            }
        }
    }

    // Process tasks
    if (!ctx.tasks.empty())
    {
        Compiler compiler(&ctx);

        Utils::Printf(WHITE "Using compiler: %s\n", ctx.options->compilerPath.generic_string().c_str());

        ctx.originalTaskCount = (uint32_t)ctx.tasks.size();
        ctx.processedTaskCount = 0;
        ctx.failedTaskCount = 0;

        // Retry limit for compilation task sub-process failures that can occur when threading
        ctx.taskRetryCount = ctx.options->retryCount;

        compiler.ExeCompile();

#if 0
        uint32_t threadsNum = std::max(ctx.options->serial ? 1u : uint32_t(std::thread::hardware_concurrency()), 1u);
        std::vector<std::thread> threads(threadsNum);
        for (uint32_t i = 0; i < threadsNum; i++)
        {
            if (!ctx.options->useAPI)
                threads[i] = std::thread([&]()
                {
                    compiler.ExeCompile();
                });
#ifdef WIN32
            else if (ctx.options->platform == Platform_DXBC)
                threads[i] = std::thread(FxcCompile);
            else
                threads[i] = std::thread(DxcCompile);
#endif
        }

        for (uint32_t i = 0; i < threadsNum; i++)
            threads[i].join();
#endif

        // Dump shader blobs
        for (const auto &[blobName, blobEntries] : ctx.shaderBlobs)
        {
            // If a blob would contain one entry with no defines, just skip it:
            // the individual file's output name is the same as the blob, and we're done here.
            if (blobEntries.size() == 1 && blobEntries[0].combinedDefines.empty())
                continue;

            // Validate that the blob doesn't contain any shaders with empty defines.
            // In such case, that individual shader's output file is the same as the blob output file, which wouldn't work.
            // We could detect this condition earlier and work around it by renaming the shader output file, if necessary.
            bool invalidEntry = false;
            for (const auto &entry : blobEntries)
            {
                if (entry.combinedDefines.empty())
                {
                    const std::string blobBaseName = std::filesystem::path(blobName).stem().generic_string();
                    Utils::Printf(RED "ERROR: Cannot create a blob for shader %s where some permutation(s) have no definitions!",
                        blobBaseName.c_str());
                    invalidEntry = true;
                    break;
                }
            }

            if (invalidEntry)
            {
                if (ctx.options->continueOnError)
                    continue;

                return 1;
            }

            if (ctx.options->binaryBlob)
            {
                bool result = ctx.CreateBlob(blobName, blobEntries, false);
                if (!result && !ctx.options->continueOnError)
                    return 1;
            }

            if (ctx.options->headerBlob)
            {
                bool result = ctx.CreateBlob(blobName, blobEntries, true);
                if (!result && !ctx.options->continueOnError)
                    return 1;
            }

            if (!ctx.options->binary)
                ctx.RemoveIntermediateBlobFiles(blobEntries);
        }

        // Report failed tasks
        if (ctx.failedTaskCount)
            Utils::Printf(YELLOW "WARNING: %u task(s) failed to complete!\n", ctx.failedTaskCount.load());
        else
            Utils::Printf(WHITE "%d task(s) completed successfully.\n", ctx.originalTaskCount);

        Utils::Printf(WHITE "Elapsed time %.2f ms\n", timer.GetElapsedTime());
    }
    else
    {
        Utils::Printf(WHITE "All %s shaders are up to date.\n", Utils::PlatformToString(ctx.options->platformType));
    }

    return (ctx.terminate || ctx.failedTaskCount > 0) ? 1 : 0;
}