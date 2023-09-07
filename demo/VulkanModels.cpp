#include "Vulkan.h"

#include "TangentSpace.h"
#include "Vertex.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

namespace fs = std::filesystem;
using namespace vk;

void Vulkan::loadModel(const std::string& name, const std::string& extension, const glm::float3& translation,
                       const glm::quat& rotation, const glm::float3& scale)
{
    // Load the glTF model.
    bool success;
    tinygltf::TinyGLTF tinyGLTF;
    tinygltf::Model _model;
    std::string error, warning;
    const std::string path = modelPath(name, extension).string();
    if (extension == "gltf")
    {
        success = tinyGLTF.LoadASCIIFromFile(&_model, &error, &warning, path);
    }
    else if (extension == "glb")
    {
        success = tinyGLTF.LoadBinaryFromFile(&_model, &error, &warning, path);
    }
    else
    {
        throw std::invalid_argument("Failed to load model [" + name + "]: unsupported extension " + extension);
    }
    if (!warning.empty())
    {
        std::clog << warning << std::endl;
    }
    if (!success)
    {
        throw std::runtime_error("Failed to load model [" + name + "]" + (error.empty() ? "" : (": " + error)));
    }
    _models.emplace_back(_model);

    // Create the 3D model.
    Model model{
        .name = name,
        .translation = translation,
        .rotation = rotation,
        .scale = scale,
    };
    modelNamesToIndices[name] = models.size();

    // Create the meshes.
    model.meshes.reserve(_model.meshes.size());
    for (const tinygltf::Mesh& _mesh : _model.meshes)
    {
        Model::Mesh mesh;
        mesh.name = _mesh.name;
        const auto& extras = _mesh.extras;
        bool deformable{};
        if (extras.Has("deformable"))
        {
            deformable = extras.Get("deformable").Get<bool>();
        }
        if (deformable)
        {
            // Prepare the mesh for simulation.
            float compliance{};
            if (extras.Has("compliance"))
            {
                compliance = extras.Get("compliance").GetNumberAsDouble();
            }
            float density{1000.0f};
            if (extras.Has("density"))
            {
                density = extras.Get("density").GetNumberAsDouble();
            }
            std::vector<uint32_t> staticNodes{};
            if (extras.Has("static"))
            {
                const auto& staticValues = extras.Get("static");
                const uint32_t staticCount = staticValues.ArrayLen();
                staticNodes.reserve(staticCount);
                for (uint32_t i = 0; i < staticCount; i++)
                {
                    staticNodes.emplace_back(staticValues.Get(i).GetNumberAsInt());
                }
            }
            mesh.embedding = loadMesh(model.name, mesh.name, compliance, density, staticNodes,
                                      compose(translation, rotation, scale));
        }
        if (_mesh.primitives.size() != 1)
        {
            throw std::runtime_error("Failed to load model [" + model.name + "]: unsupported primitive count");
        }
        const tinygltf::Primitive& _primitive = _mesh.primitives[0];
        // Calculate and reserve vertex sizes.
        mesh.vertexCount = _model.accessors[_primitive.attributes.at("POSITION")].count;
        if (mesh.vertexCount > maxVertexCount)
        {
            maxVertexCount = mesh.vertexCount;
        }
        mesh.positionOffset = vertexBufferSize;
        vertexBufferSize += mesh.vertexCount * sizeof(Vertex::position);
        mesh.normalOffset = vertexBufferSize;
        vertexBufferSize += mesh.vertexCount * sizeof(Vertex::normal);
        mesh.tangentOffset = vertexBufferSize;
        vertexBufferSize += mesh.vertexCount * sizeof(Vertex::tangent);
        for (uint32_t i = 0; i < 5; i++)
        {
            if (_primitive.attributes.contains("TEXCOORD_" + std::to_string(i)))
            {
                mesh.texcoordOffset[i] = vertexBufferSize;
                vertexBufferSize += mesh.vertexCount * sizeof(Vertex::texcoord[i]);
            }
        }
        if (_primitive.attributes.contains("COLOR_0"))
        {
            mesh.colorOffset = vertexBufferSize;
            vertexBufferSize += mesh.vertexCount * sizeof(Vertex::color);
        }
        if (mesh.embedding != MeshEmbedding::none || _primitive.attributes.contains("JOINTS_0"))
        {
            mesh.jointsOffset = vertexBufferSize;
            vertexBufferSize += mesh.vertexCount * sizeof(Vertex::joints);
        }
        if (mesh.embedding != MeshEmbedding::none || _primitive.attributes.contains("WEIGHTS_0"))
        {
            mesh.weightsOffset = vertexBufferSize;
            vertexBufferSize += mesh.vertexCount * sizeof(Vertex::weights);
        }
        // Calculate and reserve index sizes.
        if (_primitive.indices != -1)
        {
            const tinygltf::Accessor& _accessor = _model.accessors[_primitive.indices];
            mesh.indexCount = _accessor.count;
            mesh.indexOffset = indexBufferSize;
            indexBufferSize += mesh.indexCount * tinygltf::GetComponentSizeInBytes(_accessor.componentType);
        }

        model.meshes.emplace_back(mesh);
    }

    // Create the skins.
    model.skins.reserve(_model.skins.size());
    for (const tinygltf::Skin& _skin : _model.skins)
    {
        Model::Skin skin;
        // Calculate and reserve uniform sizes.
        skin.uniformOffset = varUniformBufferSize;
        varUniformBufferSize += alignedSkinUniformSize;
        model.skins.emplace_back(skin);
    }
    skinCount += model.skins.size();

    // Create the nodes.
    model.nodes.reserve(_model.nodes.size());
    for (const tinygltf::Node& _node : _model.nodes)
    {
        Model::Node node;
        model.nodes.emplace_back(node);
    }

    // Create the materials.
    model.materials.reserve(_model.materials.size());
    for (const tinygltf::Material& _material : _model.materials)
    {
        Model::Material material;
        const auto& extras = _material.extras;
        if (extras.Has("variable"))
        {
            material.variable = extras.Get("variable").Get<bool>();
        }
        // Calculate and reserve uniform sizes.
        if (material.variable)
        {
            material.uniformOffset = varUniformBufferSize;
            varUniformBufferSize += alignedMaterialUniformSize;
        }
        else
        {
            material.uniformOffset = constUniformBufferSize;
            constUniformBufferSize += alignedMaterialUniformSize;
        }
        model.materials.emplace_back(material);
    }
    materialCount += model.materials.size();

    // Create the animations.
    model.animations.reserve(_model.animations.size());
    for (const tinygltf::Animation& _animation : _model.animations)
    {
        Model::Animation animation;
        animation.channels = {_animation.channels.size(), Model::Animation::Channel{}};
        animation.samplers = {_animation.samplers.size(), Model::Animation::Sampler{}};
        model.animations.emplace_back(animation);
    }

    models.emplace_back(model);
}

void Vulkan::initializeModels()
{
    SMikkTSpaceInterface tsInterface = TangentSpace::Interface();

    setupGraphics();
    for (uint32_t i = 0; i < models.size(); i++)
    {
        tinygltf::Model& _model = _models[i];
        Model& model = models[i];

        // Initialize the nodes.
        for (uint32_t j = 0; j < model.nodes.size(); j++)
        {
            const tinygltf::Node& _node = _model.nodes[j];
            Model::Node& node = model.nodes[j];
            node.name = _node.name;
            model.nodeNamesToIndices[node.name] = j;
            if (_node.matrix.size() == 16)
            {
                decompose(glm::make_mat4(_node.matrix.data()), node.translation, node.rotation, node.scale);
            }
            if (_node.translation.size() == 3)
            {
                node.translation = glm::make_vec3(_node.translation.data());
            }
            if (_node.rotation.size() == 4)
            {
                node.rotation = glm::normalize(glm::make_quat(_node.rotation.data()));
            }
            if (_node.scale.size() == 3)
            {
                node.scale = glm::make_vec3(_node.scale.data());
            }
            if (_node.mesh != -1)
            {
                node.mesh = &model.meshes[_node.mesh];
                node.mesh->nodes.emplace_back(&node);
            }
            if (_node.skin != -1)
            {
                node.skin = &model.skins[_node.skin];
            }
            for (int k : _node.children)
            {
                Model::Node& child = model.nodes[k];
                node.children.emplace_back(&child);
                child.parent = &node;
            }
        }
        if (_model.defaultScene != -1)
        {
            model.root = &model.nodes[_model.scenes[_model.defaultScene].nodes[0]];
        }
        else if (!model.nodes.empty())
        {
            model.root = &model.nodes[0];
        }
        model.update();

        // Initialize the samplers.
        model.samplers.reserve(_model.samplers.size());
        for (const tinygltf::Sampler& _sampler : _model.samplers)
        {
            SamplerCreateInfo sampler = gpu.sampler();
            if (_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
            {
                sampler.magFilter = Filter::eNearest;
            }
            if (_sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ||
                _sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ||
                _sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR)
            {
                sampler.minFilter = Filter::eNearest;
            }
            if (_sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ||
                _sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
            {
                sampler.mipmapMode = SamplerMipmapMode::eNearest;
            }
            if (_sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT)
            {
                sampler.addressModeU = SamplerAddressMode::eMirroredRepeat;
            }
            else if (_sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE)
            {
                sampler.addressModeU = SamplerAddressMode::eClampToEdge;
            }
            if (_sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT)
            {
                sampler.addressModeV = sampler.addressModeW = SamplerAddressMode::eMirroredRepeat;
            }
            else if (_sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE)
            {
                sampler.addressModeV = sampler.addressModeW = SamplerAddressMode::eClampToEdge;
            }
            model.samplers.emplace_back(device.createSampler(sampler));
        }

        // Load the image data.
        std::vector<std::vector<ImageData>> images;
        images.reserve(_model.images.size());
        for (const tinygltf::Image& _image : _model.images)
        {
            const std::string name = fs::path(_image.uri).stem().string();
            images.emplace_back(ImageData::loadKhronosTexture(name, model.name)[0]);
        }
        model.images.resize(images.size());
        std::vector<bool> initializedImage(images.size(), false);

        // Load the texture infos.
        struct Texture
        {
            vk::Sampler& sampler;
            uint32_t image;
        };
        std::vector<Texture> textures;
        textures.reserve(_model.textures.size());
        for (const tinygltf::Texture& _texture : _model.textures)
        {
            if (_texture.source == -1)
            {
                throw std::runtime_error("Failed to initialize model [" + model.name + "]: texture without source");
            }
            textures.emplace_back(Texture{
                .sampler = (_texture.sampler == -1) ? linearRepeatAniSampler : model.samplers[_texture.sampler],
                .image = static_cast<uint32_t>(_texture.source),
            });
        }

        // Initialize the textures and the materials.
        for (uint32_t j = 0; j < model.materials.size(); j++)
        {
            const tinygltf::Material& _material = _model.materials[j];
            Model::Material& material = model.materials[j];
            material.name = _material.name;
            model.materialNamesToIndices[material.name] = j;
            material.descSets = initDescriptorSets(materialDescLayout);
            const auto initializeMaterialTexture = [&](uint32_t index, uint32_t binding,
                                                       ColorSpace colorSpace = ColorSpace::sRGB) -> void {
                uint32_t image = textures[index].image;
                if (!initializedImage[image])
                {
                    model.images[image] = initTextureMipmap(ImageUsageFlagBits::eSampled, images[image], colorSpace);
                    initializedImage[image] = true;
                }
                for (DescriptorSet& set : material.descSets)
                {
                    setCombinedImageSampler(textures[index].sampler, model.images[image], set, binding);
                }
            };
            const auto initializeWhiteTexture = [&](uint32_t binding) -> void {
                for (DescriptorSet& set : material.descSets)
                {
                    setCombinedImageSampler(linearRepeatSampler, whiteImage, set, binding);
                }
            };
            // pbrSpecularGlossiness
            if (_material.extensions.contains("KHR_materials_pbrSpecularGlossiness"))
            {
                material.uniform.workflow = static_cast<glm::uint>(MaterialUniform::PbrWorkflow::SpecularGlossiness);
                const auto& pbrSpecularGlossiness = _material.extensions.at("KHR_materials_pbrSpecularGlossiness");
                // diffuseFactor
                if (pbrSpecularGlossiness.Has("diffuseFactor"))
                {
                    const auto& diffuseFactor = pbrSpecularGlossiness.Get("diffuseFactor");
                    for (uint32_t i = 0; i < 4; i++)
                    {
                        material.uniform.colorFactor[i] = diffuseFactor.Get(i).GetNumberAsDouble();
                    }
                }
                // diffuseTexture
                if (pbrSpecularGlossiness.Has("diffuseTexture"))
                {
                    const auto& diffuseTexture = pbrSpecularGlossiness.Get("diffuseTexture");
                    uint32_t index = diffuseTexture.Get("index").GetNumberAsInt();
                    initializeMaterialTexture(index, 0, ColorSpace::sRGB);
                    if (diffuseTexture.Has("texCoord"))
                    {
                        material.uniform.colorTexcoord = diffuseTexture.Get("texCoord").GetNumberAsInt();
                    }
                }
                else
                {
                    initializeWhiteTexture(0);
                }
                // specularFactor
                if (pbrSpecularGlossiness.Has("specularFactor"))
                {
                    const auto& specularFactor = pbrSpecularGlossiness.Get("specularFactor");
                    for (uint32_t i = 0; i < 3; i++)
                    {
                        material.uniform.pbrFactor[i] = specularFactor.Get(i).GetNumberAsDouble();
                    }
                }
                // glossinessFactor
                if (pbrSpecularGlossiness.Has("glossinessFactor"))
                {
                    const auto& glossinessFactor = pbrSpecularGlossiness.Get("glossinessFactor");
                    material.uniform.pbrFactor[3] = glossinessFactor.GetNumberAsDouble();
                }
                // specularGlossinessTexture
                if (pbrSpecularGlossiness.Has("specularGlossinessTexture"))
                {
                    const auto& specularGlossinessTexture = pbrSpecularGlossiness.Get("specularGlossinessTexture");
                    uint32_t index = specularGlossinessTexture.Get("index").GetNumberAsInt();
                    initializeMaterialTexture(index, 1, ColorSpace::sRGB);
                    if (specularGlossinessTexture.Has("texCoord"))
                    {
                        material.uniform.pbrTexcoord = specularGlossinessTexture.Get("texCoord").GetNumberAsInt();
                    }
                }
                else
                {
                    initializeWhiteTexture(1);
                }
            }
            // pbrMetallicRoughness
            else
            {
                material.uniform.workflow = static_cast<glm::uint>(MaterialUniform::PbrWorkflow::MetallicRoughness);
                const tinygltf::PbrMetallicRoughness& pbrMetallicRoughness = _material.pbrMetallicRoughness;
                // baseColorFactor
                for (uint32_t i = 0; i < 4; i++)
                {
                    material.uniform.colorFactor[i] = pbrMetallicRoughness.baseColorFactor[i];
                }
                // baseColorTexture
                if (pbrMetallicRoughness.baseColorTexture.index != -1)
                {
                    initializeMaterialTexture(pbrMetallicRoughness.baseColorTexture.index, 0, ColorSpace::sRGB);
                }
                else
                {
                    initializeWhiteTexture(0);
                }
                material.uniform.colorTexcoord = pbrMetallicRoughness.baseColorTexture.texCoord;
                // metallicFactor
                material.uniform.pbrFactor.b = pbrMetallicRoughness.metallicFactor;
                // roughnessFactor
                material.uniform.pbrFactor.g = pbrMetallicRoughness.roughnessFactor;
                // metallicRoughnessTexture
                if (pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
                {
                    initializeMaterialTexture(pbrMetallicRoughness.metallicRoughnessTexture.index, 1,
                                              ColorSpace::linear);
                }
                else
                {
                    initializeWhiteTexture(1);
                }
                material.uniform.pbrTexcoord = pbrMetallicRoughness.metallicRoughnessTexture.texCoord;
            }
            // normalTexture
            if (_material.normalTexture.index != -1)
            {
                initializeMaterialTexture(_material.normalTexture.index, 2, ColorSpace::linear);
            }
            else
            {
                for (DescriptorSet& set : material.descSets)
                {
                    setCombinedImageSampler(linearRepeatSampler, blueImage, set, 2);
                }
            }
            material.uniform.normalTexcoord = _material.normalTexture.texCoord;
            material.uniform.normalScale = _material.normalTexture.scale;
            // occlusionTexture
            if (_material.occlusionTexture.index != -1)
            {
                initializeMaterialTexture(_material.occlusionTexture.index, 3, ColorSpace::linear);
            }
            else
            {
                initializeWhiteTexture(3);
            }
            material.uniform.occlusionTexcoord = _material.occlusionTexture.texCoord;
            material.uniform.occlusionStrength = _material.occlusionTexture.strength;
            // emissiveTexture
            if (_material.emissiveTexture.index != -1)
            {
                initializeMaterialTexture(_material.emissiveTexture.index, 4, ColorSpace::sRGB);
            }
            else
            {
                initializeWhiteTexture(4);
            }
            material.uniform.emissiveTexcoord = _material.emissiveTexture.texCoord;
            // emissiveFactor
            for (uint32_t i = 0; i < 3; i++)
            {
                material.uniform.emissiveFactor[i] = _material.emissiveFactor[i];
            }

            // Initialize the uniforms.
            if (material.variable)
            {
                for (uint32_t k = 0; k < frameCount; k++)
                {
                    setUniformBuffer(varUniformBuffers[k], material.uniformOffset, sizeof(MaterialUniform),
                                     material.descSets[k], 5);
                    varUniformBuffers[k].set(material.uniform, material.uniformOffset);
                }
            }
            else
            {
                fillBuffer(constUniformBuffer, Data::of(material.uniform), material.uniformOffset);
                for (DescriptorSet& set : material.descSets)
                {
                    setUniformBuffer(constUniformBuffer, material.uniformOffset, sizeof(MaterialUniform), set, 5);
                }
            }
        }

        // Initialize the meshes.
        for (uint32_t j = 0; j < model.meshes.size(); j++)
        {
            const tinygltf::Mesh& _mesh = _model.meshes[j];
            Model::Mesh& mesh = model.meshes[j];
            model.meshNamesToIndices[mesh.name] = j;
            const tinygltf::Primitive& _primitive = _mesh.primitives[0];
            TangentSpace::UserData tsData;
            if (_primitive.material != -1)
            {
                mesh.material = &model.materials[_primitive.material];
            }
            else
            {
                throw std::runtime_error("Failed to initialize model [" + model.name + "]: missing material");
            }
            if (_primitive.mode != TINYGLTF_MODE_TRIANGLES)
            {
                throw std::runtime_error("Failed to initialize model [" + model.name +
                                         "]: unsupported primitive topology");
            }
            // Initialize the indices.
            if (_primitive.indices != -1)
            {
                const tinygltf::Accessor& _accessor = _model.accessors[_primitive.indices];
                tsData.faceCount = mesh.indexCount / 3;
                switch (_accessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    mesh.indexType = IndexType::eUint32;
                    tsData.indexSize = 4;
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    mesh.indexType = IndexType::eUint16;
                    tsData.indexSize = 2;
                    break;
                default:
                    throw std::runtime_error("Failed to initialize model [" + model.name + "]: unsupported index type");
                }
                const tinygltf::BufferView& _bufferView = _model.bufferViews[_accessor.bufferView];
                tinygltf::Buffer& _buffer = _model.buffers[_bufferView.buffer];
                tsData.indices = Data{
                    .data = &_buffer.data[_accessor.byteOffset + _bufferView.byteOffset],
                    .size = mesh.indexCount * tsData.indexSize,
                };
                fillBuffer(indexBuffer, tsData.indices, mesh.indexOffset);
            }
            else
            {
                throw std::runtime_error("Failed to initialize model [" + model.name + "]: missing indices");
            }
            // Initialize the vertex attributes.
            const auto initializeVertexAttribute =
                [&]<typename srcValT, typename dstVecT>(const std::string& name, DeviceSize offset,
                                                        dstVecT (*make_vec)(const srcValT* const)) -> void {
                const tinygltf::Accessor& _accessor = _model.accessors[_primitive.attributes.at(name)];
                const tinygltf::BufferView& _bufferView = _model.bufferViews[_accessor.bufferView];
                tinygltf::Buffer& _buffer = _model.buffers[_bufferView.buffer];
                const size_t stride = _accessor.ByteStride(_bufferView) / sizeof(srcValT);
                const size_t size = mesh.vertexCount * sizeof(dstVecT);
                Data data = Data::allocate(size);
                srcValT* src = reinterpret_cast<srcValT*>(&_buffer.data[_accessor.byteOffset + _bufferView.byteOffset]);
                dstVecT* dst = static_cast<dstVecT*>(data());
                for (uint32_t k = 0; k < mesh.vertexCount; k++)
                {
                    dst[k] = make_vec(&src[k * stride]);
                }
                if (name == "POSITION" || name == "NORMAL" ||
                    name == ("TEXCOORD_" + std::to_string(mesh.material->uniform.normalTexcoord)))
                {
                    data.alloc = false;
                    if (name == "POSITION")
                        tsData.positions = data;
                    else if (name == "NORMAL")
                        tsData.normals = data;
                    else
                        tsData.texcoords = data;
                }
                fillBuffer(vertexBuffer, data, offset);
            };
            // position
            if (_primitive.attributes.contains("POSITION"))
            {
                initializeVertexAttribute("POSITION", mesh.positionOffset, glm::make_vec3<float>);
            }
            else
            {
                throw std::runtime_error("Failed to initialize model [" + model.name + "]: missing positions");
            }
            // normal
            if (_primitive.attributes.contains("NORMAL"))
            {
                initializeVertexAttribute("NORMAL", mesh.normalOffset, glm::make_vec3<float>);
            }
            else
            {
                throw std::runtime_error("Failed to initialize model [" + model.name + "]: missing normals");
            }
            // texcoord
            for (uint32_t k = 0; k < 5; k++)
            {
                const std::string name = "TEXCOORD_" + std::to_string(k);
                if (_primitive.attributes.contains(name))
                {
                    const tinygltf::Accessor& _accessor = _model.accessors[_primitive.attributes.at(name)];
                    switch (_accessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        initializeVertexAttribute(name, mesh.texcoordOffset[k], glm::make_vec2<float>);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        initializeVertexAttribute(
                            name, mesh.texcoordOffset[k], +[](const uint8_t* const ptr) -> glm::float2 {
                                return glm::float2(glm::make_vec2(ptr)) / 255.0f;
                            });
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        initializeVertexAttribute(
                            name, mesh.texcoordOffset[k], +[](const uint16_t* const ptr) -> glm::float2 {
                                return glm::float2(glm::make_vec2(ptr)) / 65535.0f;
                            });
                        break;
                    default:
                        throw std::runtime_error("Failed to initialize model [" + model.name +
                                                 "]: unsupported texcoord type");
                    }
                }
                else
                {
                    mesh.texcoordOffset[k] = whiteVertexOffset;
                }
            }
            // tangent
            tsData.tangents = Data::allocate(mesh.vertexCount * sizeof(Vertex::tangent));
            TangentSpace::generate(tsInterface, tsData);
            fillBuffer(vertexBuffer, tsData.tangents, mesh.tangentOffset);
            if (mesh.embedding == MeshEmbedding::none)
            {
                tsData.positions.free();
            }
            tsData.normals.free();
            tsData.texcoords.free();
            // color
            if (_primitive.attributes.contains("COLOR_0"))
            {
                const tinygltf::Accessor& _accessor = _model.accessors[_primitive.attributes.at("COLOR_0")];
                const bool hasAlpha = (_accessor.type == TINYGLTF_TYPE_VEC4);
                switch (_accessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    initializeVertexAttribute(
                        "COLOR_0", mesh.colorOffset,
                        hasAlpha ? +[](const float* const ptr) -> glm::float4 { return glm::make_vec4(ptr); }
                                 : +[](const float* const ptr) -> glm::float4 {
                                       return {glm::make_vec3(ptr), 1.0f};
                                   });
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    initializeVertexAttribute(
                        "COLOR_0", mesh.colorOffset,
                        hasAlpha ? +[](const uint8_t* const ptr)
                                       -> glm::float4 { return glm::float4(glm::make_vec4(ptr)) / 255.0f; }
                                 : +[](const uint8_t* const ptr) -> glm::float4 {
                                       return {glm::float3(glm::make_vec3(ptr)) / 255.0f, 1.0f};
                                   });
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    initializeVertexAttribute(
                        "COLOR_0", mesh.colorOffset,
                        hasAlpha ? +[](const uint16_t* const ptr)
                                       -> glm::float4 { return glm::float4(glm::make_vec4(ptr)) / 65535.0f; }
                                 : +[](const uint16_t* const ptr) -> glm::float4 {
                                       return {glm::float3(glm::make_vec3(ptr)) / 65535.0f, 1.0f};
                                   });
                    break;
                default:
                    throw std::runtime_error("Failed to initialize model [" + model.name + "]: unsupported color type");
                }
            }
            else
            {
                mesh.colorOffset = whiteVertexOffset;
            }
            if (mesh.embedding != MeshEmbedding::none)
            {
                // Generate the mesh embedding.
                Data jointData, weightData;
                embedMesh(model.name, mesh.name, tsData.positions, jointData, weightData);
                fillBuffer(vertexBuffer, jointData, mesh.jointsOffset);
                fillBuffer(vertexBuffer, weightData, mesh.weightsOffset);
                tsData.positions.free();
            }
            else
            {
                // joints
                if (_primitive.attributes.contains("JOINTS_0"))
                {
                    const tinygltf::Accessor& _accessor = _model.accessors[_primitive.attributes.at("JOINTS_0")];
                    switch (_accessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        initializeVertexAttribute(
                            "JOINTS_0", mesh.jointsOffset,
                            +[](const uint8_t* const ptr) -> glm::uvec4 { return glm::make_vec4(ptr); });
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        initializeVertexAttribute(
                            "JOINTS_0", mesh.jointsOffset,
                            +[](const uint16_t* const ptr) -> glm::uvec4 { return glm::make_vec4(ptr); });
                        break;
                    default:
                        throw std::runtime_error("Failed to initialize model [" + model.name +
                                                 "]: unsupported joints type");
                    }
                }
                else
                {
                    mesh.jointsOffset = whiteVertexOffset;
                }
                // weights
                if (_primitive.attributes.contains("WEIGHTS_0"))
                {
                    const tinygltf::Accessor& _accessor = _model.accessors[_primitive.attributes.at("WEIGHTS_0")];
                    switch (_accessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        initializeVertexAttribute("WEIGHTS_0", mesh.weightsOffset, glm::make_vec4<float>);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        initializeVertexAttribute(
                            "WEIGHTS_0", mesh.weightsOffset, +[](const uint8_t* const ptr) -> glm::float4 {
                                return glm::float4(glm::make_vec4(ptr)) / 255.0f;
                            });
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        initializeVertexAttribute(
                            "WEIGHTS_0", mesh.weightsOffset, +[](const uint16_t* const ptr) -> glm::float4 {
                                return glm::float4(glm::make_vec4(ptr)) / 65535.0f;
                            });
                        break;
                    default:
                        throw std::runtime_error("Failed to initialize model [" + model.name +
                                                 "]: unsupported weights type");
                    }
                }
                else
                {
                    mesh.weightsOffset = whiteVertexOffset;
                }
            }
        }

        // Initialize the node back references.
        for (uint32_t j = 0; j < model.nodes.size(); j++)
        {
            const tinygltf::Node& _node = _model.nodes[j];
            Model::Node& node = model.nodes[j];
            if (_node.mesh != -1)
            {
                node.mesh->material->nodes.emplace_back(&node);
            }
        }

        // Initialize the skins.
        for (uint32_t j = 0; j < model.skins.size(); j++)
        {
            const tinygltf::Skin& _skin = _model.skins[j];
            Model::Skin& skin = model.skins[j];
            skin.descSets = initDescriptorSets(skinDescLayout);
            for (uint32_t k = 0; k < frameCount; k++)
            {
                setUniformBuffer(varUniformBuffers[k], skin.uniformOffset, sizeof(SkinUniform), skin.descSets[k], 0);
                skin.uniform.jointCount = _skin.joints.size();
                varUniformBuffers[k].set(skin.uniform, skin.uniformOffset);
            }
            if (_skin.joints.size() > 128)
            {
                throw std::runtime_error("Failed to initialize model [" + model.name + "]: unsupported joint count");
            }
            skin.joints.reserve(_skin.joints.size());
            for (int k : _skin.joints)
            {
                skin.joints.emplace_back(&model.nodes[k]);
            }
            const tinygltf::Accessor& _accessor = _model.accessors[_skin.inverseBindMatrices];
            const tinygltf::BufferView& _bufferView = _model.bufferViews[_accessor.bufferView];
            tinygltf::Buffer& _buffer = _model.buffers[_bufferView.buffer];
            const size_t stride = _accessor.ByteStride(_bufferView) / sizeof(float);
            float* src = reinterpret_cast<float*>(&_buffer.data[_accessor.byteOffset + _bufferView.byteOffset]);
            skin.inverseBind.reserve(skin.joints.size());
            for (uint32_t k = 0; k < skin.joints.size(); k++)
            {
                skin.inverseBind.emplace_back(glm::make_mat4(&src[k * stride]));
            }
        }

        // Initialize the animations.
        for (uint32_t j = 0; j < model.animations.size(); j++)
        {
            const tinygltf::Animation& _animation = _model.animations[j];
            Model::Animation& animation = model.animations[j];
            animation.name = _animation.name;
            model.animationNamesToIndices[animation.name] = j;
            animation.model = &model;
            // Initialize the animation channels.
            for (uint32_t k = 0; k < animation.channels.size(); k++)
            {
                const tinygltf::AnimationChannel& _channel = _animation.channels[k];
                Model::Animation::Channel& channel = animation.channels[k];
                channel.sampler = &animation.samplers[_channel.sampler];
                if (_channel.target_node != -1)
                {
                    channel.node = &model.nodes[_channel.target_node];
                }
                else
                {
                    throw std::runtime_error("Failed to initialize model [" + model.name + "]: missing target node");
                }
                if (_channel.target_path == "translation")
                {
                    channel.path = Model::Animation::Channel::Path::translation;
                }
                else if (_channel.target_path == "rotation")
                {
                    channel.path = Model::Animation::Channel::Path::rotation;
                }
                else if (_channel.target_path == "scale")
                {
                    channel.path = Model::Animation::Channel::Path::scale;
                }
                else
                {
                    throw std::runtime_error("Failed to initialize model [" + model.name +
                                             "]: unsupported target path");
                }
            }
            // Initialize the animation samplers.
            for (uint32_t k = 0; k < animation.samplers.size(); k++)
            {
                const tinygltf::AnimationSampler& _sampler = _animation.samplers[k];
                Model::Animation::Sampler& sampler = animation.samplers[k];
                {
                    const tinygltf::Accessor& _accessor = _model.accessors[_sampler.input];
                    const tinygltf::BufferView& _bufferView = _model.bufferViews[_accessor.bufferView];
                    tinygltf::Buffer& _buffer = _model.buffers[_bufferView.buffer];
                    const size_t stride = _accessor.ByteStride(_bufferView) / sizeof(float);
                    float* src = reinterpret_cast<float*>(&_buffer.data[_accessor.byteOffset + _bufferView.byteOffset]);
                    sampler.times.reserve(_accessor.count);
                    for (uint32_t l = 0; l < _accessor.count; l++)
                    {
                        sampler.times.emplace_back(src[l * stride]);
                    }
                }
                if (_sampler.interpolation == "LINEAR")
                {
                    sampler.interpolation = Model::Animation::Sampler::Interpolation::linear;
                }
                else if (_sampler.interpolation == "STEP")
                {
                    sampler.interpolation = Model::Animation::Sampler::Interpolation::step;
                }
                else
                {
                    throw std::runtime_error("Failed to initialize model [" + model.name +
                                             "]: unsupported sampler interpolation algorithm");
                }
                {
                    const tinygltf::Accessor& _accessor = _model.accessors[_sampler.output];
                    const tinygltf::BufferView& _bufferView = _model.bufferViews[_accessor.bufferView];
                    tinygltf::Buffer& _buffer = _model.buffers[_bufferView.buffer];
                    const size_t stride = _accessor.ByteStride(_bufferView) / sizeof(float);
                    float* src = reinterpret_cast<float*>(&_buffer.data[_accessor.byteOffset + _bufferView.byteOffset]);
                    sampler.values.reserve(_accessor.count);
                    if (_accessor.type == TINYGLTF_TYPE_VEC3)
                    {
                        for (uint32_t l = 0; l < _accessor.count; l++)
                        {
                            sampler.values.emplace_back(glm::float4(glm::make_vec3(&src[l * stride]), 1.0f));
                        }
                    }
                    else if (_accessor.type == TINYGLTF_TYPE_VEC4)
                    {
                        for (uint32_t l = 0; l < _accessor.count; l++)
                        {
                            sampler.values.emplace_back(glm::make_vec4(&src[l * stride]));
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Failed to initialize model [" + model.name +
                                                 "]: unsupported sampler value type");
                    }
                }
                if (k == 0 || sampler.times.front() < animation.start)
                {
                    animation.start = sampler.times.front();
                }
                if (k == 0 || sampler.times.back() > animation.end)
                {
                    animation.end = sampler.times.back();
                }
            }
            animation.time = animation.start;
        }
    }
    playGraphics();

    // Destroy the glTF models.
    _models.clear();
}

Vulkan::Model& Vulkan::getModel(const std::string& name)
{
    return models[modelNamesToIndices[name]];
}

void Vulkan::destroyModel(Model& model)
{
    for (Sampler& sampler : model.samplers)
    {
        device.destroySampler(sampler);
    }
    for (AllocatedImage& image : model.images)
    {
        destroyImage(image);
    }
}