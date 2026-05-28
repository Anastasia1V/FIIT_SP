#include <not_implemented.h>
#include "../include/allocator_red_black_tree.h"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory_resource>
#include <mutex>
#include <new>
#include <set>
#include <stdexcept>
#include <vector>

namespace
{
struct memory_block
{
    size_t offset;
    size_t size;
    bool occupied;
    memory_block *previous;
    memory_block *next;
};

struct free_block_compare
{
    bool operator()(const memory_block *left, const memory_block *right) const noexcept
    {
        if (left->size < right->size) {
            return true;
        }
        if (left->size > right->size) {
            return false;
        }
        return left->offset < right->offset;
    }
};

struct free_block_size_key
{
    size_t size;
    size_t offset;
};

struct free_block_compare_with_key
{
    using is_transparent = std::true_type;

    bool operator()(const memory_block *left, const memory_block *right) const noexcept
    {
        if (left->size < right->size) {
            return true;
        }
        if (left->size > right->size) {
            return false;
        }
        return left->offset < right->offset;
    }

    bool operator()(const memory_block *left, const free_block_size_key &right) const noexcept
    {
        if (left->size < right.size) {
            return true;
        }
        if (left->size > right.size) {
            return false;
        }
        return left->offset < right.offset;
    }

    bool operator()(const free_block_size_key &left, const memory_block *right) const noexcept
    {
        if (left.size < right->size) {
            return true;
        }

        if (left.size > right->size) {
            return false;
        }

        return left.offset < right->offset;
    }
};

struct allocator_data
{
    std::pmr::memory_resource *parent_allocator;
    allocator_with_fit_mode::fit_mode fit_mode;
    size_t space_size;
    unsigned char *memory;
    memory_block *head;
    memory_block *tail;
    std::set<memory_block *, free_block_compare_with_key> free_blocks;
    mutable std::mutex mutex;
};

constexpr size_t minimal_split_size = 128;

constexpr size_t round_up(size_t value, size_t divisor)
{
    if (divisor == 0) {
        return value;
    }
    if (value > std::numeric_limits<size_t>::max() - (divisor - 1)) {
        throw std::bad_alloc();
    }
    return (value + divisor - 1) / divisor * divisor;
}

static size_t aligned_block_size(size_t size)
{
    return round_up(size, alignof(std::max_align_t));
}

static void *allocate_raw_memory(
    size_t size,
    std::pmr::memory_resource *parent_allocator)
{
    if (parent_allocator != nullptr) {
        return parent_allocator->allocate(size, alignof(std::max_align_t));
    }
    return ::operator new(size, std::align_val_t(alignof(std::max_align_t)));
}

static void deallocate_raw_memory(
    void *memory,
    size_t size,
    std::pmr::memory_resource *parent_allocator)
{
    if (memory == nullptr) {
        return;
    }
    if (parent_allocator != nullptr) {
        parent_allocator->deallocate(memory, size, alignof(std::max_align_t));
    } else {
        ::operator delete(memory, std::align_val_t(alignof(std::max_align_t)));
    }
}

static allocator_data *get_allocator_data(void *trusted_memory)
{
    return reinterpret_cast<allocator_data *>(trusted_memory);
}

static const allocator_data *get_allocator_data(const void *trusted_memory)
{
    return reinterpret_cast<const allocator_data *>(trusted_memory);
}

static memory_block *create_block(
    size_t offset,
    size_t size,
    bool occupied)
{
    memory_block *block = new memory_block;
    block->offset = offset;
    block->size = size;
    block->occupied = occupied;
    block->previous = nullptr;
    block->next = nullptr;
    return block;
}

static void destroy_blocks(allocator_data *data)
{
    memory_block *current = data->head;
    while (current != nullptr) {
        memory_block *next = current->next;
        delete current;
        current = next;
    }
    data->head = nullptr;
    data->tail = nullptr;
    data->free_blocks.clear();
}

static allocator_data *create_allocator_data(
    size_t space_size,
    std::pmr::memory_resource *parent_allocator,
    allocator_with_fit_mode::fit_mode fit_mode)
{
    if (space_size == 0) {
        throw std::invalid_argument("allocator size == 0");
    }
    allocator_data *data = new allocator_data;
    data->parent_allocator = parent_allocator;
    data->fit_mode = fit_mode;
    data->space_size = space_size;
    data->memory = nullptr;
    data->head = nullptr;
    data->tail = nullptr;
    try {
        data->memory = reinterpret_cast<unsigned char *>(
            allocate_raw_memory(space_size, parent_allocator));
        data->head = create_block(0, space_size, false);
        data->tail = data->head;
        data->free_blocks.insert(data->head);
        return data;
    } catch (const std::exception& exception) {
        if (data->memory != nullptr) {
            deallocate_raw_memory(
                data->memory,
                space_size,
                parent_allocator);
            data->memory = nullptr;
        }
        delete data;
        throw exception;
    }
}

static allocator_data *copy_allocator_data(const allocator_data *other)
{
    if (other == nullptr) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(other->mutex);
    allocator_data *data = new allocator_data;
    data->parent_allocator = other->parent_allocator;
    data->fit_mode = other->fit_mode;
    data->space_size = other->space_size;
    data->memory = nullptr;
    data->head = nullptr;
    data->tail = nullptr;
    try {
        data->memory = reinterpret_cast<unsigned char *>(
            allocate_raw_memory(
                data->space_size,
                data->parent_allocator));
        std::memcpy(
            data->memory,
            other->memory,
            data->space_size);
        memory_block *source = other->head;
        memory_block *prev = nullptr;
        while (source != nullptr) {
            memory_block *copy = create_block(
                source->offset,
                source->size,
                source->occupied);
            copy->previous = prev;
            if (prev != nullptr) {
                prev->next = copy;
            } else {
                data->head = copy;
            }
            if (!copy->occupied) {
                data->free_blocks.insert(copy);
            }
            prev = copy;
            source = source->next;
        }
        data->tail = prev;
        return data;
    } catch (const std::exception& exception) {
        destroy_blocks(data);
        if (data->memory != nullptr) {
            deallocate_raw_memory(
                data->memory,
                data->space_size,
                data->parent_allocator);
            data->memory = nullptr;
        }
        delete data;
        throw exception;
    }
}

static void destroy_allocator_data(allocator_data *data)
{
    if (data == nullptr) {
        return;
    }
    destroy_blocks(data);
    if (data->memory != nullptr) {
        deallocate_raw_memory(
            data->memory,
            data->space_size,
            data->parent_allocator);
        data->memory = nullptr;
    }
    delete data;
}

static void remove_free_block(
    allocator_data *data,
    memory_block *block)
{
    if (block == nullptr) {
        return;
    }
    if (block->occupied) {
        return;
    }
    std::set<memory_block *, free_block_compare_with_key>::iterator iterator = 
        data->free_blocks.find(block);
    if (iterator != data->free_blocks.end()) {
        data->free_blocks.erase(iterator);
    }
}

static void insert_free_block(
    allocator_data *data,
    memory_block *block)
{
    if (block == nullptr) {
        return;
    }
    if (block->occupied) {
        return;
    }
    data->free_blocks.insert(block);
}

static memory_block *find_first_fit(
    allocator_data *data,
    size_t required_size)
{
    memory_block *current = data->head;
    while (current != nullptr) {
        if (!current->occupied && current->size >= required_size) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

static memory_block *find_best_fit(
    allocator_data *data,
    size_t required_size)
{
    free_block_size_key key;
    key.size = required_size;
    key.offset = 0;
    std::set<memory_block *, free_block_compare_with_key>::iterator iterator = 
        data->free_blocks.lower_bound(key);
    if (iterator == data->free_blocks.end()) {
        return nullptr;
    }
    return *iterator;
}

static memory_block *find_worst_fit(
    allocator_data *data,
    size_t required_size)
{
    if (data->free_blocks.empty()) {
        return nullptr;
    }
    std::set<memory_block *, free_block_compare_with_key>::iterator iterator = data->free_blocks.end();
    --iterator;
    memory_block *largest = *iterator;
    if (largest->size >= required_size) {
        return largest;
    }
    return nullptr;
}

static memory_block *find_block_for_allocate(
    allocator_data *data,
    size_t required_size)
{
    if (data->fit_mode == allocator_with_fit_mode::fit_mode::first_fit) {
        return find_first_fit(data, required_size);
    }
    if (data->fit_mode == allocator_with_fit_mode::fit_mode::the_best_fit) {
        return find_best_fit(data, required_size);
    }
    if (data->fit_mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
        return find_worst_fit(data, required_size);
    }
    return nullptr;
}

static void split_block(
    allocator_data *data,
    memory_block *block,
    size_t required_size)
{
    if (block == nullptr) {
        return;
    }
    if (block->size <= required_size) {
        block->occupied = true;
        return;
    }
    size_t remain = block->size - required_size;
    if (remain < minimal_split_size) {
        block->occupied = true;
        return;
    }
    memory_block *new_block = create_block(
        block->offset + required_size,
        remain,
        false);
    new_block->previous = block;
    new_block->next = block->next;
    if (block->next != nullptr) {
        block->next->previous = new_block;
    } else {
        data->tail = new_block;
    }
    block->next = new_block;
    block->size = required_size;
    block->occupied = true;
    insert_free_block(data, new_block);
}

static void merge_with_next_block(
    allocator_data *data,
    memory_block *block)
{
    if (block == nullptr) {
        return;
    }
    if (block->next == nullptr) {
        return;
    }
    memory_block *next = block->next;
    if (next->occupied) {
        return;
    }
    if (block->offset + block->size != next->offset) {
        return;
    }
    remove_free_block(data, next);
    block->size += next->size;
    block->next = next->next;
    if (next->next != nullptr) {
        next->next->previous = block;
    } else {
        data->tail = block;
    }
    delete next;
}

static memory_block *merge_free_blocks(
    allocator_data *data,
    memory_block *block)
{
    if (block == nullptr) {
        return nullptr;
    }
    while (block->previous != nullptr) {
        if (block->previous->occupied) {
            break;
        }
        if (block->previous->offset + block->previous->size != block->offset) {
            break;
        }
        memory_block *left = block->previous;
        remove_free_block(data, left);
        left->size += block->size;
        left->next = block->next;
        if (block->next != nullptr) {
            block->next->previous = left;
        } else {
            data->tail = left;
        }
        delete block;
        block = left;
    }

    while (block->next != nullptr) {
        if (block->next->occupied) {
            break;
        }
        if (block->offset + block->size != block->next->offset) {
            break;
        }
        merge_with_next_block(data, block);
    }
    return block;
}

static memory_block *find_block_by_pointer(
    allocator_data *data,
    unsigned char *pointer)
{
    if (data == nullptr) {
        return nullptr;
    }
    if (pointer == nullptr) {
        return nullptr;
    }
    if (pointer < data->memory ||
        pointer >= data->memory + data->space_size) {
        return nullptr;
    }
    size_t offset = static_cast<size_t>(pointer - data->memory);
    memory_block *current = data->head;
    while (current != nullptr) {
        if (current->offset == offset) {
            return current;
        }
        if (current->offset > offset) {
            break;
        }
        current = current->next;
    }
    return nullptr;
}
}

allocator_red_black_tree::~allocator_red_black_tree()
{
    destroy_allocator_data(get_allocator_data(_trusted_memory));
    _trusted_memory = nullptr;
}

allocator_red_black_tree::allocator_red_black_tree(
    allocator_red_black_tree &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(
    allocator_red_black_tree &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    destroy_allocator_data(get_allocator_data(_trusted_memory));
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    _trusted_memory = create_allocator_data(space_size, parent_allocator, allocate_fit_mode);
}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
{
    _trusted_memory =
        copy_allocator_data(get_allocator_data(other._trusted_memory));
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other)
{
    if (this == &other) {
        return *this;
    }
    destroy_allocator_data(get_allocator_data(_trusted_memory));
    _trusted_memory = copy_allocator_data(get_allocator_data(other._trusted_memory));
    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size)
{
    allocator_data *data =
        get_allocator_data(_trusted_memory);

    if (data == nullptr) {
        throw std::runtime_error("allocator is empty");
    }
    if (size == 0) {
        size = 1;
    }
    size_t required_size = aligned_block_size(size);
    if (required_size < size) {
        throw std::bad_alloc();
    }
    std::lock_guard<std::mutex> lock(data->mutex);
    memory_block *block = find_block_for_allocate(data, required_size);
    if (block == nullptr) {
        throw std::bad_alloc();
    }
    remove_free_block(data, block);
    split_block(data, block, required_size);
    return data->memory + block->offset;
}

void allocator_red_black_tree::do_deallocate_sm(
    void *at)
{
    allocator_data *data = get_allocator_data(_trusted_memory);
    if (data == nullptr) {
        throw std::runtime_error("allocator is empty");
    }
    if (at == nullptr) {
        return;
    }
    unsigned char *pointer = reinterpret_cast<unsigned char *>(at);
    std::lock_guard<std::mutex> lock(data->mutex);
    if (pointer < data->memory || pointer >= data->memory + data->space_size) {
        throw std::invalid_argument("pointer outside allocator");
    }
    memory_block *block = find_block_by_pointer(data, pointer);
    if (block == nullptr) {
        throw std::invalid_argument("pointer outside allocator");
    }
    if (!block->occupied) {
        throw std::invalid_argument("already free");
    }
    block->occupied = false;
    block = merge_free_blocks(data, block);
    insert_free_block(data, block);
}

void allocator_red_black_tree::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    allocator_data *data = get_allocator_data(_trusted_memory);
    if (data == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(data->mutex);
    data->fit_mode = mode;
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> blocks_info;
    const allocator_data *data = get_allocator_data(_trusted_memory);
    if (data == nullptr) {
        return blocks_info;
    }
    std::lock_guard<std::mutex> lock(data->mutex);
    memory_block *current = data->head;
    while (current != nullptr) {
        allocator_test_utils::block_info info;
        info.block_size = current->size;
        info.is_block_occupied = current->occupied;
        blocks_info.push_back(info);
        current = current->next;
    }
    return blocks_info;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    allocator_data *data = get_allocator_data(_trusted_memory);
    if (data == nullptr) {
        return rb_iterator();
    }
    return rb_iterator(data);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    return rb_iterator();
}

bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr == other._block_ptr && _trusted == other._trusted;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    if (_block_ptr == nullptr) {
        return *this;
    }
    memory_block *block = reinterpret_cast<memory_block *>(_block_ptr);
    _block_ptr = block->next;
    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int n)
{
    (void)n;
    rb_iterator prev = *this;
    ++(*this);
    return prev;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    if (_block_ptr == nullptr) {
        return 0;
    }
    memory_block *block = reinterpret_cast<memory_block *>(_block_ptr);
    return block->size;
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    if (_block_ptr == nullptr) {
        return nullptr;
    }
    if (_trusted == nullptr) {
        return nullptr;
    }
    allocator_data *data = get_allocator_data(_trusted);
    if (data == nullptr) {
        return nullptr;
    }
    memory_block *block = reinterpret_cast<memory_block *>(_block_ptr);
    return data->memory + block->offset;
}

allocator_red_black_tree::rb_iterator::rb_iterator()
{
    _block_ptr = nullptr;
    _trusted = nullptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
{
    _trusted = trusted;
    allocator_data *data = get_allocator_data(trusted);
    if (data == nullptr) {
        _block_ptr = nullptr;
        return;
    }
    std::lock_guard<std::mutex> lock(data->mutex);
    _block_ptr = data->head;
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    if (_block_ptr == nullptr) {
        return false;
    }
    memory_block *block = reinterpret_cast<memory_block *>(_block_ptr);
    return block->occupied;
}
