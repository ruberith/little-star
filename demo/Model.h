#pragma once

#include "Buffer.h"
#include "Image.h"
#include "Uniform.h"
#include "Utils.h"
#include <unordered_map>
#include <vulkan/vulkan.hpp>

// 3D model
template <uint32_t frameCount> struct Model
{
    struct Node;

    // material
    struct Material
    {
        // material name
        std::string name;
        // May the uniform data be updated?
        bool variable{};
        // uniform data
        MaterialUniform uniform;
        // uniform buffer offset
        vk::DeviceSize uniformOffset{-1u};
        // descriptor sets
        std::array<vk::DescriptorSet, frameCount> descSets;
        // material nodes
        std::vector<Node*> nodes;

        // Update the material uniform data in the given buffer.
        void update(AllocatedBuffer& buffer)
        {
            if (!variable)
            {
                throw std::runtime_error("Failed to update: material is not variable");
            }
            buffer.set(uniform, uniformOffset);
        }
    };

    // skin
    struct Skin
    {
        // uniform data
        SkinUniform uniform{};
        // uniform buffer offset
        vk::DeviceSize uniformOffset{-1u};
        // descriptor sets
        std::array<vk::DescriptorSet, frameCount> descSets;
        // inverse bind matrices
        std::vector<glm::float4x4> inverseBind;
        // joint nodes
        std::vector<Node*> joints;

        // Update the joint matrices and the skin uniform data in the given buffer.
        void update(AllocatedBuffer& buffer)
        {
            for (uint32_t i = 0; i < joints.size(); i++)
            {
                uniform.joint[i] = joints[i]->model * inverseBind[i];
            }
            buffer.set(uniform, uniformOffset);
        }
    };

    // mesh
    struct Mesh
    {
        // mesh name
        std::string name;
        // mesh embedding
        MeshEmbedding embedding{};
        // vertex count
        uint32_t vertexCount{};
        // position offset in vertex buffer
        vk::DeviceSize positionOffset{-1u};
        // normal offset in vertex buffer
        vk::DeviceSize normalOffset{-1u};
        // tangent offset in vertex buffer
        vk::DeviceSize tangentOffset{-1u};
        // texcoord offsets in vertex buffer
        std::array<vk::DeviceSize, 5> texcoordOffset{-1u, -1u, -1u, -1u, -1u};
        // color offset in vertex buffer
        vk::DeviceSize colorOffset{-1u};
        // joints offset in vertex buffer
        vk::DeviceSize jointsOffset{-1u};
        // weights offset in vertex buffer
        vk::DeviceSize weightsOffset{-1u};
        // index count
        uint32_t indexCount{};
        // index buffer offset
        vk::DeviceSize indexOffset{-1u};
        // index type
        vk::IndexType indexType{vk::IndexType::eUint32};
        // used material
        Material* material{};
        // mesh nodes
        std::vector<Node*> nodes;
    };

    // node
    struct Node
    {
        // node name
        std::string name;
        // model matrix
        glm::float4x4 model{1.0f};
        // translation vector
        glm::float3 translation{};
        // rotation quaternion
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        // scale vector
        glm::float3 scale{1.0f};
        // attached mesh
        Mesh* mesh{};
        // attached skin
        Skin* skin{};
        // parent node
        Node* parent{};
        // child nodes
        std::vector<Node*> children;

        // Update the model matrix. Recursively update the child nodes.
        void update()
        {
            model = compose(translation, rotation, scale);
            if (parent)
            {
                model = parent->model * model;
            }
            for (Node* child : children)
            {
                child->update();
            }
        }
    };

    // keyframe animation
    struct Animation
    {
        // model parent
        Model* model{};

        // animation sampler
        struct Sampler
        {
            // keyframe times
            std::vector<float> times;
            // interpolation type
            enum struct Interpolation
            {
                step,
                linear,
            } interpolation{Interpolation::linear};
            // keyframe values
            std::vector<glm::float4> values;
        };

        // animation channel
        struct Channel
        {
            // used sampler
            Sampler* sampler{};
            // animated node
            Node* node{};
            // animated property
            enum struct Path
            {
                translation,
                rotation,
                scale,
            } path{Path::translation};
        };

        // animation name
        std::string name;
        // animation time
        float time{};
        // start time
        float start{};
        // end time
        float end{};
        // animation channels
        std::vector<Channel> channels;
        // animation samplers
        std::vector<Sampler> samplers;

        // Update the animation using the given time step. Optionally loop the animation.
        void play(float deltaTime, bool loop = false)
        {
            using namespace glm;

            // Blend the animations upon change.
            if (model->activeAnimation != this)
            {
                model->activeAnimation = this;
                model->animationBlend = 0.0f;
            }
            model->animationBlend = saturate(model->animationBlend + 0.25f * deltaTime);

            // Keep the animation time in bounds.
            if (time < start || time > end)
            {
                if (loop)
                {
                    time = start + (time > end ? std::fmod(time - end, end - start) : 0.0f);
                }
                else
                {
                    time = std::clamp(time, start, end);
                }
            }

            // Interpolate between the keyframes and update the affected node properties.
            float3 translation;
            quat rotation;
            float3 scale;
            for (const Channel& channel : channels)
            {
                const Sampler& sampler = *channel.sampler;
                Node& node = *channel.node;
                auto high = std::upper_bound(sampler.times.begin(), sampler.times.end(), time);
                auto low = high - 1;
                if (low == sampler.times.end() || high == sampler.times.end())
                {
                    if (time <= sampler.times.front())
                    {
                        low = high = sampler.times.begin();
                    }
                    else
                    {
                        low = high = sampler.times.end() - 1;
                    }
                }
                const float _time = *low;
                const float4& _value = sampler.values[low - sampler.times.begin()];
                if (sampler.interpolation == Sampler::Interpolation::step || low == high)
                {
                    switch (channel.path)
                    {
                    case Channel::Path::translation:
                        translation = float3(_value);
                        break;
                    case Channel::Path::rotation:
                        rotation = normalize(quat(_value.w, _value.x, _value.y, _value.z));
                        break;
                    case Channel::Path::scale:
                        scale = float3(_value);
                        break;
                    }
                }
                else if (sampler.interpolation == Sampler::Interpolation::linear)
                {
                    const float time_ = *high;
                    const glm::float4& value_ = sampler.values[high - sampler.times.begin()];
                    const float t = (time - _time) / (time_ - _time);
                    switch (channel.path)
                    {
                    case Channel::Path::translation:
                        translation = lerp(float3(_value), float3(value_), t);
                        break;
                    case Channel::Path::rotation:
                        rotation = normalize(slerp(quat(_value.w, _value.x, _value.y, _value.z),
                                                   quat(value_.w, value_.x, value_.y, value_.z), t));
                        break;
                    case Channel::Path::scale:
                        scale = lerp(float3(_value), float3(value_), t);
                        break;
                    }
                }
                switch (channel.path)
                {
                case Channel::Path::translation:
                    node.translation = lerp(node.translation, translation, model->animationBlend);
                    break;
                case Channel::Path::rotation:
                    node.rotation = normalize(slerp(node.rotation, rotation, model->animationBlend));
                    break;
                case Channel::Path::scale:
                    node.scale = lerp(node.scale, scale, model->animationBlend);
                    break;
                }
            }

            // Update the animation time.
            time += deltaTime;
        }

        // Reset the animation time to the start time.
        void reset()
        {
            time = start;
        }
    };

    // model name
    std::string name;
    // translation vector
    glm::float3 translation{};
    // rotation quaternion
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    // scale vector
    glm::float3 scale{1.0f};
    // root node
    Node* root{};
    // meshes
    std::vector<Mesh> meshes;
    // skins
    std::vector<Skin> skins;
    // nodes
    std::vector<Node> nodes;
    // materials
    std::vector<Material> materials;
    // samplers
    std::vector<vk::Sampler> samplers;
    // images
    std::vector<AllocatedImage> images;
    // animations
    std::vector<Animation> animations;
    // active animation
    Animation* activeAnimation{};
    // animation blend value
    float animationBlend{};
    // node name => node index
    std::unordered_map<std::string, uint32_t> nodeNamesToIndices;
    // material name => material index
    std::unordered_map<std::string, uint32_t> materialNamesToIndices;
    // mesh name => mesh index
    std::unordered_map<std::string, uint32_t> meshNamesToIndices;
    // animation name => animation index
    std::unordered_map<std::string, uint32_t> animationNamesToIndices;

    // Return the specified node.
    Node& node(const std::string& name)
    {
        return nodes[nodeNamesToIndices[name]];
    }

    // Return the specified material.
    Material& material(const std::string& name)
    {
        return materials[materialNamesToIndices[name]];
    }

    // Return the specified mesh.
    Mesh& mesh(const std::string& name)
    {
        return meshes[meshNamesToIndices[name]];
    }

    // Return the specified animation.
    Animation& animation(const std::string& name)
    {
        return animations[animationNamesToIndices[name]];
    }

    // Update the model matrices of the nodes recursively, starting at the root node.
    void update()
    {
        if (root)
        {
            root->model =
                compose(translation, rotation, scale) * compose(root->translation, root->rotation, root->scale);
            for (Node* child : root->children)
            {
                child->update();
            }
        }
    }
};