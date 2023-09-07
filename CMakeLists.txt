cmake_minimum_required(VERSION 3.25)
project(demo)

# CMake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# CPM
include(cmake/CPM.cmake)

# Debug
add_compile_definitions($<$<CONFIG:DEBUG>:DEBUG>)

# Vulkan
add_compile_definitions(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
add_compile_definitions(VULKAN_HPP_NO_CONSTRUCTORS)
find_package(Vulkan REQUIRED COMPONENTS dxc)

# MSVC
if(MSVC)
    add_compile_options(/wd5054)
else()
    add_compile_options(-fms-extensions)
endif()

# GLFW
CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.8
    OPTIONS
        "GLFW_BUILD_DOCS OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_EXAMPLES OFF"
)

# OpenGL Mathematics (GLM)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(GLM_FORCE_RADIANS)
CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 0.9.9.8
)

# Dear ImGui
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.89.9
    DOWNLOAD_ONLY
)
include_directories(${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
add_library(imgui
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
    ${imgui_SOURCE_DIR}/imconfig.h
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_internal.h
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imstb_rectpack.h
    ${imgui_SOURCE_DIR}/imstb_textedit.h
    ${imgui_SOURCE_DIR}/imstb_truetype.h
)
target_link_libraries(imgui Vulkan::Vulkan glfw)

# KTX (Khronos Texture)
CPMAddPackage(
    NAME ktx
    GITHUB_REPOSITORY KhronosGroup/KTX-Software
    GIT_TAG v4.2.1
    OPTIONS
        "KTX_FEATURE_STATIC_LIBRARY ON"
        "KTX_FEATURE_TESTS OFF"
        "KTX_FEATURE_TOOLS OFF"
        "KTX_FEATURE_GL_UPLOAD OFF"
        "KTX_FEATURE_VK_UPLOAD OFF"
)

# MikkTSpace
CPMAddPackage(
    NAME MikkTSpace
    VERSION 3e895b4
    GITHUB_REPOSITORY mmikk/MikkTSpace
    GIT_TAG 3e895b4
    DOWNLOAD_ONLY
)
include_directories(${MikkTSpace_SOURCE_DIR})
add_library(MikkTSpace
    ${MikkTSpace_SOURCE_DIR}/mikktspace.c
    ${MikkTSpace_SOURCE_DIR}/mikktspace.h
)

# MshIO
CPMAddPackage(
    NAME mshio
    GITHUB_REPOSITORY qnzhou/MshIO
    GIT_TAG v0.0.1
)

# SoLoud
CPMAddPackage(
    NAME SoLoud
    VERSION 20200207
    GITHUB_REPOSITORY jarikomppa/soloud
    GIT_TAG RELEASE_20200207
    DOWNLOAD_ONLY
)
add_compile_definitions(WITH_MINIAUDIO)
include_directories(${SoLoud_SOURCE_DIR}/include)
add_library(SoLoud
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/dr_flac.h
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/dr_impl.cpp
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/dr_mp3.h
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/dr_wav.h
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/soloud_wav.cpp
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/soloud_wavstream.cpp
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/stb_vorbis.c
    ${SoLoud_SOURCE_DIR}/src/audiosource/wav/stb_vorbis.h
    ${SoLoud_SOURCE_DIR}/src/backend/miniaudio/miniaudio.h
    ${SoLoud_SOURCE_DIR}/src/backend/miniaudio/soloud_miniaudio.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_audiosource.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_bus.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_3d.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_basicops.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_faderops.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_filterops.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_getters.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_setters.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_voicegroup.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_core_voiceops.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_fader.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_fft.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_fft_lut.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_file.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_filter.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_misc.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_queue.cpp
    ${SoLoud_SOURCE_DIR}/src/core/soloud_thread.cpp
)

# stb
CPMAddPackage(
    NAME stb
    VERSION 5736b15
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG 5736b15
    DOWNLOAD_ONLY
)
include_directories(${stb_SOURCE_DIR})

# tinygltf
add_compile_definitions(TINYGLTF_NO_EXTERNAL_IMAGE)
add_compile_definitions(TINYGLTF_NO_STB_IMAGE)
add_compile_definitions(TINYGLTF_NO_STB_IMAGE_WRITE)
add_compile_definitions(TINYGLTF_USE_CPP14)
CPMAddPackage(
    NAME tinygltf
    GITHUB_REPOSITORY syoyo/tinygltf
    GIT_TAG v2.8.15
    OPTIONS
        "TINYGLTF_BUILD_LOADER_EXAMPLE OFF"
        "TINYGLTF_HEADER_ONLY ON"
        "TINYGLTF_INSTALL OFF"
)

# tinyobjloader
CPMAddPackage(
    NAME tinyobjloader
    GITHUB_REPOSITORY tinyobjloader/tinyobjloader
    GIT_TAG v1.0.6
)

# Vulkan Memory Allocator
CPMAddPackage(
    NAME VulkanMemoryAllocator
    GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG v3.0.1
)
CPMAddPackage(
    NAME VulkanMemoryAllocator_Hpp
    GITHUB_REPOSITORY ruberith/VulkanMemoryAllocator-Hpp
    GIT_TAG v3.0.1
    DOWNLOAD_ONLY
)
include_directories(${VulkanMemoryAllocator_Hpp_SOURCE_DIR}/include)

# demo
add_executable(demo
    demo/main.cpp
    demo/Audio.cpp
    demo/Audio.h
    demo/Buffer.h
    demo/Camera.cpp
    demo/Camera.h
    demo/Data.h
    demo/Demo.cpp
    demo/Demo.h
    demo/Descriptor.h
    demo/Engine.cpp
    demo/Engine.h
    demo/GLFW.cpp
    demo/GLFW.h
    demo/GPU.cpp
    demo/GPU.h
    demo/GUI.cpp
    demo/GUI.h
    demo/Image.cpp
    demo/Image.h
    demo/Model.h
    demo/Player.cpp
    demo/Player.h
    demo/Shader.cpp
    demo/Shader.h
    demo/Storage.h
    demo/SurfaceMesh.h
    demo/TangentSpace.h
    demo/Uniform.h
    demo/Utils.h
    demo/VersionNumber.h
    demo/Vertex.h
    demo/Vulkan.cpp
    demo/VulkanDescriptors.cpp
    demo/VulkanMemory.cpp
    demo/VulkanModels.cpp
    demo/VulkanPipelines.cpp
    demo/VulkanRender.cpp
    demo/VulkanSim.cpp
    demo/Vulkan.h
)
target_link_libraries(demo
    Vulkan::Vulkan
    Vulkan::dxc_lib
    glfw
    glm::glm
    imgui
    ktx
    MikkTSpace
    mshio
    SoLoud
    tinygltf
    tinyobjloader
    VulkanMemoryAllocator
)

# Resources
set(RESOURCE_FILES
    README.md
)
set_source_files_properties(${RESOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${RESOURCE_FILES})
target_sources(demo PRIVATE ${RESOURCE_FILES})