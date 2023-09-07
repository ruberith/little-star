#include "Shader.h"

#include <codecvt>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace std;
using namespace vk;

bool Shader::isCompiled()
{
    return fs::exists(path());
}

void Shader::load()
{
    fs::path input = path();
    ifstream file(input, ios::ate | ios::binary);
    if (!file.is_open())
    {
        throw runtime_error("Failed to load shader from " + input.string());
    }
    streamsize size = file.tellg();
    code.resize(size);
    file.seekg(0);
    file.read(code.data(), size);
    file.close();
}

vk::ShaderStageFlagBits Shader::stageBit() const
{
    switch (stage)
    {
    case ShaderStage::Vertex:
        return ShaderStageFlagBits::eVertex;
    case ShaderStage::Hull:
    case ShaderStage::TessellationControl:
        return ShaderStageFlagBits::eTessellationControl;
    case ShaderStage::Domain:
    case ShaderStage::TessellationEvaluation:
        return ShaderStageFlagBits::eTessellationEvaluation;
    case ShaderStage::Geometry:
        return ShaderStageFlagBits::eGeometry;
    case ShaderStage::Pixel:
    case ShaderStage::Fragment:
        return ShaderStageFlagBits::eFragment;
    case ShaderStage::Compute:
        return ShaderStageFlagBits::eCompute;
    case ShaderStage::Amplification:
    case ShaderStage::Task:
        return ShaderStageFlagBits::eTaskNV;
    case ShaderStage::Mesh:
        return ShaderStageFlagBits::eMeshNV;
    case ShaderStage::Raygen:
        return ShaderStageFlagBits::eRaygenKHR;
    case ShaderStage::AnyHit:
        return ShaderStageFlagBits::eAnyHitKHR;
    case ShaderStage::ClosestHit:
        return ShaderStageFlagBits::eClosestHitKHR;
    case ShaderStage::Miss:
        return ShaderStageFlagBits::eMissKHR;
    case ShaderStage::Intersection:
        return ShaderStageFlagBits::eIntersectionKHR;
    case ShaderStage::Callable:
        return ShaderStageFlagBits::eCallableKHR;
    }
}

std::string Shader::stageSuffix() const
{
    switch (stage)
    {
    case ShaderStage::Vertex:
        return "vs";
    case ShaderStage::Hull:
    case ShaderStage::TessellationControl:
        return "hs";
    case ShaderStage::Domain:
    case ShaderStage::TessellationEvaluation:
        return "ds";
    case ShaderStage::Geometry:
        return "gs";
    case ShaderStage::Pixel:
    case ShaderStage::Fragment:
        return "ps";
    case ShaderStage::Compute:
        return "cs";
    case ShaderStage::Amplification:
    case ShaderStage::Task:
        return "as";
    case ShaderStage::Mesh:
        return "ms";
    case ShaderStage::Raygen:
    case ShaderStage::AnyHit:
    case ShaderStage::ClosestHit:
    case ShaderStage::Miss:
    case ShaderStage::Intersection:
    case ShaderStage::Callable:
        return "lib";
    }
}

ShaderCompiler::ShaderCompiler()
{
    status = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    if (FAILED(status))
    {
        throw runtime_error("Failed to create DXC utils");
    }

    status = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(status))
    {
        throw runtime_error("Failed to create DXC compiler");
    }

    status = utils->CreateDefaultIncludeHandler(&includeHandler);
    if (FAILED(status))
    {
        throw runtime_error("Failed to create DXC include handler");
    }
}

void ShaderCompiler::compile(const Shader& shader)
{
    fs::path input = shader.path(false);
    wstring w_input = input.wstring();
    wstring w_inputFilename = input.filename().wstring();
    wstring w_hlslVersion = to_wstring(hlsl.major);
    wstring w_includePath = includePath.wstring();
    string stageSuffix = shader.stageSuffix();
    wstring w_stageSuffix = wstring(stageSuffix.begin(), stageSuffix.end());
    wstring w_targetProfile =
        w_stageSuffix + L"_" + to_wstring(shaderModel.major) + L"_" + to_wstring(shaderModel.minor);
    vector<LPCWSTR> args{
        // clang-format off
        w_inputFilename.data(),
        L"-E",      L"main",
        L"-HV",     w_hlslVersion.data(),
        L"-I",      w_includePath.data(),
        L"-T",      w_targetProfile.data(),
        L"-spirv",
        // clang-format on
    };
    for (const wstring& w_macro : shader.macros)
    {
        args.emplace_back(L"-D");
        args.emplace_back(w_macro.data());
    }

    // Load the shader.
    CComPtr<IDxcBlobEncoding> inputBlob{};
    status = utils->LoadFile(w_input.data(), {}, &inputBlob);
    if (FAILED(status))
    {
        throw runtime_error("Failed to load shader [" + input.filename().string() + "]");
    }
    DxcBuffer source{
        .Ptr = inputBlob->GetBufferPointer(),
        .Size = inputBlob->GetBufferSize(),
        .Encoding = DXC_CP_ACP,
    };

    // Compile the shader.
    CComPtr<IDxcResult> result{};
    status = compiler->Compile(&source, args.data(), static_cast<uint32_t>(args.size()), includeHandler,
                               IID_PPV_ARGS(&result));
    if (FAILED(status))
    {
        throw runtime_error("Failed to compile shader [" + input.filename().string() + "]");
    }

    // Check for compilation errors.
    CComPtr<IDxcBlobUtf8> errorBlob{};
    status = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), {});
    if (SUCCEEDED(status) && errorBlob && errorBlob->GetStringLength() != 0)
    {
        cerr << errorBlob->GetStringPointer() << endl;
    }

    // Check the compilation result.
    if (FAILED(result->GetStatus(&status)) || FAILED(status))
    {
        throw runtime_error("Failed to compile shader [" + input.filename().string() + "]");
    }

    // Save the binary.
    CComPtr<IDxcBlob> outputBlob{};
    status = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&outputBlob), {});
    if (FAILED(status) || !outputBlob)
    {
        throw runtime_error("Failed to get binary of shader [" + input.filename().string() + "]");
    }
    ofstream file(shader.path(), ios::binary);
    file.write(static_cast<const char*>(outputBlob->GetBufferPointer()),
               static_cast<streamsize>(outputBlob->GetBufferSize()));
    file.close();
}
