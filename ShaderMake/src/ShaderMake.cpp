/*
Copyright (c) 2014-2025, NVIDIA CORPORATION. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "ShaderMake.h"

int32_t main(int32_t argc, const char** argv)
{
    // Init timer

    Timer_Init();
    uint64_t start = Timer_GetTicks();

    // Set signal handler
    signal(SIGINT, SignalHandler);
#ifdef _WIN32
    signal(SIGBREAK, SignalHandler);
#endif

    // Parse command line
    const char* self = argv[0];
    if (!options.Parse(argc, argv))
        return 1;

    // Set envvar
    char envBuf[1024];
    if (!options.useAPI)
    {
        #ifdef _WIN32 // workaround for Windows
            snprintf(envBuf, sizeof(envBuf), "COMPILER=\"%s\"", options.compiler);
        #else
            snprintf(envBuf, sizeof(envBuf), "COMPILER=%s", options.compiler);
        #endif

        if (putenv(envBuf) != 0)
            return 1;
    }

#ifdef _WIN32
    // Setup a directory where to look for the compiler first
    std::filesystem::path compilerPath = std::filesystem::path(options.compiler).parent_path();
    SetDllDirectoryA(compilerPath.string().c_str());
    // This will still leave the launch folder as the first entry in the search path, so
    // try to explicitly load the appropriate DLL from the correct path
    if (options.useAPI)
    {
        char const* dllName = nullptr;
        switch (options.platform)
        {
        case DXIL:
        case SPIRV:
            dllName = "dxcompiler.dll";
            break;
        case DXBC:
            dllName = "d3dcompiler_47.dll";
            break;
        default:
            break;
        }
        if (dllName != nullptr)
        {
            std::filesystem::path dllPath = compilerPath / dllName;
            if (LoadLibraryA(dllPath.string().c_str()) == NULL)
            {
                Printf(RED "ERROR: Failed to load compiler dll: \"%s\"!\n", dllPath.string().c_str());
                return 1;
            }
        }
    }
#endif

    { // Gather shader permutations
        std::filesystem::file_time_type configTime = std::filesystem::last_write_time(options.configFile);
        configTime = max(configTime, std::filesystem::last_write_time(self));

        ifstream configStream(options.configFile);

        string line;
        line.reserve(256);

        vector<bool> blocks;
        blocks.push_back(true);

        for (uint32_t lineIndex = 0; getline(configStream, line); lineIndex++)
        {
            TrimConfigLine(line);

            // Skip an empty or commented line
            if (line.empty() || line[0] == '\n' || (line[0] == '/' && line[1] == '/'))
                continue;

            // TODO: preprocessor supports "#ifdef MACRO / #if 1 / #if 0", "#else" and "#endif"
            size_t pos = line.find("#ifdef");
            if (pos != string::npos)
            {
                pos += 6;
                pos += line.substr(pos).find_first_not_of(' ');

                string define = line.substr(pos);
                bool state = blocks.back() && find(options.defines.begin(), options.defines.end(), define) != options.defines.end();

                blocks.push_back(state);
            }
            else if (line.find("#if 1") != string::npos)
                blocks.push_back(blocks.back());
            else if (line.find("#if 0") != string::npos)
                blocks.push_back(false);
            else if (line.find("#endif") != string::npos)
            {
                if (blocks.size() == 1)
                    Printf(RED "%s(%u,0): ERROR: Unexpected '#endif'!\n", PathToString(options.configFile).c_str(), lineIndex + 1);
                else
                    blocks.pop_back();
            }
            else if (line.find("#else") != string::npos)
            {
                if (blocks.size() < 2)
                    Printf(RED "%s(%u,0): ERROR: Unexpected '#else'!\n", PathToString(options.configFile).c_str(), lineIndex + 1);
                else if (blocks[blocks.size() - 2])
                    blocks.back() = !blocks.back();
            }
            else if (blocks.back())
            {
                if (!ExpandPermutations(lineIndex, line, configTime))
                    return 1;
            }
        }
    }

    // Process tasks
    if (!g_TaskData.empty())
    {
        Printf(WHITE "Using compiler: %s\n", options.compiler);

        g_OriginalTaskCount = (uint32_t)g_TaskData.size();
        g_ProcessedTaskCount = 0;
        g_FailedTaskCount = 0;

        // Retry limit for compilation task sub-process failures that can occur when threading
        g_TaskRetryCount = options.retryCount;

        uint32_t threadsNum = max(options.serial ? 1 : thread::hardware_concurrency(), 1u);

        vector<thread> threads(threadsNum);
        for (uint32_t i = 0; i < threadsNum; i++)
        {
            if (!options.useAPI)
                threads[i] = thread(ExeCompile);
#ifdef WIN32
            else if (options.platform == DXBC)
                threads[i] = thread(FxcCompile);
            else
                threads[i] = thread(DxcCompile);
#endif
        }

        for (uint32_t i = 0; i < threadsNum; i++)
            threads[i].join();

        // If a fatal error or a termination request happened, don't proceed to the blob building.
        if (g_Terminate)
            return 1;

        // Dump shader blobs
        for (const auto& [blobName, blobEntries] : g_ShaderBlobs)
        {
            // If a blob would contain one entry with no defines, just skip it:
            // the individual file's output name is the same as the blob, and we're done here.
            if (blobEntries.size() == 1 && blobEntries[0].combinedDefines.empty())
                continue;

            // Validate that the blob doesn't contain any shaders with empty defines.
            // In such case, that individual shader's output file is the same as the blob output file, which wouldn't work.
            // We could detect this condition earlier and work around it by renaming the shader output file, if necessary.
            bool invalidEntry = false;
            for (const auto& entry : blobEntries)
            {
                if (entry.combinedDefines.empty())
                {
                    const string blobBaseName = std::filesystem::path(blobName).stem().generic_string();
                    Printf(RED "ERROR: Cannot create a blob for shader %s where some permutation(s) have no definitions!",
                        blobBaseName.c_str());
                    invalidEntry = true;
                    break;
                }
            }

            if (invalidEntry)
            {
                if (options.continueOnError)
                    continue;

                return 1;
            }

            if (options.binaryBlob)
            {
                bool result = CreateBlob(blobName, blobEntries, false);
                if (!result && !options.continueOnError)
                    return 1;
            }

            if (options.headerBlob)
            {
                bool result = CreateBlob(blobName, blobEntries, true);
                if (!result && !options.continueOnError)
                    return 1;
            }

            if (!options.binary)
                RemoveIntermediateBlobFiles(blobEntries);
        }

        // Report failed tasks
        if (g_FailedTaskCount)
            Printf(YELLOW "WARNING: %u task(s) failed to complete!\n", g_FailedTaskCount.load());
        else
            Printf(WHITE "%d task(s) completed successfully.\n", g_OriginalTaskCount);

        uint64_t end = Timer_GetTicks();
        Printf(WHITE "Elapsed time %.2f ms\n", Timer_ConvertTicksToMilliseconds(end - start));
    }
    else
        Printf(WHITE "All %s shaders are up to date.\n", options.platformName);

    return (g_Terminate || g_FailedTaskCount) ? 1 : 0;
}
