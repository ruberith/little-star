#pragma once

#include "Utils.h"
#include "VersionNumber.h"
#include <vulkan/vulkan.hpp>

#ifdef _WIN32
#include <atlbase.h>
#endif
#include <dxc/dxcapi.h>

// shader stage
enum class ShaderStage
{
    Vertex,
    Hull,
    TessellationControl,
    Domain,
    TessellationEvaluation,
    Geometry,
    Pixel,
    Fragment,
    Compute,
    Amplification,
    Task,
    Mesh,
    Raygen,
    AnyHit,
    ClosestHit,
    Miss,
    Intersection,
    Callable
};

// shader
struct Shader
{
    // shader name
    std::string name;
    // shader stage
    ShaderStage stage;
    // shader code
    std::vector<char> code;
    // macros (<identifier>=<value>) defined in the shader
    std::vector<std::wstring> macros;

    // Return true if the shader is compiled to SPIR-V. Return false otherwise.
    bool isCompiled();
    // Load the shader.
    void load();
    // Return the shader stage flag bit.
    vk::ShaderStageFlagBits stageBit() const;
    // Return the stage suffix of the shader file.
    std::string stageSuffix() const;
    // Return the path to the (compiled) shader file.
    inline std::filesystem::path path(bool compiled = true) const
    {
        return demoPath.parent_path().parent_path() / "demo" / "shaders" / name /
               (name + "." + stageSuffix() + (compiled ? ".spv" : ".hlsl"));
    }
    // Return the wide-character macro string constructed from identifier and value.
    template <typename T> static std::wstring macro(const std::string& identifier, T value)
    {
        return std::wstring(identifier.begin(), identifier.end()) + L"=" + std::to_wstring(value);
    }
};

// shader compiler
class ShaderCompiler
{
  private:
    // HLSL version
    const VersionNumber hlsl{.major = 2021};
    // include path
    const std::filesystem::path includePath = []() -> std::filesystem::path {
        return demoPath.parent_path().parent_path() / "demo" / "shaders";
    }();
    // HLSL shader model
    const VersionNumber shaderModel{.major = 6, .minor = 6};
    // compiler status
    HRESULT status{S_OK};
    // DXC utils
    CComPtr<IDxcUtils> utils{};
    // DXC compiler
    CComPtr<IDxcCompiler3> compiler{};
    // DXC include handler
    CComPtr<IDxcIncludeHandler> includeHandler{};

  public:
    // Construct the shader compiler.
    ShaderCompiler();
    // Destruct the shader compiler.
    ~ShaderCompiler() = default;
    // Compile the given shader from HLSL to SPIR-V.
    void compile(const Shader& shader);
};