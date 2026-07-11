#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace cacheflow {

class PoolArena {
public:
    explicit PoolArena(std::size_t chunk_size = 1 << 20)
        : chunk_size_(chunk_size) {}

    void* allocate_bytes(std::size_t size, std::size_t alignment) {
        if (size == 0) {
            size = 1;
        }

        if (chunks_.empty() || !fits_in_current_chunk(size, alignment)) {
            allocate_chunk(std::max(chunk_size_, size + alignment));
        }

        auto& chunk = chunks_.back();
        auto base = reinterpret_cast<std::uintptr_t>(chunk.get() + used_);
        auto aligned = align_up(base, alignment);
        used_ = static_cast<std::size_t>(aligned - reinterpret_cast<std::uintptr_t>(chunk.get()) + size);
        return reinterpret_cast<void*>(aligned);
    }

private:
    bool fits_in_current_chunk(std::size_t size, std::size_t alignment) const {
        const auto& chunk = chunks_.back();
        auto base = reinterpret_cast<std::uintptr_t>(chunk.get() + used_);
        auto aligned = align_up(base, alignment);
        const auto consumed = static_cast<std::size_t>(aligned - reinterpret_cast<std::uintptr_t>(chunk.get()) + size);
        return consumed <= chunk_size_;
    }

    static std::uintptr_t align_up(std::uintptr_t value, std::size_t alignment) {
        const auto mask = static_cast<std::uintptr_t>(alignment - 1);
        return (value + mask) & ~mask;
    }

    void allocate_chunk(std::size_t size) {
        chunks_.push_back(std::unique_ptr<std::byte[]>(new std::byte[size]));
        chunk_size_ = size;
        used_ = 0;
    }

    std::size_t chunk_size_;
    std::vector<std::unique_ptr<std::byte[]>> chunks_;
    std::size_t used_{0};
};

template <typename T>
class PoolAllocator {
public:
    using value_type = T;

    PoolAllocator() noexcept : arena_(nullptr) {}
    explicit PoolAllocator(PoolArena* arena) noexcept : arena_(arena) {}

    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other) noexcept : arena_(other.arena_) {}

    T* allocate(std::size_t n) {
        if (arena_ == nullptr) {
            return static_cast<T*>(::operator new(n * sizeof(T)));
        }

        return static_cast<T*>(arena_->allocate_bytes(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* pointer, std::size_t) noexcept {
        if (arena_ == nullptr) {
            ::operator delete(pointer);
        }
    }

    template <typename U>
    bool operator==(const PoolAllocator<U>& other) const noexcept {
        return arena_ == other.arena_;
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

    template <typename>
    friend class PoolAllocator;

private:
    PoolArena* arena_;
};

} // namespace cacheflow