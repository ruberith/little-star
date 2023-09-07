#pragma once

#include "Buffer.h"
#include "GPU.h"
#include "Image.h"
#include "Model.h"
#include "Shader.h"
#include "Storage.h"
#include <tiny_gltf.h>

class Engine;
union DescriptorInfo;

// graphics/compute system
class Vulkan
{
  private:
    // engine parent
    Engine& engine;
    // parallel render frame / simulation update count
    static constexpr uint32_t frameCount{2};
    // 3D model
    using Model = Model<frameCount>;
    // star particle count
    static constexpr uint32_t starParticleCount{8192};
    // star particle radius
    static constexpr float starParticleRadius{0.05f};
    // attachment count
    static constexpr uint32_t attachmentCount{2};
    // XPBD substep count
    static constexpr uint32_t substepCount{20};
    // XPBD substep delta time
    const float substepDeltaTime;
    // shadow resolution
    static constexpr uint32_t shadowRes{8192};

  public:
    // star position
    static constexpr glm::float3 starPosition{0.0f, 30.0f, 0.0f};

    // === Vulkan.cpp ==============================================================================================
  private:
    // result
    vk::Result result;
    // instance
    vk::Instance instance;
    // debug messenger
    vk::DebugUtilsMessengerEXT debugUtilsMessenger;
    // surface
    vk::SurfaceKHR surface;
    // GPU (physical device)
    GPU gpu;
    // logical device
    vk::Device device;
    // device queues
    vk::Queue graphicsQueue, computeQueue, transferQueue, presentQueue;
    // Is the ... queue distinct?
    bool computeQueueDistinct{}, transferQueueDistinct{}, presentQueueDistinct{};
    // memory allocator
    vma::Allocator allocator;
    // command pools
    vk::CommandPool graphicsPool, transferPool;
    std::array<vk::CommandPool, frameCount> simPools, renderPools;
    // command buffers
    vk::CommandBuffer graphicsBuffer, transferBuffer;
    std::array<vk::CommandBuffer, frameCount> simBuffers, renderBuffers;
    // aligned uniform sizes
    vk::DeviceSize alignedMaterialUniformSize{}, alignedSkinUniformSize{};
    // uniform buffer sizes
    vk::DeviceSize constUniformBufferSize{}, varUniformBufferSize{};
    // uniform buffer offsets
    vk::DeviceSize playerCollisionUniformOffset{-1u}, attachmentOffset{-1u}, sceneUniformOffset{-1u},
        cameraTransformUniformOffset{-1u}, lightTransformUniformOffset{-1u}, viewProjectionUniformOffset{-1u};
    // uniform buffers
    AllocatedBuffer constUniformBuffer;
    std::array<AllocatedBuffer, frameCount> varUniformBuffers;
    // vertex buffer size
    vk::DeviceSize vertexBufferSize{};
    // vertex buffer offsets
    vk::DeviceSize particleVertexOffset{-1u}, skyboxVertexOffset{-1u}, whiteVertexOffset{-1u};
    // maximum vertex count of all meshes
    uint32_t maxVertexCount{};
    // index buffer size
    vk::DeviceSize indexBufferSize{};
    // index buffer offsets
    vk::DeviceSize particleIndexOffset{-1u}, skyboxIndexOffset{-1u};
    // index counts
    uint32_t particleIndexCount{}, skyboxIndexCount{};
    // mesh buffers
    AllocatedBuffer vertexBuffer, indexBuffer;
    std::array<AllocatedBuffer, frameCount> guiVertexBuffers, guiIndexBuffers;
    // samplers
    vk::Sampler linearClampSampler, linearClampAniSampler, linearRepeatSampler, linearRepeatAniSampler,
        nearestClampSampler, shadowSampler;
    // sampled images
    AllocatedImage brdfImage, irradianceImage, radianceImage, whiteImage, blueImage, skyboxImage, fontImage,
        shadowImage;
    // semaphores
    vk::Semaphore simComplete;
    std::array<vk::Semaphore, frameCount> imageAcquired, renderComplete;
    // fences
    std::array<vk::Fence, frameCount> updateInFlight, frameInFlight;

  public:
    // Construct the Vulkan object given the engine.
    Vulkan(Engine& engine);
    // Wait for the device to idle.
    void deviceWaitIdle();
    // Destruct the Vulkan object.
    ~Vulkan();
    // Handle a debug message.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                          const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                          void* userData);

    // === VulkanDescriptors.cpp ===================================================================================
  private:
    // descriptor set count
    uint32_t descSetCount{};
    // descriptor pool sizes
    std::vector<vk::DescriptorPoolSize> descPoolSizes;
    // descriptor set layouts
    vk::DescriptorSetLayout starUpdateDescLayout, spatialHashDescLayout, spatialSortDescLayout,
        spatialGroupsortDescLayout, spatialCollectDescLayout, xpbdPredictDescLayout, xpbdObjcollDescLayout,
        xpbdPcollDescLayout, xpbdDistDescLayout, xpbdVolDescLayout, xpbdCorrectDescLayout, depthDescLayout,
        sceneDescLayout, materialDescLayout, skinDescLayout, particleDescLayout, skyboxDescLayout, postDescLayout,
        guiDescLayout;
    // descriptor pool
    vk::DescriptorPool descPool;

    // Initialize a descriptor set layout from the given bindings.
    // Optionally specify how many descriptor sets should be reserved for each frame.
    vk::DescriptorSetLayout initDescriptorSetLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                                                    uint32_t count = 1);
    // Initialize the descriptor pool.
    void initializeDescriptorPool();
    // Initialize a descriptor set from the given layout.
    vk::DescriptorSet initDescriptorSet(vk::DescriptorSetLayout& layout);
    // Initialize descriptor sets for each frame from the given layout.
    std::array<vk::DescriptorSet, frameCount> initDescriptorSets(vk::DescriptorSetLayout& layout);
    // Set the specified descriptor.
    void setDescriptor(DescriptorInfo info, vk::DescriptorType type, vk::DescriptorSet& set, uint32_t binding,
                       uint32_t arrayElement = 0);
    // Set the specified combined image sampler descriptor.
    void setCombinedImageSampler(const vk::Sampler& sampler, AllocatedImage& image, vk::DescriptorSet& set,
                                 uint32_t binding, uint32_t arrayElement = 0);
    // Set the specified sampled image descriptor.
    void setSampledImage(AllocatedImage& image, vk::DescriptorSet& set, uint32_t binding, uint32_t arrayElement = 0);
    // Set the specified uniform buffer descriptor.
    void setUniformBuffer(AllocatedBuffer& buffer, vk::DeviceSize offset, vk::DeviceSize range, vk::DescriptorSet& set,
                          uint32_t binding, uint32_t arrayElement = 0);
    // Set the specified storage buffer descriptor.
    void setStorageBuffer(AllocatedBuffer& buffer, vk::DeviceSize offset, vk::DeviceSize range, vk::DescriptorSet& set,
                          uint32_t binding, uint32_t arrayElement = 0);
    // Set the specified storage buffer descriptor. The whole storage buffer will be used.
    void setStorageBuffer(AllocatedBuffer& buffer, vk::DescriptorSet& set, uint32_t binding, uint32_t arrayElement = 0);

    // === VulkanMemory.cpp ========================================================================================
  private:
    // active command buffer
    vk::CommandBuffer activeBuffer{};
    // staging buffers
    std::vector<AllocatedBuffer> stagingBuffers;

    // Activate the given command buffer. All commands are recorded to the active command buffer.
    void activate(vk::CommandBuffer& commandBuffer);
    // Setup command recording by allocating memory from the pool and activating the buffer.
    void setup(vk::CommandPool& commandPool, vk::CommandBuffer& commandBuffer);
    // Play the recorded commands by submitting the buffer to the queue and returning the memory to the pool.
    void play(vk::Queue& queue, vk::CommandPool& commandPool, vk::CommandBuffer& commandBuffer);
    // Setup the graphics buffer for recording.
    void setupGraphics();
    // Play the recorded graphics commands.
    void playGraphics();
    // Setup the transfer buffer for recording.
    void setupTransfer();
    // Play the recorded transfer commands.
    void playTransfer();
    // Create a buffer.
    AllocatedBuffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags bufferUsage,
                                 vma::AllocationCreateFlags flags = {},
                                 vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto);
    // Create a staging buffer.
    AllocatedBuffer createStagingBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage);
    // Map the memory of the given buffer.
    void mapBuffer(AllocatedBuffer& buffer);
    // Unmap the memory of the given buffer.
    void unmapBuffer(AllocatedBuffer& buffer);
    // Fill the staging buffer with data.
    void fillStagingBuffer(AllocatedBuffer& buffer, Data data, vk::DeviceSize offset = 0);
    // Fill the staging buffer with data from multiple sources. Optionally align the data.
    void fillStagingBuffer(AllocatedBuffer& buffer, std::vector<Data> data, bool align = false);
    // Initialize a staging buffer with data.
    AllocatedBuffer initStagingBuffer(vk::BufferUsageFlags usage, Data data);
    // Create a readback buffer.
    AllocatedBuffer createReadbackBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage);
    // Copy the source buffer to the destination buffer.
    void copyBuffer(AllocatedBuffer& srcBuffer, AllocatedBuffer& dstBuffer, vk::DeviceSize offset = 0);
    // Fill the buffer with data.
    void fillBuffer(AllocatedBuffer& buffer, Data data, vk::DeviceSize offset = 0);
    // Initialize a buffer with data.
    AllocatedBuffer initBuffer(vk::BufferUsageFlags usage, Data data);
    // Initialize a buffer with data from multiple sources.
    AllocatedBuffer initBuffer(vk::BufferUsageFlags usage, std::vector<Data> data);
    // Clear the given buffer entirely or partially.
    void clearBuffer(AllocatedBuffer& buffer, uint32_t value = 0, vk::DeviceSize offset = 0,
                     vk::DeviceSize size = VK_WHOLE_SIZE);
    // Synchronize access to the given buffer.
    void syncBufferAccess(AllocatedBuffer& buffer, vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccess,
                          vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccess, vk::DeviceSize offset = 0,
                          vk::DeviceSize size = VK_WHOLE_SIZE);
    // Destroy the given buffer.
    void destroyBuffer(AllocatedBuffer& buffer);
    // Create an image view.
    vk::ImageView createImageView(vk::Image& image, vk::ImageViewType type, vk::Format format, uint32_t mipLevels = 1,
                                  uint32_t layers = 1);
    // Create an image.
    AllocatedImage createImage(vk::ImageViewType type, uint32_t width, uint32_t height, uint32_t mipLevels,
                               uint32_t layers, vk::SampleCountFlagBits samples, vk::Format format,
                               vk::ImageTiling tiling, vk::ImageUsageFlags imageUsage,
                               vma::AllocationCreateFlags flags = {},
                               vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto);
    // Transition the layout of the given image.
    void transitionImageLayout(AllocatedImage& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                               uint32_t mipLevel = 0, uint32_t levelCount = 0);
    // Copy the buffer to the image.
    void copyBufferToImage(AllocatedBuffer& buffer, AllocatedImage& image, uint32_t width, uint32_t height,
                           uint32_t mipLevel = 0, uint32_t layer = 0);
    // Copy the image to the buffer.
    void copyImageToBuffer(AllocatedImage& image, AllocatedBuffer& buffer, uint32_t width, uint32_t height,
                           uint32_t mipLevel = 0, uint32_t layer = 0);
    // Fill the image with data.
    void fillImage(AllocatedImage& image, ImageData data, uint32_t mipLevel = 0, uint32_t layer = 0);
    // Initialize a texture image with data.
    AllocatedImage initTextureImage(vk::ImageUsageFlags usage, ImageData data,
                                    ColorSpace colorSpace = ColorSpace::sRGB);
    // Initialize a texture image with a constant color.
    AllocatedImage initTextureImage(vk::ImageUsageFlags usage, const glm::u8vec4& color);
    // Initialize a texture mipmap.
    AllocatedImage initTextureMipmap(vk::ImageUsageFlags usage, std::vector<ImageData> data,
                                     ColorSpace colorSpace = ColorSpace::sRGB);
    // Initialize a texture cubemap.
    AllocatedImage initTextureCubemap(vk::ImageUsageFlags usage, std::vector<std::vector<ImageData>> data,
                                      ColorSpace colorSpace = ColorSpace::sRGB);
    // Destroy the given image.
    void destroyImage(AllocatedImage& image);
    // Blit the given image, i.e. copy the source mip level to the destination mip level with scaling.
    void blitImage(AllocatedImage& image, uint32_t srcMipLevel, int32_t srcWidth, int32_t srcHeight,
                   uint32_t dstMipLevel, int32_t dstWidth, int32_t dstHeight);

    // === VulkanModels.cpp ========================================================================================
  private:
    // glTF models
    std::vector<tinygltf::Model> _models;
    // 3D models
    std::vector<Model> models;
    // model name => model index
    std::unordered_map<std::string, uint32_t> modelNamesToIndices;
    // skin count
    uint32_t skinCount{};
    // material count
    uint32_t materialCount{};

    // Load a model and define its initial transformation.
    void loadModel(const std::string& name, const std::string& extension = "gltf", const glm::float3& translation = {},
                   const glm::quat& rotation = {1.0f, 0.0f, 0.0f, 0.0f}, const glm::float3& scale = glm::float3{1.0f});
    // Initialize the models.
    void initializeModels();
    // Get the specified model.
    Model& getModel(const std::string& name);
    // Destroy the given model.
    void destroyModel(Model& model);

    // === VulkanPipelines.cpp =====================================================================================
  private:
    // shader compiler
    ShaderCompiler shaderCompiler;
    // shader modules
    std::vector<vk::ShaderModule> shaderModules;
    // pipeline layouts
    vk::PipelineLayout starUpdatePipelineLayout, spatialHashPipelineLayout, spatialSortPipelineLayout,
        spatialGroupsortPipelineLayout, spatialCollectPipelineLayout, xpbdPredictPipelineLayout,
        xpbdObjcollPipelineLayout, xpbdPcollPipelineLayout, xpbdDistPipelineLayout, xpbdVolPipelineLayout,
        xpbdCorrectPipelineLayout, depthPipelineLayout, particleDepthPipelineLayout, lightingPipelineLayout,
        particlePipelineLayout, skyboxPipelineLayout, postPipelineLayout, guiPipelineLayout;
    // pipelines
    vk::Pipeline starUpdatePipeline, spatialHashPipeline, spatialSortPipeline, spatialGroupsortPipeline,
        spatialCollectPipeline, xpbdPredictPipeline, xpbdObjcollPipeline, xpbdPcollPipeline, xpbdDistPipeline,
        xpbdVolPipeline, xpbdCorrectPipeline, depthPipeline, particleDepthPipeline, lightingPipeline, particlePipeline,
        skyboxPipeline, postPipeline, guiPipeline, shadowPipeline;
    // descriptor sets
    std::array<vk::DescriptorSet, frameCount> starUpdateDescSets, spatialHashDescSets, spatialSortDescSets,
        spatialGroupsortDescSets, spatialCollectDescSets, xpbdPredictDescSets, xpbdObjcollDescSets, xpbdPcollDescSets,
        xpbdDistDescSets, xpbdVolDescSets, xpbdCorrectDescSets, depthDescSets, shadowDescSets, sceneDescSets,
        inactiveSkinDescSets, particleDescSets, skyboxDescSets, postDescSets, guiDescSets;

    // Initialize the given shaders.
    std::vector<vk::PipelineShaderStageCreateInfo> initializeShaders(std::vector<Shader>& shaders);
    // Initialize the star update pipeline.
    void initializeStarUpdatePipeline();
    // Initialize the spatial hash pipeline.
    void initializeSpatialHashPipeline();
    // Initialize the spatial sort pipeline.
    void initializeSpatialSortPipeline();
    // Initialize the spatial groupsort pipeline.
    void initializeSpatialGroupsortPipeline();
    // Initialize the spatial collect pipeline.
    void initializeSpatialCollectPipeline();
    // Initialize the XPBD predict pipeline.
    void initializeXpbdPredictPipeline();
    // Initialize the XPBD object collide pipeline.
    void initializeXpbdObjcollPipeline();
    // Initialize the XPBD particle collide pipeline.
    void initializeXpbdPcollPipeline();
    // Initialize the XPBD distance constrain pipeline.
    void initializeXpbdDistPipeline();
    // Initialize the XPBD volume constrain pipeline.
    void initializeXpbdVolPipeline();
    // Initialize the XPBD correct pipeline.
    void initializeXpbdCorrectPipeline();
    // Initialize the depth pipeline.
    void initializeDepthPipeline();
    // Initialize the particle depth pipeline.
    void initializeParticleDepthPipeline();
    // Initialize the lighting pipeline.
    void initializeLightingPipeline();
    // Initialize the particle pipeline.
    void initializeParticlePipeline();
    // Initialize the skybox pipeline.
    void initializeSkyboxPipeline();
    // Initialize the post pipeline.
    void initializePostPipeline();

  public:
    // Initialize the GUI pipeline.
    void initializeGuiPipeline();

    // === VulkanRender.cpp ========================================================================================
  private:
    // swapchain image count
    uint32_t swapchainMinImageCount{}, swapchainImageCount{};
    // swapchain format
    vk::Format swapchainFormat;
    // swapchain extent
    vk::Extent2D swapchainExtent;
    // swapchain
    vk::SwapchainKHR swapchain;
    // swapchain images
    std::vector<AllocatedImage> swapchainImages;
    // MSAA sample count
    vk::SampleCountFlagBits msaaSamples;
    // depth format
    vk::Format depthFormat;
    // attached images
    AllocatedImage colorImage, colorResolveImage, depthImage, depthResolveImage, normalImage, normalResolveImage;
    // cinematic bar height
    float bar{0.1f};
    // frame brightness
    float brightness{0.0f};
    // scene uniform
    SceneUniform scene{};
    // transform uniforms
    TransformUniform cameraTransform{}, lightTransform{};
    // view-projection uniform
    ViewProjectionUniform viewProjection{};
    // skybox model-view-projection matrix
    glm::float4x4 skyboxModelViewProjection{1.0f};
    // render frame index
    uint32_t frameIndex{};

    // Initialize the swapchain.
    void initializeSwapchain();
    // Update the scene.
    void updateScene();
    // Record the rendering commands to the given render buffer.
    void recordRendering(vk::CommandBuffer& renderBuffer, AllocatedImage& swapchainImage);
    // Record the GUI rendering commands to the given render buffer.
    void renderGui(vk::CommandBuffer& renderBuffer);
    // Recreate the swapchain.
    void recreateSwapchain();
    // Terminate the swapchain.
    void terminateSwapchain();

  public:
    // star regeneration progress
    float starProgress{};
    // Enable cel shading?
    bool cel{true};
    // Has the framebuffer been resized?
    bool resizedFramebuffer{};

    // Render the next frame.
    void render();

    // === VulkanSim.cpp ===========================================================================================
  private:
    // storage data
    struct
    {
        // storage buffer offsets
        struct
        {
            vk::DeviceSize x{-1u}, x_{-1u}, dx{-1u}, dxE7{-1u}, v{-1u}, spat{-1u}, cell{-1u}, r{-1u}, w{-1u},
                state{-1u}, distConstr{-1u}, volConstr{-1u};
        } offset{};
        // storage data sizes
        struct
        {
            vk::DeviceSize x{}, x_{}, dx{}, dxE7{}, v{}, spat{}, cell{}, r{}, w{}, state{}, distConstr{}, volConstr{};
        } size{};
        // particle positions
        std::vector<glm::float4> x{};
        // particle velocities
        std::vector<glm::float4> v{};
        // particle radii
        std::vector<float> r{};
        // particle weights (= inverse masses)
        std::vector<float> w{};
        // particle states
        std::vector<glm::uint> state{};
        // distance constraints
        std::vector<DistanceConstraint> distConstr{};
        // volume constraints
        std::vector<VolumeConstraint> volConstr{};
    } storage{};
    // mesh data
    struct Mesh
    {
        // Is the mesh tetrahedral or triangular?
        bool tet{};
        // element node indices
        std::vector<glm::uvec4> elements{};
        // maximum node radius
        float r_max{};
        // mean node distance
        float d_mean{};
    };
    // <model name>/<mesh name> => mesh data
    std::unordered_map<std::string, Mesh> _meshes{};
    // entity counts
    uint32_t particleCount{}, spatCount{}, distCount{}, volCount{};
    // workgroup dimensions
    WorkgroupDimensions starWorkgroup{}, particleWorkgroup{}, spatWorkgroup{}, sharedSpatWorkgroup{}, distWorkgroup{},
        volWorkgroup{};
    // maximum particle radius
    float r_max{};
    // spatial grid cell size
    float cellSize{};
    // storage buffer size
    vk::DeviceSize storageBufferSize{};
    // storage buffer
    AllocatedBuffer storageBuffer{};
    // counter buffer size
    vk::DeviceSize counterBufferSize{};
    // counter buffer
    AllocatedBuffer counterBuffer{};
    // player model nodes used for collision
    std::array<Model::Node*, 18> playerCollisionNodes{};
    // player collision uniform
    PlayerCollisionUniform playerCollision{};
    // player model nodes used for attachments
    std::array<Model::Node*, attachmentCount> playerAttachmentNodes{};
    // attachment particle indices (initial: node tags)
    std::array<uint32_t, attachmentCount> attachmentIndices{11533, 24011};
    // buffer copies used to update attachments
    std::array<vk::BufferCopy, attachmentCount> attachmentCopies{};
    // local attachment positions
    std::array<glm::float4, attachmentCount> attachmentDeltas{
        glm::float4{3.0f, -22.0f, -8.0f, 1.0f},
        glm::float4{-3.0f, -22.0f, -8.0f, 1.0f},
    };
    // attachment positions
    std::array<glm::float4, attachmentCount> attachmentPositions{};
    // Activate the star particles?
    bool starParticlesActive{};
    // Should the star attract all star particles?
    bool attractAllStarParticles{};
    // total simulation update count
    uint64_t updateCount{};
    // simulation update index
    uint32_t updateIndex{};

    // Generate the star particles.
    void generateStarParticles();
    // Load the specified mesh.
    MeshEmbedding loadMesh(const std::string& model, const std::string& mesh, float compliance = 0.0f,
                           float density = 1000.0f, const std::vector<uint32_t>& staticNodes = {},
                           const glm::float4x4& transformation = glm::float4x4{1.0f});
    // Embed the specified mesh. Fill the joint and weight data for barycentric skinning.
    void embedMesh(const std::string& model, const std::string& mesh, Data positionData, Data& jointData,
                   Data& weightData);
    // Initialize the simulation.
    void initializeSimulation();
    // Update the player.
    void updatePlayer();
    // Record the simulation commands to the given sim buffer.
    void recordSimulation(vk::CommandBuffer& simBuffer);

  public:
    // Simulate the next update.
    void sim();
};