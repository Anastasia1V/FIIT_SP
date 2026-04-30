#include <not_implemented.h>
#include "../include/allocator_boundary_tags.h"

static constexpr size_t METADATA_SIZE = sizeof(size_t) + sizeof(void *) * 3;
static constexpr size_t OCCUPIED_BIT = size_t(1) << (sizeof(size_t) * 8 - 1);

static size_t round_up(size_t object_size, size_t divisor)
{
    return (object_size + divisor - 1) / divisor * divisor;
}

static void calculate_offsets(size_t &parent_offset, size_t &fit_offset,
    size_t &size_offset, size_t &mutex_offset, size_t &head_offset, size_t &data_offset)
{
    parent_offset = 0;
    fit_offset = parent_offset + sizeof(std::pmr::memory_resource *);
    size_offset = round_up(fit_offset + sizeof(allocator_with_fit_mode::fit_mode), alignof(size_t));
    mutex_offset = round_up(size_offset + sizeof(size_t), alignof(std::mutex));
    head_offset = round_up(mutex_offset + sizeof(std::mutex), alignof(void *));
    data_offset = round_up(head_offset + sizeof(void *), alignof(std::max_align_t));
}

static void do_delete_allocator(void *allocator, std::pmr::memory_resource *parent, size_t space_size)
{
    if (allocator == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *allocator_ptr = reinterpret_cast<unsigned char *>(allocator);
    reinterpret_cast<std::mutex *>(allocator_ptr + mutex_offset)->~mutex();
    size_t total_size = data_offset + space_size;
    if (parent != nullptr) {
        parent->deallocate(allocator, total_size, alignof(std::max_align_t));
    } else {
        ::operator delete(allocator);
    }
}

static void delete_allocator(void *&allocator)
{
    if (allocator == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *allocator_ptr = reinterpret_cast<unsigned char *>(allocator);
    std::pmr::memory_resource *parent = nullptr;
    size_t space_size = 0;
    {
        std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(allocator_ptr + mutex_offset));
        parent = *reinterpret_cast<std::pmr::memory_resource **>(allocator_ptr + parent_offset);
        space_size = *reinterpret_cast<size_t *>(allocator_ptr + size_offset);
    }
    do_delete_allocator(allocator, parent, space_size);
    allocator = nullptr;
}

static void copy_allocator(const unsigned char *from, unsigned char *to,
    size_t total_size, size_t mutex_offset, size_t head_offset)
{
    std::copy_n(from, mutex_offset, to);
    new (to + mutex_offset) std::mutex();
    std::copy_n(from + mutex_offset + sizeof(std::mutex), total_size - (mutex_offset + sizeof(std::mutex)),
        to + mutex_offset + sizeof(std::mutex));
    const unsigned char *ptr_from = reinterpret_cast<const unsigned char *>(
        *reinterpret_cast<void *const *>(from + head_offset));
    if (ptr_from == nullptr) {
        *reinterpret_cast<void **>(to + head_offset) = nullptr;
        return;
    }
    *reinterpret_cast<void **>(to + head_offset) = to + (ptr_from - from);
    const unsigned char *current = ptr_from;
    while (current != nullptr) {
        unsigned char *ptr_to = to + (current - from);
        const void *previous = *reinterpret_cast<void *const *>(current + sizeof(size_t));
        if (previous != nullptr) {
            *reinterpret_cast<void **>(ptr_to + sizeof(size_t)) =
                to + (reinterpret_cast<const unsigned char *>(previous) - from);
        } else {
            *reinterpret_cast<void **>(ptr_to + sizeof(size_t)) = nullptr;
        }
        const void *next = *reinterpret_cast<void *const *>(current + sizeof(size_t) + sizeof(void *));
        if (next != nullptr) {
            *reinterpret_cast<void **>(ptr_to + sizeof(size_t) + sizeof(void *)) =
                to + (reinterpret_cast<const unsigned char *>(next) - from);
        } else {
            *reinterpret_cast<void **>(ptr_to + sizeof(size_t) + sizeof(void *)) = nullptr;
        }
        const void *reserved = *reinterpret_cast<void *const *>(current +
            sizeof(size_t) + sizeof(void *) * 2);
        if (reserved != nullptr) {
            *reinterpret_cast<void **>(ptr_to + sizeof(size_t) + sizeof(void *) * 2) =
                to + (reinterpret_cast<const unsigned char *>(reserved) - from);
        } else {
            *reinterpret_cast<void **>(ptr_to + sizeof(size_t) + sizeof(void *) * 2) = nullptr;
        }
        current = reinterpret_cast<const unsigned char *>(next);
    }
}

static void *copy_allocator_from(const void *other)
{
    if (other == nullptr) {
        return nullptr;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *ptr_other = reinterpret_cast<unsigned char *>(const_cast<void *>(other));
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(ptr_other + mutex_offset));
    size_t space_size = *reinterpret_cast<size_t *>(ptr_other + size_offset);
    if (space_size > std::numeric_limits<size_t>::max() - data_offset) {
        throw std::bad_alloc();
    }
    size_t total_size = data_offset + space_size;
    std::pmr::memory_resource *parent =
        *reinterpret_cast<std::pmr::memory_resource **>(ptr_other + parent_offset);
    void *allocator_ptr = nullptr;
    if (parent != nullptr) {
        allocator_ptr = parent->allocate(total_size, alignof(std::max_align_t));
    } else {
        allocator_ptr = ::operator new(total_size);
    }
    copy_allocator(ptr_other, reinterpret_cast<unsigned char *>(allocator_ptr),
        total_size, mutex_offset, head_offset);
    return allocator_ptr;
}

static void merge_blocks(void *allocator, unsigned char *block)
{
    if (allocator == nullptr || block == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *begin = reinterpret_cast<unsigned char *>(allocator) + data_offset;
    size_t space_size =
        *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(allocator) + size_offset);
    unsigned char *end = begin + space_size;
    while (true) {
        bool merged = false;
        unsigned char *previous = nullptr;
        if (block > begin) {
            size_t prev_size = *reinterpret_cast<size_t *>(block - sizeof(size_t)) & ~OCCUPIED_BIT;
            previous = block - prev_size;
            if (previous < begin || previous >= block) {
                previous = nullptr;
            }
        }
        if (previous != nullptr &&
            (*reinterpret_cast<const size_t *>(previous) & OCCUPIED_BIT) == 0) {
            size_t new_size = (*reinterpret_cast<size_t *>(previous) & ~OCCUPIED_BIT) +
                (*reinterpret_cast<size_t *>(block) & ~OCCUPIED_BIT);
            *reinterpret_cast<size_t *>(previous) = new_size;
            *reinterpret_cast<size_t *>(previous + new_size - sizeof(size_t)) = new_size;
            block = previous;
            merged = true;
        }
        size_t block_size = *reinterpret_cast<const size_t *>(block) & ~OCCUPIED_BIT;
        unsigned char *next_block = nullptr;
        unsigned char *ptr_next = block + block_size;
        if (ptr_next < end) {
            next_block = ptr_next;
        }
        if (next_block != nullptr && (*reinterpret_cast<const size_t *>(next_block) & OCCUPIED_BIT) == 0 &&
            block + block_size == next_block) {
            size_t new_size = block_size + (*reinterpret_cast<const size_t *>(next_block) & ~OCCUPIED_BIT);
            *reinterpret_cast<size_t *>(block) = new_size;
            *reinterpret_cast<size_t *>(block + new_size - sizeof(size_t)) = new_size;
            merged = true;
        }
        if (!merged) {
            break;
        }
    }
}

allocator_boundary_tags::~allocator_boundary_tags()
{
    delete_allocator(_trusted_memory);
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept
{
    _trusted_memory = nullptr;
    if (other._trusted_memory == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *ptr_other = reinterpret_cast<unsigned char *>(other._trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(ptr_other + mutex_offset));
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (other._trusted_memory == nullptr) {
        delete_allocator(_trusted_memory);
        return *this;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    if (_trusted_memory == nullptr) {
        unsigned char *ptr_other = reinterpret_cast<unsigned char *>(other._trusted_memory);
        std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(ptr_other + mutex_offset));
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
        return *this;
    }
    void *old_this = _trusted_memory;
    unsigned char *ptr_this = reinterpret_cast<unsigned char *>(_trusted_memory);
    unsigned char *ptr_other = reinterpret_cast<unsigned char *>(other._trusted_memory);
    {
        std::mutex &mutex_this = *reinterpret_cast<std::mutex *>(ptr_this + mutex_offset);
        std::mutex &mutex_other = *reinterpret_cast<std::mutex *>(ptr_other + mutex_offset);
        std::scoped_lock lock(mutex_this, mutex_other);
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    delete_allocator(old_this);
    return *this;
}

/** If parent_allocator* == nullptr you should use std::pmr::get_default_resource()
 */
allocator_boundary_tags::allocator_boundary_tags(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    if (space_size < METADATA_SIZE) {
        throw std::invalid_argument("Запрашиваемый размер должен быть больше размера блока метаданных");
    }
    if (space_size > std::numeric_limits<size_t>::max() - data_offset) {
        throw std::bad_alloc();
    }
    size_t total_size = data_offset + space_size;
    if (parent_allocator == nullptr) {
        parent_allocator = std::pmr::get_default_resource();
    }
    _trusted_memory = parent_allocator->allocate(total_size, alignof(std::max_align_t));
    try {
        unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
        *reinterpret_cast<std::pmr::memory_resource **>(memory_ptr + parent_offset) = parent_allocator;
        *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + fit_offset) = allocate_fit_mode;
        *reinterpret_cast<size_t *>(memory_ptr + size_offset) = space_size;
        new (memory_ptr + mutex_offset) std::mutex();
        unsigned char *first = memory_ptr + data_offset;
        *reinterpret_cast<void **>(memory_ptr + head_offset) = first;
        *reinterpret_cast<size_t *>(first) = space_size;
        *reinterpret_cast<size_t *>(first + space_size - sizeof(size_t)) = space_size;
        *reinterpret_cast<void **>(first + sizeof(size_t)) = nullptr;
        *reinterpret_cast<void **>(first + sizeof(size_t) + sizeof(void *)) = nullptr;
        *reinterpret_cast<void **>(first + sizeof(size_t) + sizeof(void *) * 2) = nullptr;
    }
    catch (std::bad_alloc) {
        parent_allocator->deallocate(_trusted_memory, total_size, alignof(std::max_align_t));
        _trusted_memory = nullptr;
        throw std::bad_alloc();
    }
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr) {
        throw std::runtime_error("Пустой аллокатор");
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    if (size > std::numeric_limits<size_t>::max() - METADATA_SIZE) {
        throw std::bad_alloc();
    }
    size_t total_size = size + METADATA_SIZE;
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    allocator_with_fit_mode::fit_mode mode =
        *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + fit_offset);
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *begin = memory_ptr + data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *block_ptr = nullptr;
    unsigned char *previous_ptr = nullptr;
    size_t block_size = 0;
    unsigned char *previous = nullptr;
    unsigned char *current = begin;
    while (current < end) {
        size_t current_size = *reinterpret_cast<const size_t *>(current) & ~OCCUPIED_BIT;
        bool occupied = (*reinterpret_cast<const size_t *>(current) & OCCUPIED_BIT) != 0;
        if (!occupied && current_size >= total_size) {
            if (mode == allocator_with_fit_mode::fit_mode::first_fit) {
                block_ptr = current;
                previous_ptr = previous;
                block_size = current_size;
                break;
            }
            if (block_ptr == nullptr) {
                block_ptr = current;
                previous_ptr = previous;
                block_size = current_size;
            } else if (mode == allocator_with_fit_mode::fit_mode::the_best_fit) {
                if (current_size < block_size) {
                    block_ptr = current;
                    previous_ptr = previous;
                    block_size = current_size;
                }
            } else if (mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
                if (current_size > block_size) {
                    block_ptr = current;
                    previous_ptr = previous;
                    block_size = current_size;
                }
            }
        }
        previous = current;
        unsigned char *next = current + current_size;
        if (next <= current || next > end) {
            break;
        }
        current = next;
    }
    if (block_ptr == nullptr) {
        throw std::bad_alloc();
    }
    size_t remain = block_size - total_size;
    if (remain >= METADATA_SIZE) {
        unsigned char *remain_block = block_ptr + total_size;
        void *best_next = *reinterpret_cast<void *const *>(block_ptr + sizeof(size_t) + sizeof(void *));
        size_t size_with_bit = total_size | OCCUPIED_BIT;
        *reinterpret_cast<size_t *>(block_ptr) = size_with_bit;
        *reinterpret_cast<size_t *>(block_ptr + total_size - sizeof(size_t)) = size_with_bit;
        *reinterpret_cast<void **>(block_ptr + sizeof(size_t)) = nullptr;
        *reinterpret_cast<void **>(block_ptr + sizeof(size_t) + sizeof(void *)) = nullptr;
        *reinterpret_cast<void **>(block_ptr + sizeof(size_t) + sizeof(void *) * 2) = nullptr;
        *reinterpret_cast<size_t *>(remain_block) = remain;
        *reinterpret_cast<size_t *>(remain_block + remain - sizeof(size_t)) = remain;
        *reinterpret_cast<void **>(remain_block + sizeof(size_t)) = previous_ptr;
        *reinterpret_cast<void **>(remain_block + sizeof(size_t) + sizeof(void *)) = best_next;
        *reinterpret_cast<void **>(remain_block + sizeof(size_t) + sizeof(void *) * 2) = nullptr;
        if (previous_ptr == nullptr) {
            *reinterpret_cast<void **>(memory_ptr + head_offset) = remain_block;
        } else {
            *reinterpret_cast<void **>(previous_ptr + sizeof(size_t) + sizeof(void *)) = remain_block;
        }
        if (best_next != nullptr) {
            *reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(best_next) + sizeof(size_t)) = remain_block;
        }
    } else {
        size_t size_with_bit = block_size | OCCUPIED_BIT;
        *reinterpret_cast<size_t *>(block_ptr) = size_with_bit;
        *reinterpret_cast<size_t *>(block_ptr + block_size - sizeof(size_t)) = size_with_bit;
        void *next = *reinterpret_cast<void *const *>(block_ptr + sizeof(size_t) + sizeof(void *));
        if (previous_ptr == nullptr) {
            *reinterpret_cast<void **>(memory_ptr + head_offset) = next;
        } else {
            *reinterpret_cast<void **>(previous_ptr + sizeof(size_t) + sizeof(void *)) = next;
        }
        if (next != nullptr) {
            *reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(next) + sizeof(size_t)) = previous_ptr;
        }
        *reinterpret_cast<void **>(block_ptr + sizeof(size_t)) = nullptr;
        *reinterpret_cast<void **>(block_ptr + sizeof(size_t) + sizeof(void *)) = nullptr;
        *reinterpret_cast<void **>(block_ptr + sizeof(size_t) + sizeof(void *) * 2) = nullptr;
    }
    return block_ptr + METADATA_SIZE;
}

void allocator_boundary_tags::do_deallocate_sm(
    void *at)
{
    if (_trusted_memory == nullptr) {
        throw std::runtime_error("Пустой аллокатор");
    }
    if (at == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *begin = memory_ptr + data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *ptr_at = reinterpret_cast<unsigned char *>(at);
    if (ptr_at < begin + METADATA_SIZE || ptr_at >= end) {
        throw std::invalid_argument("Указатель вне аллокатора");
    }
    unsigned char *metadata_ptr = nullptr;
    for (unsigned char *ptr_current = begin; ptr_current < end;) {
        if (ptr_current + METADATA_SIZE == ptr_at) {
            metadata_ptr = ptr_current;
            break;
        }
        size_t current_size = *reinterpret_cast<const size_t *>(ptr_current) & ~OCCUPIED_BIT;
        unsigned char *next = ptr_current + current_size;
        if (next <= ptr_current || next > end) {
            break;
        }
        ptr_current = next;
    }
    if (metadata_ptr == nullptr) {
        throw std::invalid_argument("Указатель вне аллокатора");
    }
    if ((*reinterpret_cast<const size_t *>(metadata_ptr) & OCCUPIED_BIT) == 0) {
        throw std::invalid_argument("Память уже освобождена");
    }
    size_t size = *reinterpret_cast<size_t *>(metadata_ptr) & ~OCCUPIED_BIT;
    *reinterpret_cast<size_t *>(metadata_ptr) = size;
    *reinterpret_cast<size_t *>(metadata_ptr + size - sizeof(size_t)) = size;
    merge_blocks(_trusted_memory, metadata_ptr);
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    if (_trusted_memory == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + fit_offset) = mode;
}


std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    try {
        return get_blocks_info_inner();
    } catch (std::bad_alloc) {
        return {};
    }
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    return boundary_iterator(_trusted_memory);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    return boundary_iterator();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> blocks_info;
    if (_trusted_memory == nullptr) {
        return blocks_info;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *begin = memory_ptr + data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *ptr_current = begin;
    while (ptr_current < end) {
        allocator_test_utils::block_info block;
        block.block_size = *reinterpret_cast<const size_t *>(ptr_current) & ~OCCUPIED_BIT;
        block.is_block_occupied = (*reinterpret_cast<const size_t *>(ptr_current) & OCCUPIED_BIT) != 0;
        blocks_info.push_back(block);
        unsigned char *next = ptr_current + (*reinterpret_cast<const size_t *>(ptr_current) & ~OCCUPIED_BIT);
        if (next <= ptr_current || next > end) {
            break;
        }
        ptr_current = next;
    }
    return blocks_info;
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other)
{
    _trusted_memory = copy_allocator_from(other._trusted_memory);
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other)
{
    if (this == &other) {
        return *this;
    }
    if (other._trusted_memory == nullptr) {
        delete_allocator(_trusted_memory);
        return *this;
    }
    void *new_memory = copy_allocator_from(other._trusted_memory);
    if (_trusted_memory == nullptr) {
        _trusted_memory = new_memory;
        return *this;
    }
    void *old_memory = _trusted_memory;
    _trusted_memory = new_memory;
    delete_allocator(old_memory);
    return *this;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

bool allocator_boundary_tags::boundary_iterator::operator==(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
        const allocator_boundary_tags::boundary_iterator & other) const noexcept
{
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (_trusted_memory == nullptr || _occupied_ptr == nullptr) {
        _occupied_ptr = nullptr;
        _occupied = false;
        return *this;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    unsigned char *current = reinterpret_cast<unsigned char *>(_occupied_ptr);
    unsigned char *next = current + (*reinterpret_cast<const size_t *>(current) & ~OCCUPIED_BIT);
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *end = memory_ptr + data_offset + space_size;
    if (next >= end || next < current) {
        _occupied_ptr = nullptr;
        _occupied = false;
    } else {
        _occupied_ptr = next;
        _occupied = (*reinterpret_cast<const size_t *>(next) & OCCUPIED_BIT) != 0;
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (_trusted_memory == nullptr) {
        _occupied_ptr = nullptr;
        _occupied = false;
        return *this;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *begin = memory_ptr + data_offset;
    unsigned char *end = begin + space_size;
    if (_occupied_ptr == nullptr) {
        unsigned char *last = nullptr;
        unsigned char *current = begin;
        while (current < end) {
            last = current;
            unsigned char *next = current + (*reinterpret_cast<const size_t *>(current) & ~OCCUPIED_BIT);
            if (next <= current || next > end) {
                break;
            }
            current = next;
        }
        _occupied_ptr = last;
        if (_occupied_ptr == nullptr) {
            _occupied = false;
        } else {
            _occupied = (*reinterpret_cast<const size_t *>(_occupied_ptr) & OCCUPIED_BIT) != 0;
        }
        return *this;
    }
    unsigned char *current_block = reinterpret_cast<unsigned char *>(_occupied_ptr);
    unsigned char *previous = nullptr;
    unsigned char *current_ptr = begin;
    while (current_ptr < end) {
        unsigned char *next = current_ptr + (*reinterpret_cast<const size_t *>(current_ptr) & ~OCCUPIED_BIT);
        if (next <= current_ptr || next > end) {
            break;
        }
        if (next == current_block) {
            previous = current_ptr;
            break;
        }
        current_ptr = next;
    }
    if (previous == nullptr) {
        _occupied_ptr = nullptr;
        _occupied = false;
    } else {
        _occupied_ptr = previous;
        _occupied = (*reinterpret_cast<const size_t *>(previous) & OCCUPIED_BIT) != 0;
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)
{
    boundary_iterator previous = *this;
    ++(*this);
    return previous;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)
{
    boundary_iterator previous = *this;
    --(*this);
    return previous;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    if (_occupied_ptr == nullptr) {
        return 0;
    }
    return *reinterpret_cast<const size_t *>(_occupied_ptr) & ~OCCUPIED_BIT;
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    return _occupied;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    if (_occupied_ptr == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<unsigned char *>(_occupied_ptr) + METADATA_SIZE;
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()
{
}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted)
{
    if (trusted == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(trusted);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    _occupied_ptr = *reinterpret_cast<void **>(memory_ptr + head_offset);
    if (_occupied_ptr != nullptr) {
        _occupied = (*reinterpret_cast<const size_t *>(_occupied_ptr) & OCCUPIED_BIT) != 0;
    }
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    return _occupied_ptr;
}
