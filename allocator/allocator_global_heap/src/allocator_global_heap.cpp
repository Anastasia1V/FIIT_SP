#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap()
{
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    if (size == 0) {
        size = 1;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    void *ptr = ::operator new(size);
    return ptr;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    if (at == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    ::operator delete(at);
}

allocator_global_heap::~allocator_global_heap()
{
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other)
{
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept
{
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    return *this;
}
