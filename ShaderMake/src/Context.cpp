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

#include "Context.h"
#include "argparse.h"

#include "ShaderBlob.h"

#ifdef _WIN32
#   include <windows.h>
#else
#endif
#include <list>
#include <regex>

namespace ShaderMake {

namespace ArgsUtils {

int AddInclude(struct argparse *self, const struct argparse_option *option)
{
    ((Options *)(option->data))->includeDirs.push_back(*(const char **)option->value);
    UNUSED(self);
    return 0;
}

int AddGlobalDefine(struct argparse *self, const struct argparse_option *option)
{
    ((Options *)(option->data))->defines.push_back(*(const char **)option->value);
    UNUSED(self);
    return 0;
}

int AddRelaxedInclude(struct argparse *self, const struct argparse_option *option)
{
    ((Options *)(option->data))->relaxedIncludes.push_back(*(const char **)option->value);
    UNUSED(self);
    return 0;
}

int AddSpirvExtension(struct argparse *self, const struct argparse_option *option)
{
    ((Options *)(option->data))->spirvExtensions.push_back(*(const char **)option->value);
    UNUSED(self);
    return 0;
}

int AddCompilerOptions(struct argparse *self, const struct argparse_option *option)
{
    ((Options *)(option->data))->compilerOptions.push_back(*(const char **)option->value);
    UNUSED(self);
    return 0;
}

int AddLocalDefine(struct argparse *self, const struct argparse_option *option)
{
    ((ConfigLine *)(option->data))->defines.push_back(*(const char **)option->value);
    UNUSED(self);
    return 0;
}

}

bool Options::Parse(int32_t argc, const char **argv)
{
    const char *config = nullptr;
    const char *unused = nullptr; // storage for callbacks
    const char *srcDir = "";
    bool ignoreConfigDir = false;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Required options:"),
            OPT_STRING('p', "platform", &platformName, "DXBC, DXIL or SPIRV", nullptr, 0, 0),
            OPT_STRING('c', "config", &config, "Configuration file with the list of shaders to compile", nullptr, 0, 0),
            OPT_STRING('o', "out", &outputDir, "Output directory", nullptr, 0, 0),
            OPT_BOOLEAN('b', "binary", &binary, "Output binary files", nullptr, 0, 0),
            OPT_BOOLEAN('h', "header", &header, "Output header files", nullptr, 0, 0),
            OPT_BOOLEAN('B', "binaryBlob", &binaryBlob, "Output binary blob files", nullptr, 0, 0),
            OPT_BOOLEAN('H', "headerBlob", &headerBlob, "Output header blob files", nullptr, 0, 0),
            OPT_STRING(0, "compiler", &compiler, "Path to a FXC/DXC/Slang compiler executable", nullptr, 0, 0),
        OPT_GROUP("Compiler settings:"),
            OPT_STRING('m', "shaderModel", &shaderModel, "Shader model for DXIL/SPIRV (always SM 5.0 for DXBC) in 'X_Y' format", nullptr, 0, 0),
            OPT_INTEGER('O', "optimization", &optimizationLevel, "Optimization level 0-3 (default = 3, disabled = 0)", nullptr, 0, 0),
            OPT_STRING('X', "compilerOptions", &unused, "Custom command line options for the compiler, separated by spaces", ArgsUtils::AddCompilerOptions, (intptr_t)this, 0),
            OPT_BOOLEAN(0, "WX", &warningsAreErrors, "Maps to '-WX' DXC/FXC option: warnings are errors", nullptr, 0, 0),
            OPT_BOOLEAN(0, "allResourcesBound", &allResourcesBound, "Maps to '-all_resources_bound' DXC/FXC option: all resources bound", nullptr, 0, 0),
            OPT_BOOLEAN(0, "PDB", &pdb, "Output PDB files in 'out/PDB/' folder", nullptr, 0, 0),
            OPT_BOOLEAN(0, "embedPDB", &embedPdb, "Embed PDB with the shader binary", nullptr, 0, 0),
            OPT_BOOLEAN(0, "stripReflection", &stripReflection, "Maps to '-Qstrip_reflect' DXC/FXC option: strip reflection information from a shader binary", nullptr, 0, 0),
            OPT_BOOLEAN(0, "matrixRowMajor", &matrixRowMajor, "Maps to '-Zpr' DXC/FXC option: pack matrices in row-major order", nullptr, 0, 0),
            OPT_BOOLEAN(0, "hlsl2021", &hlsl2021, "Maps to '-HV 2021' DXC option: enable HLSL 2021 standard", nullptr, 0, 0),
            OPT_BOOLEAN(0, "slang", &slang, "Compiler is Slang", nullptr, 0, 0),
            OPT_BOOLEAN(0, "slangHLSL", &slangHlsl, "Use HLSL compatibility mode when compiler is Slang", nullptr, 0, 0),
        OPT_GROUP("Defines & include directories:"),
            OPT_STRING('I', "include", &unused, "Include directory(s)", ArgsUtils::AddInclude, (intptr_t)this, 0),
            OPT_STRING('D', "define", &unused, "Macro definition(s) in forms 'M=value' or 'M'", ArgsUtils::AddGlobalDefine, (intptr_t)this, 0),
        OPT_GROUP("Other options:"),
            OPT_BOOLEAN('f', "force", &force, "Treat all source files as modified", nullptr, 0, 0),
            OPT_STRING(0, "sourceDir", &srcDir, "Source code directory", nullptr, 0, 0),
            OPT_STRING(0, "relaxedInclude", &unused, "Include file(s) not invoking re-compilation", ArgsUtils::AddRelaxedInclude, (intptr_t)this, 0),
            OPT_STRING(0, "outputExt", &outputExt, "Extension for output files, default is one of .dxbc, .dxil, .spirv", nullptr, 0, 0),
            OPT_BOOLEAN(0, "serial", &serial, "Disable multi-threading", nullptr, 0, 0),
            OPT_BOOLEAN(0, "flatten", &flatten, "Flatten source directory structure in the output directory", nullptr, 0, 0),
            OPT_BOOLEAN(0, "continue", &continueOnError, "Continue compilation if an error is occured", nullptr, 0, 0),
            OPT_BOOLEAN(0, "useAPI", &useAPI, "Use FXC (d3dcompiler) or DXC (dxcompiler) API explicitly (Windows only)", nullptr, 0, 0),
            OPT_BOOLEAN(0, "colorize", &colorize, "Colorize console output", nullptr, 0, 0),
            OPT_BOOLEAN(0, "verbose", &verbose, "Print commands before they are executed", nullptr, 0, 0),
            OPT_INTEGER(0, "retryCount", &retryCount, "Retry count for compilation task sub-process failures", nullptr, 0, 0),
            OPT_BOOLEAN(0, "ignoreConfigDir", &ignoreConfigDir, "Use 'current dir' instead of 'config dir' as parent path for relative dirs", nullptr, 0, 0),
        OPT_GROUP("SPIRV options:"),
            OPT_STRING(0, "vulkanMemoryLayout", &vulkanMemoryLayout, "Maps to '-fvk-use-<VALUE>-layout' DXC options: dx, gl, scalar", nullptr, 0, 0),
            OPT_STRING(0, "vulkanVersion", &vulkanVersion, "Vulkan environment version, maps to '-fspv-target-env' (default = 1.3)", nullptr, 0, 0),
            OPT_STRING(0, "spirvExt", &unused, "Maps to '-fspv-extension' option: add SPIR-V extension permitted to use", ArgsUtils::AddSpirvExtension, (intptr_t)this, 0),
            OPT_INTEGER(0, "sRegShift", &sRegShift, "SPIRV: register shift for sampler (s#) resources", nullptr, 0, 0),
            OPT_INTEGER(0, "tRegShift", &tRegShift, "SPIRV: register shift for texture (t#) resources", nullptr, 0, 0),
            OPT_INTEGER(0, "bRegShift", &bRegShift, "SPIRV: register shift for constant (b#) resources", nullptr, 0, 0),
            OPT_INTEGER(0, "uRegShift", &uRegShift, "SPIRV: register shift for UAV (u#) resources", nullptr, 0, 0),
            OPT_BOOLEAN(0, "noRegShifts", &noRegShifts, "Don't specify any register shifts for the compiler", nullptr, 0, 0),
        OPT_END(),
    };

    static const char *usages[] = {
        "ShaderMake.exe -p {DXBC|DXIL|SPIRV} --binary [--header --blob] -c \"path/to/config\"\n"
        "\t-o \"path/to/output\" --compiler \"path/to/compiler\" [other options]\n"
        "\t-D DEF1 -D DEF2=1 ... -I \"path1\" -I \"path2\" ...",
        nullptr
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, nullptr, "\nMulti-threaded shader compiling & processing tool");
    argparse_parse(&argparse, argc, argv);

#ifndef _WIN32
    useAPI = false;
#endif

    if (!config)
    {
        Utils::Printf(RED "ERROR: Config file not specified!\n");
        return false;
    }

    if (!std::filesystem::exists(config))
    {
        Utils::Printf(RED "ERROR: Config file '%s' does not exist!\n", config);
        return false;
    }

    if (!outputDir)
    {
        Utils::Printf(RED "ERROR: Output directory not specified!\n");
        return false;
    }

    if (!binary && !header && !binaryBlob && !headerBlob)
    {
        Utils::Printf(RED "ERROR: One of 'binary', 'header', 'binaryBlob' or 'headerBlob' must be set!\n");
        return false;
    }
    if (!platformName)
    {
        Utils::Printf(RED "ERROR: Platform not specified!\n");
        return false;
    }

    if (!compiler)
    {
        Utils::Printf(RED "ERROR: Compiler not specified!\n");
        return false;
    }

    if (!std::filesystem::exists(compiler))
    {
        Utils::Printf(RED "ERROR: Compiler '%s' does not exist!\n", compiler);
        return false;
    }

    if (slang && useAPI)
    {
        Utils::Printf(RED "ERROR: Use of Slang with --useAPI is not implemented.\n");
        return false;
    }

    if (strlen(shaderModel) != 3 || strstr(shaderModel, "."))
    {
        Utils::Printf(RED "ERROR: Shader model ('%s') must have format 'X_Y'!\n", shaderModel);
        return false;
    }

    // Platform
    uint32_t i = 0;
    for (; i < Platform_NUM; i++)
    {
        if (!strcmp(platformName, PlatformNames[i]))
        {
            platform = (Platform)i;
            break;
        }
    }
    if (i == Platform_NUM)
    {
        Utils::Printf(RED "ERROR: Unrecognized platform '%s'!\n", platformName);
        return false;
    }

    if (outputExt)
        outputExt = outputExt;
    else
        outputExt = PlatformExts[platform];

    if (vulkanMemoryLayout && platform != Platform_SPIRV)
    {
        Utils::Printf(RED "ERROR: --vulkanMemoryLayout is only supported for SPIRV target!\n");
        return false;
    }

    if (vulkanMemoryLayout &&
        strcmp(vulkanMemoryLayout, "dx") != 0 &&
        strcmp(vulkanMemoryLayout, "gl") != 0 &&
        strcmp(vulkanMemoryLayout, "scalar") != 0)
    {
        if (slang && (strcmp(vulkanMemoryLayout, "dx") == 0))
        {
            Utils::Printf(RED "ERROR: Unsupported value '%s' for --vulkanMemoryLayout! Only 'gl' and 'scalar' are supported for Slang.\n",
                vulkanMemoryLayout);
        }
        else
        {
            Utils::Printf(RED "ERROR: Unsupported value '%s' for --vulkanMemoryLayout! Only 'dx', 'gl' and 'scalar' are supported.\n",
                vulkanMemoryLayout);
        }
        return false;
    }

    if (!compilerOptions.empty() && useAPI && platform == Platform_DXBC)
    {
        Utils::Printf(RED "ERROR: --compilerOptions is not compatible with '--platform DXBC --useAPI'!\n");
        return false;
    }

    if (retryCount < 0)
    {
        Utils::Printf(RED "ERROR: --retryCount must be greater than or equal to 0.\n");
        return false;
    }

    // Absolute path is needed for source files to get "clickable" messages
#ifdef _WIN32
    char cd[MAX_PATH];
    if (!GetCurrentDirectoryA(sizeof(cd), cd))
#else
    char cd[PATH_MAX];
    if (!getcwd(cd, sizeof(cd)))
#endif
    {
        Utils::Printf(RED "ERROR: Cannot get the working directory!\n");
        return false;
    }

    configFile = std::filesystem::path(cd) / std::filesystem::path(config);

    std::filesystem::path fsSrcDir = srcDir;
    if (fsSrcDir.is_relative())
    {
        if (ignoreConfigDir)
            sourceDir = std::filesystem::path(cd) / fsSrcDir;
        else
            sourceDir = configFile.parent_path() / fsSrcDir;
    }
    else
        sourceDir = fsSrcDir;

    for (std::filesystem::path &path : includeDirs)
    {
        if (path.is_relative())
        {
            if (ignoreConfigDir)
                path = std::filesystem::path(cd) / path;
            else
                path = configFile.parent_path() / path;
        }
    }

    return true;
}

bool Context::GetHierarchicalUpdateTime(const std::filesystem::path &file, std::list<std::filesystem::path> &callStack, std::filesystem::file_time_type &outTime)
{
    static const std::basic_regex<char> includePattern("\\s*#include\\s+[\"<]([^>\"]+)[>\"].*");

    auto found = hierarchicalUpdateTimes.find(file);
    if (found != hierarchicalUpdateTimes.end())
    {
        outTime = found->second;

        return true;
    }

    std::ifstream stream(file);
    if (!stream.is_open())
    {
        Utils::Printf(RED "ERROR: Can't open file '%s', included in:\n", Utils::PathToString(file).c_str());
        for (const std::filesystem::path &otherFile : callStack)
            Utils::Printf(RED "\t%s\n", Utils::PathToString(otherFile).c_str());

        return false;
    }

    callStack.push_front(file);

    std::filesystem::path path = file.parent_path();
    std::filesystem::file_time_type hierarchicalUpdateTime = std::filesystem::last_write_time(file);

    for (std::string line; std::getline(stream, line);)
    {
        std::match_results<const char *> matchResult;
        std::regex_match(line.c_str(), matchResult, includePattern);
        if (matchResult.empty())
            continue;

        std::filesystem::path includeName = std::string(matchResult[1]);
        if (std::find(options.relaxedIncludes.begin(), options.relaxedIncludes.end(), includeName) != options.relaxedIncludes.end())
            continue;

        bool isFound = false;
        std::filesystem::path includeFile = path / includeName;
        if (std::filesystem::exists(includeFile))
            isFound = true;
        else
        {
            for (const std::filesystem::path &includePath : options.includeDirs)
            {
                includeFile = includePath / includeName;
                if (std::filesystem::exists(includeFile))
                {
                    isFound = true;
                    break;
                }
            }
        }

        if (!isFound)
        {
            Utils::Printf(RED "ERROR: Can't find include file '%s', included in:\n", Utils::PathToString(includeName).c_str());
            for (const std::filesystem::path &otherFile : callStack)
                Utils::Printf(RED "\t%s\n", Utils::PathToString(otherFile).c_str());

            return false;
        }

        std::filesystem::file_time_type dependencyTime;
        if (!GetHierarchicalUpdateTime(includeFile, callStack, dependencyTime))
            return false;

        hierarchicalUpdateTime = max(dependencyTime, hierarchicalUpdateTime);
    }

    callStack.pop_front();

    hierarchicalUpdateTimes[file] = hierarchicalUpdateTime;
    outTime = hierarchicalUpdateTime;

    return true;
}

std::string Context::GetShaderName(const std::filesystem::path &path)
{
    std::string name = path.filename().string();
    replace(name.begin(), name.end(), '.', '_');
    name += "_" + std::string(PlatformExts[options.platform] + 1);

    return "g_" + name;
}

void Context::DumpShader(const TaskData &taskData, const uint8_t *data, size_t dataSize)
{
    std::string file = taskData.outputFileWithoutExt + outputExt;

    if (options.binary || options.binaryBlob || (options.headerBlob && !taskData.combinedDefines.empty()))
    {
        DataOutputContext context(this, file.c_str(), false);
        if (!context.stream)
            return;

        context.WriteDataAsBinary(data, dataSize);
    }

    if (options.header || (options.headerBlob && taskData.combinedDefines.empty()))
    {
        DataOutputContext context(this, (file + ".h").c_str(), true);
        if (!context.stream)
            return;

        std::string shaderName = GetShaderName(taskData.outputFileWithoutExt);
        context.WriteTextPreamble(shaderName.c_str(), taskData.combinedDefines);
        context.WriteDataAsText(data, dataSize);
        context.WriteTextEpilog();
    }
}

bool Context::ProcessConfigLine(uint32_t lineIndex, const std::string &line, const std::filesystem::file_time_type &configTime)
{
    // Tokenize
    std::string lineCopy = line;
    std::vector<const char *> tokens;
    Utils::TokenizeConfigLine((char *)lineCopy.c_str(), tokens);

    // Parse config line
    ConfigLine configLine;
    if (!configLine.Parse((int32_t)tokens.size(), tokens.data(), options))
    {
        Utils::Printf(RED "%s(%u,0): ERROR: Can't parse config line!\n", Utils::PathToString(options.configFile).c_str(), lineIndex + 1);

        return false;
    }

    // DXBC: skip unsupported profiles
    std::string profile = configLine.profile;
    if (options.platform == Platform_DXBC && (profile == "lib" || profile == "ms" || profile == "as"))
        return true;

    // Getting the sorted index of defines. While doing this, the value of defines are also get included in sorting problem
    // but it doesn't matter until two defines keys are identical which is not the case.
    std::vector<size_t> definesSortedIndices = GetSortedConstantsIndices(configLine.defines);

    // Concatenate define strings, i.e. to get something, like: "A=1 B=0 C"
    std::string combinedDefines = "";
    for (size_t i = 0; i < configLine.defines.size(); i++)
    {
        size_t sortedIndex = definesSortedIndices[i];
        combinedDefines += configLine.defines[sortedIndex];
        if (i != configLine.defines.size() - 1)
            combinedDefines += " ";
    }

    // Compiled shader name
    std::filesystem::path shaderName = Utils::RemoveLeadingDotDots(configLine.source);
    shaderName.replace_extension("");
    if (options.flatten || configLine.outputDir) // Specifying -o <path> for a shader removes the original path
        shaderName = shaderName.filename();
    if (strcmp(configLine.entryPoint, "main"))
        shaderName += "_" + std::string(configLine.entryPoint);
    if (configLine.outputSuffix)
        shaderName += std::string(configLine.outputSuffix);

    // Compiled permutation name
    std::filesystem::path permutationName = shaderName;
    if (!configLine.defines.empty())
    {
        uint32_t permutationHash = Utils::HashToUint(std::hash<std::string>()(combinedDefines));

        char buf[16];
        snprintf(buf, sizeof(buf), "_%08X", permutationHash);

        permutationName += buf;
    }

    // Output directory
    std::filesystem::path outputDir = options.outputDir;
    if (configLine.outputDir)
        outputDir /= configLine.outputDir;

    // Create intermediate output directories
    bool force = options.force;
    std::filesystem::path endPath = outputDir / shaderName.parent_path();
    if (options.pdb)
        endPath /= PDB_DIR;
    if (endPath.string() != "" && !std::filesystem::exists(endPath))
    {
        std::filesystem::create_directories(endPath);
        force = true;
    }

    // Early out if no changes detected
    std::filesystem::file_time_type zero; // constructor sets to 0
    std::filesystem::file_time_type outputTime = zero;

    {
        std::filesystem::path outputFile = outputDir / permutationName;

        outputFile += this->outputExt;
        if (options.binary)
        {
            force |= !std::filesystem::exists(outputFile);
            if (!force)
            {
                if (outputTime == zero)
                    outputTime = std::filesystem::last_write_time(outputFile);
                else
                    outputTime = min(outputTime, std::filesystem::last_write_time(outputFile));
            }
        }

        outputFile += ".h";
        if (options.header)
        {
            force |= !std::filesystem::exists(outputFile);
            if (!force)
            {
                if (outputTime == zero)
                    outputTime = std::filesystem::last_write_time(outputFile);
                else
                    outputTime = min(outputTime, std::filesystem::last_write_time(outputFile));
            }
        }
    }

    {
        std::filesystem::path outputFile = outputDir / shaderName;

        outputFile += this->outputExt;
        if (options.binaryBlob)
        {
            force |= !std::filesystem::exists(outputFile);
            if (!force)
            {
                if (outputTime == zero)
                    outputTime = std::filesystem::last_write_time(outputFile);
                else
                    outputTime = min(outputTime, std::filesystem::last_write_time(outputFile));
            }
        }

        outputFile += ".h";
        if (options.headerBlob)
        {
            force |= !std::filesystem::exists(outputFile);
            if (!force)
            {
                if (outputTime == zero)
                    outputTime = std::filesystem::last_write_time(outputFile);
                else
                    outputTime = min(outputTime, std::filesystem::last_write_time(outputFile));
            }
        }
    }

    if (!force)
    {
        std::list<std::filesystem::path> callStack;
        std::filesystem::file_time_type sourceTime;
        std::filesystem::path sourceFile = options.sourceDir / configLine.source;
        if (!GetHierarchicalUpdateTime(sourceFile, callStack, sourceTime))
            return false;

        sourceTime = max(sourceTime, configTime);
        if (outputTime > sourceTime)
            return true;
    }

    // Prepare a task
    std::string outputFileWithoutExt = Utils::PathToString(outputDir / permutationName);
    uint32_t optimizationLevel = configLine.optimizationLevel == USE_GLOBAL_OPTIMIZATION_LEVEL ? options.optimizationLevel : configLine.optimizationLevel;
    optimizationLevel = std::min(optimizationLevel, 3u);

    TaskData &taskData = tasks.emplace_back();
    taskData.source = configLine.source;
    taskData.entryPoint = configLine.entryPoint;
    taskData.profile = configLine.profile;
    taskData.shaderModel = configLine.shaderModel;
    taskData.combinedDefines = combinedDefines;
    taskData.outputFileWithoutExt = outputFileWithoutExt;
    taskData.defines = configLine.defines;
    taskData.optimizationLevel = optimizationLevel;

    // Gather blobs
    if (options.IsBlob())
    {
        std::string blobName = Utils::PathToString(outputDir / shaderName);
        std::vector<BlobEntry> &entries = this->shaderBlobs[blobName];

        BlobEntry entry;
        entry.permutationFileWithoutExt = outputFileWithoutExt;
        entry.combinedDefines = combinedDefines;
        entries.push_back(entry);
    }

    return true;
}


bool Context::ExpandPermutations(uint32_t lineIndex, const std::string &line, const std::filesystem::file_time_type &configTime)
{
    size_t opening = line.find('{');
    if (opening == std::string::npos)
        return ProcessConfigLine(lineIndex, line, configTime);

    size_t closing = line.find('}', opening);
    if (closing == std::string::npos)
    {
        Utils::Printf(RED "%s(%u,0): ERROR: Missing '}'!\n", Utils::PathToString(options.configFile).c_str(), lineIndex + 1);

        return false;
    }

    size_t current = opening + 1;
    while (true)
    {
        size_t comma = line.find(',', current);
        if (comma == std::string::npos || comma > closing)
            comma = closing;

        std::string newConfig = line.substr(0, opening) + line.substr(current, comma - current) + line.substr(closing + 1);
        if (!ExpandPermutations(lineIndex, newConfig, configTime))
            return false;

        current = comma + 1;
        if (comma >= closing)
            break;
    }

    return true;
}

bool Context::CreateBlob(const std::string &blobName, const std::vector<BlobEntry> &entries, bool useTextOutput)
{
    // Create output file
    std::string outputFile = blobName;
    outputFile += outputExt;
    if (useTextOutput)
        outputFile += ".h";

    DataOutputContext outputContext(this, outputFile.c_str(), useTextOutput);
    if (!outputContext.stream)
    {
        Utils::Printf(RED "ERROR: Can't open output file '%s'!\n", outputFile.c_str());
        return false;
    }

    if (useTextOutput)
    {
        std::string name = GetShaderName(blobName);
        outputContext.WriteTextPreamble(name.c_str(), "");
    }

    ShaderMake::WriteFileCallback writeFileCallback = useTextOutput
        ? &DataOutputContext::WriteDataAsTextCallback
        : &DataOutputContext::WriteDataAsBinaryCallback;

    // Write "blob" header
    if (!ShaderMake::WriteFileHeader(writeFileCallback, &outputContext))
    {
        Utils::Printf(RED "ERROR: Failed to write into output file '%s'!\n", outputFile.c_str());
        return false;
    }

    bool success = true;

    // Collect individual permutations
    for (const BlobEntry &entry : entries)
    {
        // Open compiled permutation file
        std::string file = entry.permutationFileWithoutExt + outputExt;
        std::vector<uint8_t> fileData;
        if (Utils::ReadBinaryFile(file.c_str(), fileData))
        {
            if (!ShaderMake::WritePermutation(writeFileCallback, &outputContext, entry.combinedDefines, fileData.data(), fileData.size()))
            {
                Utils::Printf(RED "ERROR: Failed to write a shader permutation into '%s'!\n", outputFile.c_str());
                success = false;
            }
        }
        else
            success = false;

        if (!success)
            break;
    }

    if (useTextOutput)
        outputContext.WriteTextEpilog();

    return success;
}

void Context::RemoveIntermediateBlobFiles(const std::vector<BlobEntry> &entries)
{
    for (const BlobEntry &entry : entries)
    {
        std::string file = entry.permutationFileWithoutExt + outputExt;
        std::filesystem::remove(file);
    }
}

void Context::SignalHandler(int32_t sig)
{
    UNUSED(sig);
    terminate = true;
    Utils::Printf(RED "Aborting...\n");
}

DataOutputContext::DataOutputContext(Context *ctx, const char *file, bool textMode)
    : m_Ctx(ctx)
{
    stream = fopen(file, textMode ? "w" : "wb");
    if (!stream)
        Utils::Printf(RED "ERROR: Can't open file '%s' for writing!\n", file);
}

DataOutputContext::~DataOutputContext()
{
    if (stream)
    {
        fclose(stream);
        stream = nullptr;
    }
}

bool DataOutputContext::WriteDataAsText(const void *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        uint8_t value = ((const uint8_t *)data)[i];

        if (m_lineLength > 128)
        {
            fprintf(stream, "\n    ");
            m_lineLength = 0;
        }

        fprintf(stream, "%u,", value);

        if (value < 10)
            m_lineLength += 3;
        else if (value < 100)
            m_lineLength += 4;
        else
            m_lineLength += 5;
    }

    return true;
}

void DataOutputContext::WriteTextPreamble(const char *shaderName, const std::string &combinedDefines)
{
    fprintf(stream, "// {%s}\n", combinedDefines.c_str());
    fprintf(stream, "const uint8_t %s[] = {", shaderName);
}

void DataOutputContext::WriteTextEpilog()
{
    fprintf(stream, "\n};\n");
}

bool DataOutputContext::WriteDataAsBinary(const void *data, size_t size)
{
    if (size == 0)
        return true;

    return fwrite(data, size, 1, stream) == 1;
}

// For use as a callback in "WriteFileHeader" and "WritePermutation" functions
bool DataOutputContext::WriteDataAsTextCallback(const void *data, size_t size, void *context)
{
    return ((DataOutputContext *)context)->WriteDataAsText(data, size);
}

bool DataOutputContext::WriteDataAsBinaryCallback(const void *data, size_t size, void *context)
{
    return ((DataOutputContext *)context)->WriteDataAsBinary(data, size);
}

bool ConfigLine::Parse(int32_t argc, const char **argv, const Options &opts)
{
    source = argv[0];

    const char *unused = nullptr; // storage for the callback
    struct argparse_option options[] = {
        OPT_STRING('T', "profile", &profile, "Shader profile", nullptr, 0, 0),
        OPT_STRING('E', "entryPoint", &entryPoint, "(Optional) entry point", nullptr, 0, 0),
        OPT_STRING('D', "define", &unused, "(Optional) define(s) in forms 'M=value' or 'M'", ArgsUtils::AddLocalDefine, (intptr_t)this, 0),
        OPT_STRING('o', "output", &outputDir, "(Optional) output subdirectory", nullptr, 0, 0),
        OPT_INTEGER('O', "optimization", &optimizationLevel, "(Optional) optimization level", nullptr, 0, 0),
        OPT_STRING('s', "outputSuffix", &outputSuffix, "(Optional) suffix to add before extension after filename", nullptr, 0, 0),
        OPT_STRING('m', "shaderModel", &shaderModel, "(Optional) shader model for DXIL/SPIRV (always SM 5.0 for DXBC) in 'X_Y' format", nullptr, 0, 0),
        OPT_END(),
    };

    static const char *usages[] = {
        "path/to/shader -T profile [-E entry -O{0|1|2|3} -o \"output/subdirectory\" -s \"suffix\" -m 6_5 -D DEF1={0,1} -D DEF2={0,1,2} -D DEF3 ...]",
        nullptr
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, nullptr, "\nConfiguration options for a shader");
    argparse_parse(&argparse, argc, argv);

    if (!shaderModel)
        shaderModel = opts.shaderModel;

    // If there are some non-option elements in the config line, they will remain in the argv array.
    if (argv[0])
    {
        Utils::Printf(RED "ERROR: Unrecognized element in the config line: '%s'!\n", argv[0]);
        return false;
    }

    if (!profile)
    {
        Utils::Printf(RED "ERROR: Shader target not specified!\n");
        return false;
    }

    if (strlen(shaderModel) != 3 || strstr(shaderModel, "."))
    {
        Utils::Printf(RED "ERROR: Shader model ('%s') must have format 'X_Y'!\n", shaderModel);
        return false;
    }

    return true;
}

TaskData::TaskData(Context *ctx)
    : m_Ctx(ctx)
{
}

void TaskData::UpdateProgress(bool isSucceeded, bool willRetry, const char *message)
{
    if (isSucceeded)
    {
        float progress = 100.0f * float(++m_Ctx->processedTaskCount) / float(m_Ctx->originalTaskCount);

        if (message)
        {
            Utils::Printf(YELLOW "[%5.1f%%] %s %s {%s} {%s}\n%s",
                progress, m_Ctx->options.platformName,
                source.c_str(),
                entryPoint.c_str(),
                combinedDefines.c_str(),
                message);
        }
        else
        {
            Utils::Printf(GREEN "[%5.1f%%]" GRAY " %s" WHITE " %s" GRAY " {%s}" WHITE " {%s}\n",
                progress, m_Ctx->options.platformName,
                source.c_str(),
                entryPoint.c_str(),
                combinedDefines.c_str());
        }
    }
    else
    {
        if (willRetry)
        {
            Utils::Printf(YELLOW "[ RETRY-QUEUED ] %s %s {%s} {%s}\n",
                m_Ctx->options.platformName,
                source.c_str(),
                entryPoint.c_str(),
                combinedDefines.c_str());

            std::lock_guard<std::mutex> guard(m_Ctx->taskMutex);
            m_Ctx->tasks.push_back(*this);

            --m_Ctx->taskRetryCount;
        }
        else
        {
            Utils::Printf(RED "[ FAIL ] %s %s {%s} {%s}\n%s",
                m_Ctx->options.platformName,
                source.c_str(),
                entryPoint.c_str(),
                combinedDefines.c_str(),
                message ? message : "<no message text>!\n");

            if (!m_Ctx->options.continueOnError)
                m_Ctx->terminate = true;

            ++m_Ctx->failedTaskCount;
        }
    }
}

}