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

#include "Compiler.h"
#include "Context.h"

#include <mutex>
#include <sstream>

namespace ShaderMake {

#ifdef _WIN32
    class FxcIncluder : public ID3DInclude
    {
    public:
        FxcIncluder(Options *opts, const std::filesystem::path &sourceFile)
        {
            m_IncludeDirs.reserve(opts->includeDirs.size() + 8);

            m_IncludeDirs.push_back(sourceFile.parent_path());
            for (const std::filesystem::path &path : opts->includeDirs)
                m_IncludeDirs.push_back(path);
        }

        STDMETHOD(FxcIncluder::Open)(D3D_INCLUDE_TYPE includeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
        {
            // TODO: an error in an include file gets reported using relative path with recent WinSDKs, what makes it 
            // "unclickable" from the Visual Studio output window. It doesn't look like an issue of "FxcIncluder",
            // because the issue is here even with "D3D_COMPILE_STANDARD_FILE_INCLUDE", but "FXC.exe" works fine.

            UNUSED(includeType);
            UNUSED(pParentData);

            *ppData = 0;
            *pBytes = 0;

            // Find file
            std::filesystem::path name = std::filesystem::path(pFileName);
            std::filesystem::path file = name;
            if (file.is_relative())
            {
                for (const std::filesystem::path &path : m_IncludeDirs)
                {
                    file = path / name;
                    if (std::filesystem::exists(file))
                        break;
                }
            }

            // Open file
            FILE *stream = fopen(file.string().c_str(), "rb");
            if (!stream)
                return E_FAIL;

            // Load file
            uint32_t len = Utils::GetFileLength(stream);
            char *buf = (char *)malloc(len);
            if (!buf)
                return E_FAIL;

            fread(buf, 1, len, stream);
            fclose(stream);

            *ppData = buf;
            *pBytes = len;

            // Add the path to this file to the current include stack so that any sub-includes would be relative to this path
            m_IncludeDirs.push_back(file.parent_path());

            return S_OK;
        }

        STDMETHOD(FxcIncluder::Close)(THIS_ LPCVOID pData)
        {
            if (pData)
            {
                // Pop the path for the innermost included file from the include stack
                m_IncludeDirs.pop_back();

                // Release the data
                free((void *)pData);
            }

            return S_OK;
        }

    private:
        std::vector<std::filesystem::path> m_IncludeDirs;
    };
#endif

    Compiler::Compiler(Context *ctx)
        : m_Ctx(ctx)
    {
    }

    void Compiler::FxcCompile()
    {
        static const uint32_t optimizationLevelRemap[] = {
            D3DCOMPILE_SKIP_OPTIMIZATION,
            D3DCOMPILE_OPTIMIZATION_LEVEL1,
            D3DCOMPILE_OPTIMIZATION_LEVEL2,
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
        };

        std::vector<D3D_SHADER_MACRO> optionsDefines;
        std::vector<std::string> tokenizedDefines = m_Ctx->options->defines;
        Utils::TokenizeDefineStrings(tokenizedDefines, optionsDefines);

        while (!m_Ctx->terminate)
        {
            // Getting a task in the current thread
            TaskData taskData;
            {
                std::lock_guard<std::mutex> guard(m_Ctx->taskMutex);
                if (m_Ctx->tasks.empty())
                    return;

                taskData = m_Ctx->tasks.back();
                m_Ctx->tasks.pop_back();
            }

            // Tokenize DXBC defines
            std::vector<D3D_SHADER_MACRO> defines = optionsDefines;
            Utils::TokenizeDefineStrings(taskData.defines, defines);
            defines.push_back({ nullptr, nullptr });

            // Args
            uint32_t compilerFlags = (m_Ctx->options->pdb ? (D3DCOMPILE_DEBUG | D3DCOMPILE_DEBUG_NAME_FOR_BINARY) : 0) |
                (m_Ctx->options->allResourcesBound ? D3DCOMPILE_ALL_RESOURCES_BOUND : 0) |
                (m_Ctx->options->warningsAreErrors ? D3DCOMPILE_WARNINGS_ARE_ERRORS : 0) |
                (m_Ctx->options->matrixRowMajor ? D3DCOMPILE_PACK_MATRIX_ROW_MAJOR : 0) |
                optimizationLevelRemap[taskData.optimizationLevel];

            // Compiling the shader
            std::filesystem::path sourceFile = m_Ctx->options->baseDirectory / taskData.filepath;

            FxcIncluder fxcIncluder(m_Ctx->options, sourceFile);
            std::string profile = taskData.profile + "_5_0";

            ComPtr<ID3DBlob> codeBlob;
            ComPtr<ID3DBlob> errorBlob;
            HRESULT hr = D3DCompileFromFile(
                sourceFile.wstring().c_str(),
                defines.data(),
                &fxcIncluder,
                taskData.entryPoint.c_str(),
                profile.c_str(),
                compilerFlags, 0,
                &codeBlob,
                &errorBlob);

            bool isSucceeded = SUCCEEDED(hr) && codeBlob;

            if (m_Ctx->terminate)
                break;

            // Dump PDB
            if (isSucceeded && m_Ctx->options->pdb)
            {
                // Retrieve the debug info part of the shader
                ComPtr<ID3DBlob> pdb;
                D3DGetBlobPart(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), D3D_BLOB_PDB, 0, &pdb);

                // Retrieve the suggested name for the debug data file
                ComPtr<ID3DBlob> pdbName;
                D3DGetBlobPart(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), D3D_BLOB_DEBUG_NAME, 0, &pdbName);

                // This struct represents the first four bytes of the name blob
                struct ShaderDebugName
                {
                    uint16_t Flags;       // Reserved, must be set to zero
                    uint16_t NameLength;  // Length of the debug name, without null terminator
                    // Followed by NameLength bytes of the UTF-8-encoded name
                    // Followed by a null terminator
                    // Followed by [0-3] zero bytes to align to a 4-byte boundary
                };

                auto pDebugNameData = (const ShaderDebugName *)(pdbName->GetBufferPointer());
                auto pName = (const char *)(pDebugNameData + 1);

                std::string file = taskData.filepath.parent_path().generic_string() + "/" + PDB_DIR + "/" + pName;
                FILE *fp = fopen(file.c_str(), "wb");
                if (fp)
                {
                    fwrite(pdb->GetBufferPointer(), pdb->GetBufferSize(), 1, fp);
                    fclose(fp);
                }
            }

            // Strip reflection
            ComPtr<ID3DBlob> strippedBlob;
            if (m_Ctx->options->stripReflection && isSucceeded)
            {
                D3DStripShader(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO, &strippedBlob);
                codeBlob = strippedBlob;
            }

            // Dump output
            if (isSucceeded)
            {
                m_Ctx->DumpShader(taskData, (uint8_t *)codeBlob->GetBufferPointer(), codeBlob->GetBufferSize());
            }

            // Update progress
            taskData.UpdateProgress(m_Ctx, isSucceeded, false, errorBlob ? (char *)errorBlob->GetBufferPointer() : nullptr);

            // Terminate if this shader failed and "--continue" is not set
            if (m_Ctx->terminate)
                break;
        }
    }

    std::shared_ptr<DxcInstance> Compiler::DxcCompilerCreate()
    {
        std::shared_ptr<DxcInstance> dxcInstance = std::make_shared<DxcInstance>();

        // TODO: is a global instance thread safe?
        HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcInstance->compiler));
        if (FAILED(hr))
        {
            // Print a message explaining that we cannot compile anything.
            // This can happen when the user specifies a DXC version that is too old.
            std::lock_guard<std::mutex> guard(m_Ctx->taskMutex);
            static bool once = true;
            if (once)
            {
                Utils::Printf(RED "ERROR: Cannot create an instance of IDxcCompiler3, HRESULT = 0x%08x (%s)\n", hr, std::system_category().message(hr).c_str());
                once = false;
            }

            m_Ctx->terminate = true;
            return nullptr;
        }

        hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcInstance->utils));
        if (FAILED(hr))
        {
            // Also print an error message.
            // Not sure if this ever happens or all such cases are handled by the condition above, but let's be safe.
            std::lock_guard<std::mutex> guard(m_Ctx->taskMutex);
            static bool once = true;
            if (once)
            {
                Utils::Printf(RED "ERROR: Cannot create an instance of IDxcUtils, HRESULT = 0x%08x (%s)\n", hr, std::system_category().message(hr).c_str());
                once = false;
            }
            m_Ctx->terminate = true;
            return nullptr;
        }

        return dxcInstance;
    }

    CompileStatus Compiler::DxcCompile(std::shared_ptr<DxcInstance> &dxcInstance)
    {

        static const wchar_t *dxcOptimizationLevelRemap[] = {
            // Note: if you're getting errors like "error C2065: 'DXC_ARG_SKIP_OPTIMIZATIONS': undeclared identifier" here,
            // please update the Windows SDK to at least version 10.0.20348.0.
            DXC_ARG_SKIP_OPTIMIZATIONS,
            DXC_ARG_OPTIMIZATION_LEVEL1,
            DXC_ARG_OPTIMIZATION_LEVEL2,
            DXC_ARG_OPTIMIZATION_LEVEL3,
        };

        // Gather SPIRV register shifts once
        static const wchar_t *dxcRegShiftArgs[] = {
            L"-fvk-t-shift",
            L"-fvk-s-shift",
            L"-fvk-b-shift",
            L"-fvk-u-shift",
        };

        std::vector<std::wstring> regShifts;

        if (m_Ctx->options->noRegShifts == false)
        {
            for (uint32_t reg = 0; reg < 4; reg++)
            {
                for (uint32_t space = 0; space < SPIRV_SPACES_NUM; space++)
                {
                    wchar_t buf[64];

                    regShifts.push_back(dxcRegShiftArgs[reg]);

                    swprintf(buf, COUNT_OF(buf), L"%u", (&m_Ctx->options->tRegShift)[reg]);
                    regShifts.push_back(std::wstring(buf));

                    swprintf(buf, COUNT_OF(buf), L"%u", space);
                    regShifts.push_back(std::wstring(buf));
                }
            }
        }

        while (!m_Ctx->terminate)
        {
            // Getting a task in the current thread
            TaskData taskData;

            {
                std::lock_guard<std::mutex> guard(m_Ctx->taskMutex);
                if (m_Ctx->tasks.empty())
                {
                    return CompileStatus::Success;
                }

                taskData = m_Ctx->tasks.back();
                taskData.optimizationLevelRemap = dxcOptimizationLevelRemap[taskData.optimizationLevel];
                taskData.regShifts = regShifts;

                m_Ctx->tasks.pop_back();
            }

            DxcCompileTask(dxcInstance, taskData);
        }

        return CompileStatus::Success;
    }

    void Compiler::DxcCompileTask(std::shared_ptr<DxcInstance> &dxcInstance, TaskData &taskData)
    {
        // Compiling the shader
        std::filesystem::path sourceFile = m_Ctx->options->baseDirectory / taskData.filepath;
        std::wstring wsourceFile = sourceFile.wstring();

        ComPtr<IDxcBlob> codeBlob;
        ComPtr<IDxcBlobEncoding> errorBlob;
        bool isSucceeded = false;

        ComPtr<IDxcBlobEncoding> sourceBlob;
        HRESULT hr = dxcInstance->utils->LoadFile(wsourceFile.c_str(), nullptr, &sourceBlob);

        if (SUCCEEDED(hr))
        {
            std::vector<std::wstring> args;
            args.reserve(16 + (m_Ctx->options->defines.size()
                + taskData.defines.size()
                + m_Ctx->options->includeDirs.size()) * 2
                + (m_Ctx->options->platformType == PlatformType_SPIRV ? taskData.regShifts.size()
                + m_Ctx->options->spirvExtensions.size() : 0));

            // Source file
            args.push_back(wsourceFile);

            // Profile
            args.push_back(L"-T");
            args.push_back(Utils::AnsiToWide(taskData.profile + "_" + taskData.shaderModel));

            // Entry point
            args.push_back(L"-E");
            args.push_back(Utils::AnsiToWide(taskData.entryPoint));

            // Defines
            for (const std::string &define : m_Ctx->options->defines)
            {
                args.push_back(L"-D");
                args.push_back(Utils::AnsiToWide(define));
            }

            for (const std::string &define : taskData.defines)
            {
                args.push_back(L"-D");
                args.push_back(Utils::AnsiToWide(define));
            }

            // Include directories
            for (const std::filesystem::path &path : m_Ctx->options->includeDirs)
            {
                args.push_back(L"-I");
                args.push_back(path.wstring());
            }

            // Args
            args.push_back(taskData.optimizationLevelRemap);

            uint32_t shaderModelIndex = (taskData.shaderModel[0] - '0') * 10 + (taskData.shaderModel[2] - '0');
            if (shaderModelIndex >= 62)
                args.push_back(L"-enable-16bit-types");

            if (m_Ctx->options->warningsAreErrors)
                args.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);

            if (m_Ctx->options->allResourcesBound)
                args.push_back(DXC_ARG_ALL_RESOURCES_BOUND);

            if (m_Ctx->options->matrixRowMajor)
                args.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);

            if (m_Ctx->options->hlsl2021)
            {
                args.push_back(L"-HV");
                args.push_back(L"2021");
            }

            if (m_Ctx->options->pdb || m_Ctx->options->embedPdb)
            {
                // TODO: for SPIRV PDB can only be embedded, GetOutput(DXC_OUT_PDB) silently fails...
                args.push_back(L"-Zi");
                args.push_back(L"-Zsb"); // only binary code affects hash
            }

            if (m_Ctx->options->embedPdb)
                args.push_back(L"-Qembed_debug");

            if (m_Ctx->options->platformType == PlatformType_SPIRV)
            {
                args.push_back(L"-spirv");
                args.push_back(std::wstring(L"-fspv-target-env=vulkan") + Utils::AnsiToWide(m_Ctx->options->vulkanVersion));

                if (!m_Ctx->options->vulkanMemoryLayout.empty())
                    args.push_back(std::wstring(L"-fvk-use-") + Utils::AnsiToWide(m_Ctx->options->vulkanMemoryLayout) + std::wstring(L"-layout"));

                for (const std::string &ext : m_Ctx->options->spirvExtensions)
                    args.push_back(std::wstring(L"-fspv-extension=") + Utils::AnsiToWide(ext));

                for (const std::wstring &arg : taskData.regShifts)
                    args.push_back(arg);
            }
            else // Not supported by SPIRV gen
            {
                if (m_Ctx->options->stripReflection)
                    args.push_back(L"-Qstrip_reflect");
            }

            for (std::string const &opts : m_Ctx->options->compilerOptions)
            {
                Utils::TokenizeCompilerOptions(opts.c_str(), args);
            }

            // Debug output
            if (m_Ctx->options->verbose)
            {
                std::wstringstream cmd;
                for (const std::wstring &arg : args)
                {
                    cmd << arg;
                    cmd << L" ";
                }

                Utils::Printf(WHITE "%ls\n", cmd.str().c_str());
            }

            // Now that args are finalized, get their C-string pointers into a vector
            std::vector<const wchar_t *> argPointers;
            argPointers.reserve(args.size());
            for (const std::wstring &arg : args)
                argPointers.push_back(arg.c_str());

            // Compiling the shader
            DxcBuffer sourceBuffer = {};
            sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
            sourceBuffer.Size = sourceBlob->GetBufferSize();

            ComPtr<IDxcIncludeHandler> pDefaultIncludeHandler;
            dxcInstance->utils->CreateDefaultIncludeHandler(&pDefaultIncludeHandler);

            ComPtr<IDxcResult> dxcResult;
            hr = dxcInstance->compiler->Compile(&sourceBuffer, argPointers.data(), (uint32_t)args.size(), pDefaultIncludeHandler.Get(), IID_PPV_ARGS(&dxcResult));

            if (SUCCEEDED(hr))
                dxcResult->GetStatus(&hr);

            if (dxcResult)
            {
                dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&codeBlob), nullptr);
                dxcResult->GetErrorBuffer(&errorBlob);
            }

            isSucceeded = SUCCEEDED(hr) && codeBlob;

            // Dump PDB
            if (isSucceeded && m_Ctx->options->pdb)
            {
                ComPtr<IDxcBlob> pdb;
                ComPtr<IDxcBlobUtf16> pdbName;
                if (SUCCEEDED(dxcResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdbName)))
                {
                    std::wstring file = taskData.filepath.parent_path().wstring() + L"/" + _L(PDB_DIR) + L"/" + std::wstring(pdbName->GetStringPointer());
                    FILE *fp = _wfopen(file.c_str(), L"wb");
                    if (fp)
                    {
                        fwrite(pdb->GetBufferPointer(), pdb->GetBufferSize(), 1, fp);
                        fclose(fp);
                    }
                }
            }
        }

        if (m_Ctx->terminate)
        {
            return;
        }

        // Dump output
        if (isSucceeded)
        {
            std::filesystem::path filenameWithoutTargetExtension = (m_Ctx->options->baseDirectory / m_Ctx->options->outputDir / taskData.filepath.filename());
            taskData.finalOutputPathNoExtension = filenameWithoutTargetExtension.replace_extension(m_Ctx->options->outputExt);

            if (taskData.blob)
            {
                taskData.blob->data = (uint8_t *)codeBlob->GetBufferPointer();
                taskData.blob->dataSize = codeBlob->GetBufferSize();
            }

            m_Ctx->DumpShader(taskData, (uint8_t *)codeBlob->GetBufferPointer(), codeBlob->GetBufferSize());
        }

        // Update progress
        taskData.UpdateProgress(m_Ctx, isSucceeded, false, errorBlob ? (char *)errorBlob->GetBufferPointer() : nullptr);
    }

    void Compiler::ExeCompile()
    {
        static const char *optimizationLevelRemap[] = {
            " -Od",
            " -O1",
            " -O2",
            " -O3",
        };

        while (!m_Ctx->terminate)
        {
            // Getting a task in the current thread
            TaskData taskData;

            {
                std::lock_guard<std::mutex> guard(m_Ctx->taskMutex);
                if (m_Ctx->tasks.empty())
                    return;

                taskData = m_Ctx->tasks.back();
                m_Ctx->tasks.pop_back();
            }

            bool convertBinaryOutputToHeader = false;
            std::string outputFile = taskData.filepath.parent_path().generic_string() + m_Ctx->options->outputExt;

            // Building command line
            std::ostringstream cmd;
            {
                cmd << m_Ctx->options->compilerPath.generic_string().c_str(); // call the compiler

                if (m_Ctx->options->slang)
                {
                    if (m_Ctx->options->header || (m_Ctx->options->headerBlob && taskData.combinedDefines.empty()))
                    {
                        convertBinaryOutputToHeader = true;
                    }

                    // Slang defaults to slang language mode unless -lang <other language> sets something else.
                    // For HLSL compatibility mode:
                    //    - use -lang hlsl to set language mode to HLSL
                    //    - use -unscoped-enums so Slang doesn't require all enums to be scoped                
                    if (m_Ctx->options->slangHlsl)
                    {
                        // Language mode: hlsl
                        cmd << " -lang hlsl";

                        // Treat enums as unscoped
                        cmd << " -unscoped-enum";
                    }

                    // Profile
                    cmd << " -profile " << taskData.profile << "_" << taskData.shaderModel;

                    // Target/platform
                    cmd << " -target " << Utils::PlatformToString(m_Ctx->options->platformType);

                    // Output
                    cmd << " -o " << Utils::EscapePath(outputFile);

                    // Entry point
                    if (taskData.profile != "lib")
                    {
                        // Don't specify entry if profile is lib_*, Slang will use the entry point currently
                        cmd << " -entry " << taskData.entryPoint;
                    }

                    // Defines
                    for (const std::string &define : taskData.defines)
                        cmd << " -D " << define;

                    for (const std::string &define : m_Ctx->options->defines)
                        cmd << " -D " << define;

                    // Include directories
                    for (const std::filesystem::path &dir : m_Ctx->options->includeDirs)
                        cmd << " -I " << Utils::EscapePath(dir.string());

                    // Optimization level
                    cmd << " -O" << taskData.optimizationLevel;

                    // Warnings as errors
                    if (m_Ctx->options->warningsAreErrors)
                        cmd << " -warnings-as-errors";

                    // Matrix layout
                    if (m_Ctx->options->matrixRowMajor)
                        cmd << " -matrix-layout-row-major";
                    else
                        cmd << " -matrix-layout-column-major";

                    if (m_Ctx->options->platformType == PlatformType_SPIRV)
                    {
                        // Uses the entrypoint name from the source instead of 'main' in the SPIRV output
                        cmd << " -fvk-use-entrypoint-name";

                        if (!m_Ctx->options->vulkanMemoryLayout.empty())
                        {
                            if (strcmp(m_Ctx->options->vulkanMemoryLayout.c_str(), "scalar") == 0)
                                cmd << " -force-glsl-scalar-layout";
                            else if (strcmp(m_Ctx->options->vulkanMemoryLayout.c_str(), "gl") == 0)
                                cmd << " -fvk-use-gl-layout";
                        }

                        if (!m_Ctx->options->noRegShifts)
                        {
                            for (uint32_t space = 0; space < SPIRV_SPACES_NUM; space++)
                            {
                                cmd << " -fvk-t-shift " << m_Ctx->options->tRegShift << " " << space;
                                cmd << " -fvk-s-shift " << m_Ctx->options->sRegShift << " " << space;
                                cmd << " -fvk-b-shift " << m_Ctx->options->bRegShift << " " << space;
                                cmd << " -fvk-u-shift " << m_Ctx->options->uRegShift << " " << space;
                            }
                        }
                    }

                    // Custom options
                    for (std::string const &opts : m_Ctx->options->compilerOptions)
                        cmd << " " << opts;
                }
                else
                {
                    cmd << " -nologo";

                    // Output file
                    if (m_Ctx->options->binary || m_Ctx->options->binaryBlob || (m_Ctx->options->headerBlob && !taskData.combinedDefines.empty()))
                        cmd << " -Fo " << Utils::EscapePath(outputFile);
                    if (m_Ctx->options->header || (m_Ctx->options->headerBlob && taskData.combinedDefines.empty()))
                    {
                        cmd << " -Fh " << Utils::EscapePath(outputFile) << ".h";
                        cmd << " -Vn " << taskData.filepath.filename().generic_string();
                    }

                    // Profile
                    std::string profile = taskData.profile + "_";
                    if (m_Ctx->options->platformType == PlatformType_DXBC)
                        profile += "5_0";
                    else
                        profile += taskData.shaderModel;
                    cmd << " -T " << profile;

                    // Entry point
                    cmd << " -E " << taskData.entryPoint;

                    // Defines
                    for (const std::string &define : taskData.defines)
                        cmd << " -D " << define;

                    for (const std::string &define : m_Ctx->options->defines)
                        cmd << " -D " << define;

                    // Include directories
                    for (const std::filesystem::path &dir : m_Ctx->options->includeDirs)
                        cmd << " -I " << Utils::EscapePath(dir.string());

                    // Args
                    cmd << optimizationLevelRemap[taskData.optimizationLevel];

                    uint32_t shaderModelIndex = (taskData.shaderModel[0] - '0') * 10 + (taskData.shaderModel[2] - '0');
                    if (m_Ctx->options->platformType != PlatformType_DXBC && shaderModelIndex >= 62)
                        cmd << " -enable-16bit-types";

                    if (m_Ctx->options->warningsAreErrors)
                        cmd << " -WX";

                    if (m_Ctx->options->allResourcesBound)
                        cmd << " -all_resources_bound";

                    if (m_Ctx->options->matrixRowMajor)
                        cmd << " -Zpr";

                    if (m_Ctx->options->hlsl2021)
                        cmd << " -HV 2021";

                    if (m_Ctx->options->pdb || m_Ctx->options->embedPdb)
                        cmd << " -Zi -Zsb"; // only binary affects hash

                    if (m_Ctx->options->embedPdb)
                        cmd << " -Qembed_debug";

                    if (m_Ctx->options->platformType == PlatformType_SPIRV)
                    {
                        cmd << " -spirv";

                        cmd << " -fspv-target-env=vulkan" << m_Ctx->options->vulkanVersion;

                        if (!m_Ctx->options->vulkanMemoryLayout.empty())
                            cmd << " -fvk-use-" << m_Ctx->options->vulkanMemoryLayout << "-layout";

                        for (const std::string &ext : m_Ctx->options->spirvExtensions)
                            cmd << " -fspv-extension=" << ext;

                        if (!m_Ctx->options->noRegShifts)
                        {
                            for (uint32_t space = 0; space < SPIRV_SPACES_NUM; space++)
                            {
                                cmd << " -fvk-t-shift " << m_Ctx->options->tRegShift << " " << space;
                                cmd << " -fvk-s-shift " << m_Ctx->options->sRegShift << " " << space;
                                cmd << " -fvk-b-shift " << m_Ctx->options->bRegShift << " " << space;
                                cmd << " -fvk-u-shift " << m_Ctx->options->uRegShift << " " << space;
                            }
                        }
                    }
                    else // Not supported by SPIRV gen
                    {
                        if (m_Ctx->options->stripReflection)
                            cmd << " -Qstrip_reflect";

                        if (m_Ctx->options->pdb)
                        {
                            std::filesystem::path pdbPath = std::filesystem::path(outputFile).parent_path() / PDB_DIR;
                            cmd << " -Fd " << Utils::EscapePath(pdbPath.string() + "/"); // only binary code affects hash
                        }
                    }

                    // Custom options
                    for (std::string const &opts : m_Ctx->options->compilerOptions)
                        cmd << " " << opts;
                }

                // Source file
                std::filesystem::path sourceFile = m_Ctx->options->baseDirectory / taskData.filepath;
                cmd << " " << Utils::EscapePath(sourceFile.generic_string());
            }

            cmd << " 2>&1";

            // Debug output
            if (m_Ctx->options->verbose)
                Utils::Printf(WHITE "%s\n", cmd.str().c_str());

            // Compiling the shader
            std::ostringstream msg;
            FILE *pipe = popen(cmd.str().c_str(), "r");

            bool isSucceeded = false, willRetry = false;
            if (pipe)
            {
                char buf[1024];
                while (fgets(buf, sizeof(buf), pipe))
                {
                    // Ignore useless unmutable FXC message
                    if (strstr(buf, "compilation object save succeeded"))
                        continue;

                    msg << buf;
                }

                const int result = pclose(pipe);
                // Check status, see https://pubs.opengroup.org/onlinepubs/009696699/functions/pclose.html
                const bool childProcessError = (result == -1 && errno == ECHILD);
#ifdef WIN32
                const bool commandShellError = false;
#else
                const bool commandShellError = (WIFEXITED(result) && WEXITSTATUS(result) == 127);
#endif

                if (result == 0)
                    isSucceeded = true;

                // Retry if count > 0 and failed to execute child sub-process or command shell (posix only)
                else if (m_Ctx->taskRetryCount > 0 && (childProcessError || commandShellError))
                    willRetry = true;
            }

            // Slang cannot produce .h files directly, so we convert its binary output to .h here if needed
            if (isSucceeded && convertBinaryOutputToHeader)
            {
                std::vector<uint8_t> buffer;
                if (Utils::ReadBinaryFile(outputFile.c_str(), buffer))
                {
                    std::string headerFile = taskData.filepath.filename().generic_string() + m_Ctx->options->outputExt + ".h";
                    DataOutputContext context(m_Ctx, headerFile.c_str(), true);
                    if (context.stream)
                    {
                        std::string shaderName = taskData.filepath.filename().generic_string();
                        context.WriteTextPreamble(shaderName.c_str(), taskData.combinedDefines);
                        context.WriteDataAsText(buffer.data(), buffer.size());
                        context.WriteTextEpilog();

                        // Delete the binary file if it's not requested
                        if (!m_Ctx->options->binary)
                            std::filesystem::remove(outputFile);
                    }
                    else
                    {
                        Utils::Printf(RED "ERROR: Failed to open file '%s' for writing!\n", headerFile.c_str());
                        isSucceeded = false;
                    }
                }
                else
                {
                    isSucceeded = false;
                }
            }

            // Update progress
            taskData.UpdateProgress(m_Ctx, isSucceeded, willRetry, msg.str().c_str());
        }
    }
}
