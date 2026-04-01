#pragma once
#include <cstddef>
#include <cstdlib>
#include <memory>

namespace vega
{

/// Simple arena allocator for temporary per-frame or per-tile allocations.
class Arena
{
public:
    explicit Arena(size_t capacity)
        : capacity_(capacity)
        , size_(0)
        , data_(static_cast<char*>(std::malloc(capacity)))
    {
    }

    ~Arena() { std::free(data_); }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& other) noexcept
        : capacity_(other.capacity_), size_(other.size_), data_(other.data_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    void* alloc(size_t bytes, size_t alignment = alignof(std::max_align_t))
    {
        size_t aligned = (size_ + alignment - 1) & ~(alignment - 1);
        if (aligned + bytes > capacity_)
            return nullptr;
        void* ptr = data_ + aligned;
        size_ = aligned + bytes;
        return ptr;
    }

    template <typename T>
    T* alloc_array(size_t count)
    {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    void reset() { size_ = 0; }
    size_t used() const { return size_; }
    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    size_t size_;
    char* data_;
};

} // namespace vega
