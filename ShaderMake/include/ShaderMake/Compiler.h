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
#include <filesystem>

#ifdef _WIN32
#   include <d3dcommon.h>
#   include <combaseapi.h>
#endif

namespace ShaderMake {

namespace Utils {
#ifdef _WIN32
    static void TokenizeDefineStrings(std::vector<std::string> &in, std::vector<D3D_SHADER_MACRO> &out)
    {
        if (in.empty())
            return;

        out.reserve(out.size() + in.size());
        for (const std::string &defineString : in)
        {
            D3D_SHADER_MACRO &define = out.emplace_back();
            char *s = (char *)defineString.c_str(); // IMPORTANT: "defineString" gets split into tokens divided by '\0'
            define.Name = strtok(s, "=");
            define.Definition = strtok(nullptr, "=");
        }
    }

    // Parses a string with command line options into a vector of wstring, one wstring per option.
    // Options are separated by spaces and may be quoted with "double quotes".
    // Backslash (\) means the next character is inserted literally into the output.
    static void TokenizeCompilerOptions(const char *in, std::vector<std::wstring> &out)
    {
        std::wstring current;
        bool quotes = false;
        bool escape = false;
        const char *ptr = in;
        while (char ch = *ptr++)
        {
            if (escape)
            {
                current.push_back(wchar_t(ch));
                escape = false;
                continue;
            }

            if (ch == ' ' && !quotes)
            {
                if (!current.empty())
                    out.push_back(current);
                current.clear();
            }
            else if (ch == '\\')
            {
                escape = true;
            }
            else if (ch == '"')
            {
                quotes = !quotes;
            }
            else
            {
                current.push_back(wchar_t(ch));
            }
        }

        if (!current.empty())
        {
            out.push_back(current);
        }
    }
#endif
}
    
    class TaskData;
    class Context;
    class Options;

    class Compiler
    {
    public:
        Compiler(Context *ctxt);
        void ExeCompile();
        void FxcCompile();
        void DxcCompile();

    private:
        Context *m_Ctx = nullptr;
    };

}