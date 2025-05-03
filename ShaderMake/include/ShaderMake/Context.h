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

#pragma once

#include <vector>
#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <array>

#include <fstream>
#include <stdarg.h>

#include "Compiler.h"

#ifdef _WIN32
#   include <d3dcompiler.h> // FXC
#   include <dxcapi.h> // DXC

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#else
#   include <unistd.h>
#   include <limits.h>
#endif

#include <filesystem>
#include <list>

#ifdef SHADERMAKE_COLORS
#define RED "\x1b[31m"
#define GRAY "\x1b[90m"
#define WHITE "\x1b[0m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#endif

#define _L(x)  __L(x)
#define __L(x) L ## x
#define UNUSED(x) ((void)(x))
#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))

#define SPIRV_SPACES_NUM 8
#define PDB_DIR "PDB"

#ifdef _MSC_VER
#   define popen _popen
#   define pclose _pclose
#   define putenv _putenv
#endif

#define USE_GLOBAL_OPTIMIZATION_LEVEL 0xFF

namespace ShaderMake {

enum PlatformType : uint8_t
{
    PlatformType_DXBC,
    PlatformType_DXIL,
    PlatformType_SPIRV,
    PlatformType_NUM
};

enum CompilerType : uint8_t
{
    CompilerType_DXC,
    CompilerType_FXC,
    CompilerType_Slang
};

namespace Utils {
static void Printf(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    // Print
    vprintf(format, argptr);
    va_end(argptr);

    // Restore default color if colorization is on
    printf(WHITE);

    // IMPORTANT: needed only if being run in CMake environment
    fflush(stdout);
}
static uint32_t HashToUint(size_t hash) { return uint32_t(hash) ^ (uint32_t(hash >> 32)); }
static std::string PathToString(std::filesystem::path path) { return path.lexically_normal().make_preferred().string(); }
static std::wstring AnsiToWide(const std::string &s) { return std::wstring(s.begin(), s.end()); }
static bool IsSpace(char ch) { return strchr(" \t\r\n", ch) != nullptr; }
static bool HasRepeatingSpace(char a, char b) { return (a == b) && a == ' '; }
static std::filesystem::path RemoveLeadingDotDots(const std::filesystem::path &path)
{
    auto it = path.begin();
    while (*it == ".." && it != path.end())
        ++it;

    std::filesystem::path result;
    while (it != path.end())
    {
        result = result / *it;
        ++it;
    }

    return result;
}
static std::string EscapePath(const std::string &s)
{
    if (s.find(' ') != std::string::npos)
        return "\"" + s + "\"";
    return s;
}
static void TrimConfigLine(std::string &s)
{
    // Remove leading whitespace
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](char ch) { return !IsSpace(ch); }));

    // Remove trailing whitespace
    s.erase(find_if(s.rbegin(), s.rend(), [](char ch) { return !IsSpace(ch); }).base(), s.end());

    // Tabs to spaces
    replace(s.begin(), s.end(), '\t', ' ');

    // Remove double spaces
    std::string::iterator newEnd = unique(s.begin(), s.end(), HasRepeatingSpace);
    s.erase(newEnd, s.end());
}
static void TokenizeConfigLine(char *in, std::vector<const char *> &tokens)
{
    char *out = in;
    char *token = out;

    // Some magic to correctly tokenize spaces in ""
    bool isString = false;
    while (*in)
    {
        if (*in == '"')
            isString = !isString;
        else if (*in == ' ' && !isString)
        {
            *in = '\0';
            if (*token)
                tokens.push_back(token);
            token = out + 1;
        }

        if (*in != '"')
            *out++ = *in;

        in++;
    }
    *out = '\0';

    if (*token)
        tokens.push_back(token);
}
static uint32_t GetFileLength(FILE *stream)
{
    /*
    TODO: can be done more efficiently
    Win:
        #include <io.h>

        _filelength( _fileno(f) );
    Linux:
        #include <sys/types.h> //?
        #include <sys/stat.h>

        struct stat buf;
        fstat(fd, &buf);
        off_t size = buf.st_size;
    */

    const uint32_t pos = ftell(stream);
    fseek(stream, 0, SEEK_END);
    const uint32_t len = ftell(stream);
    fseek(stream, pos, SEEK_SET);

    return len;
}
static bool ReadBinaryFile(const char *file, std::vector<uint8_t> &outData)
{
    FILE *stream = fopen(file, "rb");
    if (!stream)
    {
        Printf(RED "ERROR: Can't open file '%s'!\n", file);
        return false;
    }

    uint32_t const binarySize = GetFileLength(stream);
    if (binarySize == 0)
    {
        Printf(RED "ERROR: Binary file '%s' is empty!\n", file);
        fclose(stream);
        return false;
    }

    // Warn if the file is suspiciously large
    if (binarySize > (64 << 20)) // > 64Mb
        Printf(YELLOW "WARNING: Binary file '%s' is too large!\n", file);

    // Allocate memory foe the whole file
    outData.resize(binarySize);

    // Read the file
    size_t bytesRead = fread(outData.data(), 1, binarySize, stream);
    bool success = (bytesRead == binarySize);

    fclose(stream);
    return success;
}
static std::string PlatformToString(PlatformType platform)
{
    switch (platform)
    {
        case PlatformType_DXIL: return "DXIL";
        case PlatformType_DXBC: return "DXBC";
        case PlatformType_SPIRV: return "SPIRV";
        default: return "";
    }
}
static std::string PlatformExtension(PlatformType platform)
{
    switch (platform)
    {
        case PlatformType_DXIL: return ".dxil";
        case PlatformType_DXBC: return ".dxbc";
        case PlatformType_SPIRV: return ".spirv";
        default: return "";
    }
}
static std::string CompilerExecutablePath(CompilerType compilerType)
{
    switch (compilerType)
    {
#ifdef _WIN32
        case CompilerType_DXC: return "dxc.exe";
        case CompilerType_FXC: return "fxc.exe";
        case CompilerType_Slang: return "slangc.exe";
#else
        case CompilerType_DXC: return "dxc";
        case CompilerType_FXC: return "fxc";
        case CompilerType_Slang: return "slangc";
#endif
        default: return "";
    }
}
}

struct BlobEntry
{
    std::string permutationFileWithoutExt;
    std::string combinedDefines;
};

class Options
{
public:
    CompilerType compilerType = CompilerType_DXC;
    PlatformType platformType = PlatformType_DXIL;

    std::filesystem::path compilerPath;
    std::filesystem::path baseDirectory;

    std::string shaderModel = "6_5";
    std::string vulkanVersion = "1.3";
    std::string outputDir;
    std::string outputExt;
    std::string vulkanMemoryLayout;

    std::vector<std::filesystem::path> includeDirs;
    std::vector<std::filesystem::path> relaxedIncludes;

    std::vector<std::string> defines;
    std::vector<std::string> spirvExtensions = { "SPV_EXT_descriptor_indexing", "KHR" };
    std::vector<std::string> compilerOptions;

    uint32_t tRegShift = 0; // must be first (or change "DxcCompile" code)
    uint32_t sRegShift = 128;
    uint32_t bRegShift = 256;
    uint32_t uRegShift = 384;

    uint32_t optimizationLevel = 3;

    bool serial = false;
    bool flatten = false;
    bool help = false;
    bool binary = true;
    bool header = false;
    bool binaryBlob = true;
    bool headerBlob = false;
    bool continueOnError = false;
    bool warningsAreErrors = false;
    bool allResourcesBound = false;
    bool pdb = false;
    bool embedPdb = false;
    bool stripReflection = false;
    bool matrixRowMajor = false;
    bool hlsl2021 = false;
    bool verbose = false;
    bool colorize = true;
    bool useAPI = false;
    bool slang = false;
    bool slangHlsl = false;
    bool noRegShifts = false;
    int retryCount = 10; // default 10 retries for compilation task sub-process failures

    inline bool IsBlob() const
    {
        return binaryBlob || headerBlob;
    }

    void AddDefine(const std::string &define)
    {
        defines.push_back(define);
    }

    void AddSpirvExtension(const std::string &ext)
    {
        spirvExtensions.push_back(ext);
    }

    void AddCompilerOptions(const std::string &opt)
    {
        compilerOptions.push_back(opt);
    }
};

enum class ShaderType
{
    Vertex,
    Pixel,
    Geometry
};

static const char *ShaderTypeToProfile(ShaderType type)
{
    switch (type)
    {
        case ShaderMake::ShaderType::Vertex:
            return "vs";
        case ShaderMake::ShaderType::Pixel:
            return "ps";
        case ShaderMake::ShaderType::Geometry:
            return "gs";
        default:
            return "invalid";
    }
}

struct ShaderContextDesc
{
    std::string entryPoint = "main";
    std::string shaderModel = "6_5";
    std::vector<std::string> defines;
    uint32_t optimizationLevel = 3;
};

class ShaderContext
{
public:
    explicit ShaderContext(const std::string &filepath, ShaderType type, const ShaderContextDesc &desc = ShaderContextDesc(), bool forceRecompile = false)
        : m_Filepath(filepath), m_Type(type), m_Desc(desc), m_ForceCompile(forceRecompile)
    {
    }

    std::string GetFilepath() const { return m_Filepath; }
    ShaderContextDesc GetDesc() const { return m_Desc; }
    ShaderType GetType() const { return m_Type; }
    bool IsForceRecompile() const { return m_ForceCompile; }

    ShaderBlob blob;

private:
    std::string m_Filepath;
    ShaderType m_Type;
    bool m_ForceCompile;
    ShaderContextDesc m_Desc;
};

struct ConfigLine
{
    std::vector<std::string> defines;
    const char *source = nullptr;
    const char *entryPoint = "main";
    const char *profile = nullptr;
    const char *outputDir = nullptr;
    const char *outputSuffix = nullptr;
    const char *shaderModel = nullptr;

    uint32_t optimizationLevel = USE_GLOBAL_OPTIMIZATION_LEVEL;

    bool Parse(int32_t argc, const char **argv, const Options &opts);
};

class TaskData;

class Context
{
public:
    Options *options = nullptr;
    std::mutex taskMutex;

    std::map<std::filesystem::path, std::filesystem::file_time_type> hierarchicalUpdateTimes;
    std::map<std::string, std::vector<BlobEntry>> shaderBlobs;
    std::vector<TaskData> tasks;
    std::atomic<uint32_t> processedTaskCount;
    std::atomic<int> taskRetryCount;
    std::atomic<uint32_t> failedTaskCount = 0;
    std::atomic<bool> terminate = false;
    uint32_t originalTaskCount;

    void DumpShader(const TaskData &taskData, const uint8_t *data, size_t datSize);
    bool ProcessConfigLine(uint32_t lineIndex, const std::string &line, const std::filesystem::file_time_type &configTime, const char *configFilepath);
    bool ExpandPermutations(uint32_t lineIndex, const std::string &line, const std::filesystem::file_time_type &configTime, const char *configFilepath);
    bool GetHierarchicalUpdateTime(const std::filesystem::path &file, std::list<std::filesystem::path> &callStack, std::filesystem::file_time_type &outTime);
    bool CreateBlob(const std::string &blobName, const std::vector<BlobEntry> &entries, bool useTextOutput);
    void RemoveIntermediateBlobFiles(const std::vector<BlobEntry> &entries);

    CompileStatus CompileShader(std::vector<std::shared_ptr<ShaderContext>> shaderContexts);
    CompileStatus CompileConfigFile(const std::string &configFilename, bool foreceRecompile = false);

    Context() = default;
    Context(Options *opts);

private:
    bool ProcessTasks();

    void ProcessOptions();
};

class DataOutputContext
{
public:
    FILE *stream = nullptr;

    DataOutputContext(Context *ctx, const char *file, bool textMode);
    ~DataOutputContext();
    bool WriteDataAsText(const void *data, size_t size);
    void WriteTextPreamble(const char *shaderName, const std::string &combinedDefines);
    void WriteTextEpilog();
    bool WriteDataAsBinary(const void *data, size_t size);
    static bool WriteDataAsTextCallback(const void *data, size_t size, void *context);
    static bool WriteDataAsBinaryCallback(const void *data, size_t size, void *context);

private:
    Context *m_Ctx = nullptr;
    uint32_t m_lineLength = 129;
};

class TaskData
{
public:
    TaskData() = default;
    void UpdateProgress(Context *ctx, bool isSucceeded, bool willRetry, const char *message);

    ShaderBlob *blob = nullptr;

    std::vector<std::string> defines;
    std::filesystem::path filepath;
    std::string entryPoint;
    std::string profile;
    std::string shaderModel;
    std::string combinedDefines;
    uint32_t optimizationLevel = 3;

    // compiling requirements (auto set)
    const wchar_t *optimizationLevelRemap = nullptr;
    std::vector<std::wstring> regShifts;
    std::filesystem::path finalOutputPathNoExtension;
};

}