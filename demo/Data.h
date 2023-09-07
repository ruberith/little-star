#pragma once

#include <vector>

namespace Size
{
// Return the size of the given vector.
template <typename T, typename A> inline constexpr size_t of(std::vector<T, A>& v)
{
    return v.size() * sizeof(T);
}

// Return the size of the given object.
template <typename T> inline constexpr size_t of(T& t)
{
    return sizeof(t);
}
} // namespace Size

// raw data
struct Data
{
    // data pointer
    void* data{};
    // data size
    size_t size{};
    // Was the data dynamically allocated and must be freed?
    bool alloc{};

    // Return the data pointer.
    void* operator()()
    {
        return data;
    }

    // Construct a Data object from the given vector.
    template <typename T, typename A> static Data of(std::vector<T, A>& v)
    {
        return Data{
            .data = v.data(),
            .size = Size::of(v),
        };
    }

    // Construct a Data object from the given object.
    template <typename T> static Data of(T& t)
    {
        return Data{
            .data = &t,
            .size = Size::of(t),
        };
    }

    // Allocate memory for data of the given size.
    static Data allocate(size_t size)
    {
        Data data{
            .data = std::malloc(size),
            .size = size,
            .alloc = true,
        };
        if (!data())
        {
            throw std::bad_alloc();
        }
        return data;
    }

    // Allocate zero-initialized memory of the given size.
    static Data zero(size_t size)
    {
        Data data{
            .data = std::calloc(1, size),
            .size = size,
            .alloc = true,
        };
        if (!data())
        {
            throw std::bad_alloc();
        }
        return data;
    }

    // Allocate memory to initialize an array of constant value.
    template <typename T> static Data set(size_t count, const T& value)
    {
        Data data = Data::allocate(count * sizeof(T));
        T* dst = static_cast<T*>(data());
        for (size_t i = 0; i < count; i++)
        {
            dst[i] = value;
        }
        return data;
    }

    // Deallocate the memory.
    void free()
    {
        std::free(data);
        data = {};
        size = {};
        alloc = {};
    }
};