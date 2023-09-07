#pragma once

#include "Data.h"
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

// Vulkan buffer + allocation
struct AllocatedBuffer
{
    // buffer handle
    vk::Buffer buffer;
    // memory allocation
    vma::Allocation allocation;
    // buffer size
    vk::DeviceSize size{};
    // buffer data when mapped
    void* data{};

    // Return the buffer handle.
    vk::Buffer& operator()()
    {
        return buffer;
    }

    // Interpret the mapped buffer data at the given offset as an object.
    // This should only be used to access buffers created with AllocationCreateFlagBits::eHostAccessRandom.
    template <typename T> T& as(vk::DeviceSize offset = 0)
    {
        if (!data)
        {
            throw std::runtime_error("Failed to access buffer data: buffer not mapped");
        }
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(data) + offset);
    }

    // Set the mapped buffer data at the given offset to the object data.
    // This is useful to fill buffers created with AllocationCreateFlagBits::eHostAccessSequentialWrite.
    template <typename T> AllocatedBuffer& set(T& t, vk::DeviceSize offset = 0)
    {
        if (!data)
        {
            throw std::runtime_error("Failed to set buffer data: buffer not mapped");
        }
        Data src = Data::of(t);
        memcpy(static_cast<uint8_t*>(data) + offset, src(), src.size);
        return *this;
    }
};