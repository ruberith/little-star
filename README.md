# Little Star

Game demo utilizing Vulkan for real-time, physically-based simulation and rendering on the GPU

<p align="center">
    <a href="https://www.youtube.com/watch?v=PDV5Vn8NEfA">
        <img src="https://img.youtube.com/vi/PDV5Vn8NEfA/0.jpg" />
    </a>
</p>

<p align="center">
    <a href="https://www.youtube.com/watch?v=PDV5Vn8NEfA">Watch on YouTube</a> | 
    <a href="https://github.com/ruberith/little-star/releases/download/v1.0.0/little-star.win-x64.zip">Play on Windows</a>
</p>

## Features

- simulation of soft, deformable bodies and thousands of particles via XPBD
- Frostbite rendering model with optional cel shading
- third-person camera on a spherical world
- import of (animated) glTF models

## Objective

The light of the little star is fading. Collect star particles and bring them back to the star to regenerate its light.

## Controls

- Escape – Toggle cursor lock
- C – Toggle cel shading
- W|A|S|D – Move
- Shift – Run
- Mouse – Rotate camera
- Scroll – Adjust camera distance

## Build

1. Install [Git](https://git-scm.com), [Git LFS](https://git-lfs.com), [CMake](https://cmake.org), a build tool like [Ninja](https://ninja-build.org), a C++20 compiler (tested are Clang on macOS and MSVC on Windows), and the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home).
2. Clone the repository using Git.
3. Configure and build the demo using CMake. You can also do this in an IDE like Visual Studio (Code).
4. Run the demo executable.

## Credits

- [Animated Astronaut Character in Space Suit Loop](https://sketchfab.com/3d-models/animated-astronaut-character-in-space-suit-loop-8fe5c8d3365e4d87bb7bc253d53a64e1) by [LasquetiSpice](https://sketchfab.com/LasquetiSpice) is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- [Book Pattern](https://polyhaven.com/a/book_pattern) by [Rob Tuytel](https://www.artstation.com/tuytel) is licensed under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- [Brown Leather](https://polyhaven.com/a/brown_leather) by [Rob Tuytel](https://www.artstation.com/tuytel) is licensed under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- [Caveat](https://github.com/googlefonts/caveat) by [Pablo Impallari](https://www.impallari.com) is licensed under [OFL 1.1](https://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=OFL).
- [CGI Moon Kit](https://svs.gsfc.nasa.gov/4720) by [NASA's Scientific Visualization Studio](https://svs.gsfc.nasa.gov) is licensed under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- [Deep Star Maps 2020](https://svs.gsfc.nasa.gov/4851) by [NASA/Goddard Space Flight Center Scientific Visualization Studio](https://svs.gsfc.nasa.gov) is licensed under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui)
- [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler)
- [GLFW](https://github.com/glfw/glfw)
- [GLM (OpenGL Mathematics)](https://github.com/g-truc/glm)
- [KTX (Khronos Texture)](https://github.com/KhronosGroup/KTX-Software)
- [MikkTSpace](https://github.com/mmikk/MikkTSpace)
- [MshIO](https://github.com/qnzhou/MshIO)
- [SoLoud](https://github.com/jarikomppa/soloud)
- [stb](https://github.com/nothings/stb)
- [TinyGLTF](https://github.com/syoyo/tinygltf)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [Vulkan](https://vulkan.lunarg.com)
- [Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [VulkanMemoryAllocator-Hpp](https://github.com/ruberith/VulkanMemoryAllocator-Hpp)

## References

- [An Isotropic 3x3 Image Gradient Operator](https://www.researchgate.net/publication/239398674_An_Isotropic_3x3_Image_Gradient_Operator)
- [A Survey on Position Based Dynamics, 2017](https://doi.org/10.2312/egt.20171034)
- [Barycentric Coordinates](https://www.cdsimpson.net/2014/10/barycentric-coordinates.html)
- [Bitonic Sort](https://www.tools-of-computing.com/tc/CS/Sorts/bitonic_sort.htm)
- [Fast Tetrahedral Meshing in the Wild](https://doi.org/10.1145/3386569.3392385)
- [glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [KHR_materials_pbrSpecularGlossiness](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Archived/KHR_materials_pbrSpecularGlossiness/README.md)
- [KTX File Format Specification](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html)
- [Moving Frostbite to Physically Based Rendering 3.0](https://www.ea.com/frostbite/news/moving-frostbite-to-pb)
- [MSH file format](https://gmsh.info/doc/texinfo/gmsh.html#MSH-file-format)
- [Optimized Spatial Hashing for Collision Detection of Deformable Objects](https://matthias-research.github.io/pages/publications/tetraederCollision.pdf)
- [Optimizing GPU occupancy and resource usage with large thread groups](https://gpuopen.com/learn/optimizing-gpu-occupancy-resource-usage-large-thread-groups/)
- [Particle Simulation using CUDA](https://developer.download.nvidia.com/assets/cuda/files/particles.pdf)
- [Physically Based Rendering in Filament](https://google.github.io/filament/Filament.md.html)
- [Physically Based Shading at Disney](https://disneyanimation.com/publications/physically-based-shading-at-disney/)
- [Real Shading in Unreal Engine 4](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)
- [Shadow Map Antialiasing](https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing)
- [Small Steps in Physics Simulation](http://mmacklin.com/smallsteps.pdf)
- [Vulkan 1.3 Specification](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/)