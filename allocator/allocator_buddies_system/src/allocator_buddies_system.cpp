#include <not_implemented.h>
#include <cstddef>
#include "../include/allocator_buddies_system.h"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <cstring>
#include <memory_resource>
#include <mutex>
#include <vector>

namespace
{
constexpr size_t round_up(size_t value, size_t divisor)
{
    return (value + divisor - 1) / divisor * divisor;
}

constexpr size_t metadata_size = sizeof(unsigned char);
constexpr size_t occupied_block_header_size = metadata_size;
constexpr size_t free_block_header_size = metadata_size + sizeof(void *);

static bool is_power_of_two(size_t value)
{
    if (value == 0) {
        return false;
    }
    return (value & (value - 1)) == 0;
}

static size_t next_power_of_two(size_t value)
{
    size_t power = 1;
    while (power < value) {
        if (power > std::numeric_limits<size_t>::max() / 2) {
            throw std::bad_alloc();
        }
        power <<= 1;
    }
    return power;
}

static unsigned char size_to_order(size_t value)
{
    unsigned char order = 0;
    size_t current = 1;
    while (current < value) {
        if (current > std::numeric_limits<size_t>::max() / 2) {
            throw std::bad_alloc();
        }
        current <<= 1;
        ++order;
    }
    return order;
}

struct layout_base
{
    size_t parent_offset;
    size_t fit_offset;
    size_t order_offset;
    size_t mutex_offset;
    size_t meta_size;
};

struct layout_full : layout_base
{
    size_t lists_offset;
    size_t data_offset;
    size_t total_size;
    unsigned char max_order;
};

static layout_base make_base_layout()
{
    layout_base layout{};
    size_t current_offset = 0;
    layout.parent_offset = current_offset;
    current_offset += sizeof(std::pmr::memory_resource *);
    current_offset = round_up(current_offset, alignof(allocator_with_fit_mode::fit_mode));
    layout.fit_offset = current_offset;
    current_offset += sizeof(allocator_with_fit_mode::fit_mode);
    current_offset = round_up(current_offset, alignof(unsigned char));
    layout.order_offset = current_offset;
    current_offset += sizeof(unsigned char);
    current_offset = round_up(current_offset, alignof(std::mutex));
    layout.mutex_offset = current_offset;
    current_offset += sizeof(std::mutex);
    layout.meta_size = current_offset;
    return layout;
}

static layout_full make_layout(size_t space_size)
{
    layout_base base_layout = make_base_layout();
    layout_full layout{};
    layout.parent_offset = base_layout.parent_offset;
    layout.fit_offset = base_layout.fit_offset;
    layout.order_offset = base_layout.order_offset;
    layout.mutex_offset = base_layout.mutex_offset;
    layout.meta_size = base_layout.meta_size;
    layout.max_order = size_to_order(space_size);
    layout.lists_offset = round_up(base_layout.meta_size, alignof(void *));
    size_t lists_size = (size_t(layout.max_order) + 1) * sizeof(void *);
    if (layout.lists_offset > std::numeric_limits<size_t>::max() - lists_size) {
        throw std::bad_alloc();
    }
    size_t after_lists = layout.lists_offset + lists_size;
    layout.data_offset = round_up(after_lists, alignof(std::max_align_t));
    if (space_size > std::numeric_limits<size_t>::max() - layout.data_offset) {
        throw std::bad_alloc();
    }
    layout.total_size = layout.data_offset + space_size;
    return layout;
}

static size_t space_size_from_order(unsigned char order)
{
    if (order >= sizeof(size_t) * 8 - 1) {
        throw std::bad_alloc();
    }
    return size_t(1) << order;
}

static size_t read_space_size(const void *memory)
{
    const unsigned char *memory_ptr = reinterpret_cast<const unsigned char *>(memory);
    layout_base base_layout = make_base_layout();
    unsigned char order = *reinterpret_cast<const unsigned char *>(memory_ptr + base_layout.order_offset);
    return space_size_from_order(order);
}

static void *read_pointer(const unsigned char *ptr)
{
    void *value = nullptr;
    std::memcpy(&value, ptr, sizeof(void *));
    return value;
}

static void write_pointer(unsigned char *ptr, void *value)
{
    std::memcpy(ptr, &value, sizeof(void *));
}

static unsigned char block_raw_meta(const unsigned char *block)
{
    return *block;
}

static bool block_occupied(const unsigned char *block)
{
    return (block_raw_meta(block) & 1u) != 0;
}

static unsigned char block_order(const unsigned char *block)
{
    return static_cast<unsigned char>(block_raw_meta(block) >> 1);
}

static size_t block_size(const unsigned char *block)
{
    unsigned char order = block_order(block);
    if (order >= sizeof(size_t) * 8 - 1) {
        return 0;
    }
    return size_t(1) << order;
}

static void set_block_state(unsigned char *block, bool occupied, unsigned char order)
{
    unsigned char value = static_cast<unsigned char>(order << 1);
    if (occupied) {
        value |= 1u;
    }
    *block = value;
}

static void *free_next(const unsigned char *block)
{
    return read_pointer(block + metadata_size);
}

static void set_free_next(unsigned char *block, void *next)
{
    write_pointer(block + metadata_size, next);
}

static void push_free_block(void **lists, unsigned char *block, unsigned char order)
{
    set_block_state(block, false, order);
    set_free_next(block, lists[order]);
    lists[order] = block;
}

static unsigned char *pop_free_block(void **lists, unsigned char order)
{
    unsigned char *block = reinterpret_cast<unsigned char *>(lists[order]);
    if (block == nullptr) {
        return nullptr;
    }
    lists[order] = free_next(block);
    set_free_next(block, nullptr);
    return block;
}

static bool remove_from_free_list(void **lists, unsigned char *block, unsigned char order)
{
    unsigned char *current = reinterpret_cast<unsigned char *>(lists[order]);
    unsigned char *previous = nullptr;
    while (current != nullptr) {
        if (current == block) {
            unsigned char *next = reinterpret_cast<unsigned char *>(free_next(current));
            if (previous == nullptr) {
                lists[order] = next;
            } else {
                set_free_next(previous, next);
            }
            set_free_next(current, nullptr);
            return true;
        }
        previous = current;
        current = reinterpret_cast<unsigned char *>(free_next(current));
    }
    return false;
}

static unsigned char *get_data_begin(void *memory)
{
    size_t space_size = read_space_size(memory);
    layout_full layout = make_layout(space_size);
    return reinterpret_cast<unsigned char *>(memory) + layout.data_offset;
}

static const unsigned char *get_data_begin(const void *memory)
{
    size_t space_size = read_space_size(memory);
    layout_full layout = make_layout(space_size);
    return reinterpret_cast<const unsigned char *>(memory) + layout.data_offset;
}

static void **get_free_lists(void *memory)
{
    size_t space_size = read_space_size(memory);
    layout_full layout = make_layout(space_size);
    return reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(memory) + layout.lists_offset);
}

static void *copy_allocator_from(const void *other)
{
    if (other == nullptr) {
        return nullptr;
    }
    layout_base base_layout = make_base_layout();
    const unsigned char *source = reinterpret_cast<const unsigned char *>(other);
    std::lock_guard<std::mutex> lock(
        *reinterpret_cast<std::mutex *>(const_cast<unsigned char *>(source) + base_layout.mutex_offset));
    size_t space_size = read_space_size(other);
    layout_full layout = make_layout(space_size);
    std::pmr::memory_resource *parent_allocator =
        *reinterpret_cast<std::pmr::memory_resource *const *>(source + base_layout.parent_offset);
    void *destination = nullptr;
    if (parent_allocator != nullptr) {
        destination = parent_allocator->allocate(layout.total_size, alignof(std::max_align_t));
    } else {
        destination = ::operator new(layout.total_size, std::align_val_t(alignof(std::max_align_t)));
    }
    unsigned char *destination_ptr = reinterpret_cast<unsigned char *>(destination);
    *reinterpret_cast<std::pmr::memory_resource **>(destination_ptr + base_layout.parent_offset) = parent_allocator;
    *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(destination_ptr + base_layout.fit_offset) =
        *reinterpret_cast<const allocator_with_fit_mode::fit_mode *>(source + base_layout.fit_offset);
    *reinterpret_cast<unsigned char *>(destination_ptr + base_layout.order_offset) =
        *reinterpret_cast<const unsigned char *>(source + base_layout.order_offset);
    new (destination_ptr + base_layout.mutex_offset) std::mutex();
    std::copy_n(source + layout.data_offset, space_size, destination_ptr + layout.data_offset);
    void **destination_lists = reinterpret_cast<void **>(destination_ptr + layout.lists_offset);
    for (size_t i = 0; i <= layout.max_order; ++i) {
        destination_lists[i] = nullptr;
    }
    unsigned char *begin = destination_ptr + layout.data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *current = begin;
    while (current < end) {
        size_t current_size = block_size(current);
        if (current_size == 0) {
            break;
        }
        if (!block_occupied(current)) {
            unsigned char order = block_order(current);
            push_free_block(destination_lists, current, order);
        }
        current += current_size;
    }
    return destination;
}

static void delete_allocator(void *&allocator)
{
    if (allocator == nullptr) {
        return;
    }
    layout_base base_layout = make_base_layout();
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(allocator);
    std::pmr::memory_resource *parent_allocator = nullptr;
    size_t space_size = 0;
    {
        std::lock_guard<std::mutex> lock(
            *reinterpret_cast<std::mutex *>(memory_ptr + base_layout.mutex_offset));
        parent_allocator =
            *reinterpret_cast<std::pmr::memory_resource **>(memory_ptr + base_layout.parent_offset);
        space_size = read_space_size(allocator);
    }
    reinterpret_cast<std::mutex *>(memory_ptr + base_layout.mutex_offset)->~mutex();
    layout_full layout = make_layout(space_size);
    if (parent_allocator != nullptr) {
        parent_allocator->deallocate(allocator, layout.total_size, alignof(std::max_align_t));
    } else {
        ::operator delete(allocator, std::align_val_t(alignof(std::max_align_t)));
    }
    allocator = nullptr;
}
}

allocator_buddies_system::~allocator_buddies_system()
{
    delete_allocator(_trusted_memory);
}

allocator_buddies_system::allocator_buddies_system(
    allocator_buddies_system &&other) noexcept
{
    _trusted_memory = nullptr;
    if (other._trusted_memory == nullptr) {
        return;
    }
    layout_base base_layout = make_base_layout();
    unsigned char *other_ptr = reinterpret_cast<unsigned char *>(other._trusted_memory);
    std::lock_guard<std::mutex> lock(
        *reinterpret_cast<std::mutex *>(other_ptr + base_layout.mutex_offset));
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_buddies_system &allocator_buddies_system::operator=(
    allocator_buddies_system &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (other._trusted_memory == nullptr) {
        delete_allocator(_trusted_memory);
        return *this;
    }
    if (_trusted_memory == nullptr) {
        layout_base base_layout = make_base_layout();
        unsigned char *other_ptr = reinterpret_cast<unsigned char *>(other._trusted_memory);
        std::lock_guard<std::mutex> lock(
            *reinterpret_cast<std::mutex *>(other_ptr + base_layout.mutex_offset));
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
        return *this;
    }
    layout_base base_layout = make_base_layout();
    void *old_memory = _trusted_memory;
    unsigned char *this_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    unsigned char *other_ptr = reinterpret_cast<unsigned char *>(other._trusted_memory);
    {
        std::mutex &this_mutex =
            *reinterpret_cast<std::mutex *>(this_ptr + base_layout.mutex_offset);
        std::mutex &other_mutex =
            *reinterpret_cast<std::mutex *>(other_ptr + base_layout.mutex_offset);
        std::scoped_lock lock(this_mutex, other_mutex);
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    delete_allocator(old_memory);
    return *this;
}

allocator_buddies_system::allocator_buddies_system(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    _trusted_memory = nullptr;
    if (space_size < (size_t(1) << min_k)) {
        throw std::logic_error("space size too small");
    }
    layout_full layout = make_layout(space_size);
    if (parent_allocator != nullptr) {
        _trusted_memory = parent_allocator->allocate(layout.total_size, alignof(std::max_align_t));
    } else {
        _trusted_memory = ::operator new(layout.total_size, std::align_val_t(alignof(std::max_align_t)));
    }
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    *reinterpret_cast<std::pmr::memory_resource **>(memory_ptr + layout.parent_offset) = parent_allocator;
    *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + layout.fit_offset) = allocate_fit_mode;
    *reinterpret_cast<unsigned char *>(memory_ptr + layout.order_offset) = layout.max_order;
    new (memory_ptr + layout.mutex_offset) std::mutex();
    void **lists = reinterpret_cast<void **>(memory_ptr + layout.lists_offset);
    for (size_t i = 0; i <= layout.max_order; ++i) {
        lists[i] = nullptr;
    }
    unsigned char *first_block = memory_ptr + layout.data_offset;
    set_block_state(first_block, false, layout.max_order);
    set_free_next(first_block, nullptr);
    lists[layout.max_order] = first_block;
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr) {
        throw std::runtime_error("allocator is empty");
    }
    if (size == 0) {
        size = 1;
    }
    if (size > std::numeric_limits<size_t>::max() - occupied_block_header_size) {
        throw std::bad_alloc();
    }
    size_t requested = size + occupied_block_header_size;
    if (requested < (size_t(1) << min_k)) {
        requested = (size_t(1) << min_k);
    }
    size_t target_size = next_power_of_two(requested);
    unsigned char target_order = size_to_order(target_size);
    layout_base base_layout = make_base_layout();
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(
        *reinterpret_cast<std::mutex *>(memory_ptr + base_layout.mutex_offset));
    allocator_with_fit_mode::fit_mode fit_mode =
        *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + base_layout.fit_offset);
    size_t space_size = read_space_size(_trusted_memory);
    layout_full layout = make_layout(space_size);
    void **lists = reinterpret_cast<void **>(memory_ptr + layout.lists_offset);
    unsigned char found_order = target_order;
    bool found_block = false;
    if (fit_mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
        for (int order = static_cast<int>(layout.max_order); order >= static_cast<int>(target_order); --order) {
            if (lists[order] != nullptr) {
                found_order = static_cast<unsigned char>(order);
                found_block = true;
                break;
            }
        }
    } else {
        for (unsigned char order = target_order; order <= layout.max_order; ++order) {
            if (lists[order] != nullptr) {
                found_order = order;
                found_block = true;
                break;
            }
        }
    }
    if (!found_block) {
        throw std::bad_alloc();
    }
    unsigned char *block = pop_free_block(lists, found_order);
    unsigned char current_order = found_order;
    while (current_order > target_order) {
        --current_order;
        size_t half_size = size_t(1) << current_order;
        unsigned char *buddy = block + half_size;
        push_free_block(lists, buddy, current_order);
        set_block_state(block, false, current_order);
    }
    set_block_state(block, true, target_order);
    return block + occupied_block_header_size;
}

void allocator_buddies_system::do_deallocate_sm(void *at)
{
    if (_trusted_memory == nullptr) {
        throw std::runtime_error("allocator is empty");
    }
    if (at == nullptr) {
        return;
    }
    layout_base base_layout = make_base_layout();
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(
        *reinterpret_cast<std::mutex *>(memory_ptr + base_layout.mutex_offset));
    size_t space_size = read_space_size(_trusted_memory);
    layout_full layout = make_layout(space_size);
    unsigned char *begin = memory_ptr + layout.data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *pointer = reinterpret_cast<unsigned char *>(at);
    if (pointer < begin + occupied_block_header_size || pointer >= end) {
        throw std::invalid_argument("pointer outside allocator");
    }
    unsigned char *block = pointer - occupied_block_header_size;
    if (block < begin || block >= end) {
        throw std::invalid_argument("pointer outside allocator");
    }
    if (!block_occupied(block)) {
        throw std::invalid_argument("memory already freed");
    }
    unsigned char order = block_order(block);
    set_block_state(block, false, order);
    set_free_next(block, nullptr);
    void **lists = reinterpret_cast<void **>(memory_ptr + layout.lists_offset);
    while (order < layout.max_order) {
        size_t block_size_value = size_t(1) << order;
        size_t block_offset = static_cast<size_t>(block - begin);
        size_t buddy_offset = block_offset ^ block_size_value;
        if (buddy_offset >= space_size) {
            break;
        }
        unsigned char *buddy = begin + buddy_offset;
        if (block_occupied(buddy)) {
            break;
        }
        if (block_order(buddy) != order) {
            break;
        }
        if (!remove_from_free_list(lists, buddy, order)) {
            break;
        }
        if (buddy < block) {
            block = buddy;
        }
        ++order;
        set_block_state(block, false, order);
        set_free_next(block, nullptr);
    }
    push_free_block(lists, block, order);
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other)
{
    _trusted_memory = copy_allocator_from(other._trusted_memory);
}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other)
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

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

inline void allocator_buddies_system::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    if (_trusted_memory == nullptr) {
        return;
    }
    layout_base base_layout = make_base_layout();
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(
        *reinterpret_cast<std::mutex *>(memory_ptr + base_layout.mutex_offset));
    *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + base_layout.fit_offset) = mode;
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> blocks_info;
    if (_trusted_memory == nullptr) {
        return blocks_info;
    }
    layout_base base_layout = make_base_layout();
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(
        *reinterpret_cast<std::mutex *>(memory_ptr + base_layout.mutex_offset));
    size_t space_size = read_space_size(_trusted_memory);
    layout_full layout = make_layout(space_size);
    unsigned char *begin = memory_ptr + layout.data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *current = begin;
    while (current < end) {
        size_t current_size = block_size(current);
        if (current_size == 0) {
            break;
        }
        if (current_size > static_cast<size_t>(end - current)) {
            break;
        }
        allocator_test_utils::block_info block_info;
        block_info.block_size = current_size;
        block_info.is_block_occupied = block_occupied(current);
        blocks_info.push_back(block_info);
        current += current_size;
    }
    return blocks_info;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    if (_trusted_memory == nullptr) {
        return buddy_iterator();
    }
    return buddy_iterator(get_data_begin(_trusted_memory));
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    return buddy_iterator();
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    if (_block == nullptr) {
        return *this;
    }
    unsigned char *current_block = reinterpret_cast<unsigned char *>(_block);
    size_t current_size = block_size(current_block);
    if (current_size == 0) {
        _block = nullptr;
        return *this;
    }
    unsigned char *next_block = current_block + current_size;
    if (block_size(next_block) == 0) {
        _block = nullptr;
    } else {
        _block = next_block;
    }
    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int n)
{
    (void)n;
    buddy_iterator previous = *this;
    ++(*this);
    return previous;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    if (_block == nullptr) {
        return 0;
    }
    return block_size(reinterpret_cast<const unsigned char *>(_block));
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    if (_block == nullptr) {
        return false;
    }
    return block_occupied(reinterpret_cast<const unsigned char *>(_block));
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    if (_block == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<unsigned char *>(_block) + occupied_block_header_size;
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start)
{
    _block = start;
}

allocator_buddies_system::buddy_iterator::buddy_iterator()
{
    _block = nullptr;
}
