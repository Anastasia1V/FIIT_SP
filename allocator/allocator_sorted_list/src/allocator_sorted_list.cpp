#include <not_implemented.h>
#include "../include/allocator_sorted_list.h"

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

static void copy_allocator(const unsigned char *from, unsigned char *to,
    size_t allocator_size, size_t mutex_offset, size_t head_offset)
{
    std::copy_n(from, mutex_offset, to);
    new (to + mutex_offset) std::mutex();
    std::copy_n(from + mutex_offset + sizeof(std::mutex),
        allocator_size - (mutex_offset + sizeof(std::mutex)), to + mutex_offset + sizeof(std::mutex));
    const unsigned char *ptr_from = reinterpret_cast<const unsigned char *>(
            *reinterpret_cast<void *const *>(from + head_offset));
    if (ptr_from == nullptr) {
        *reinterpret_cast<void **>(to + head_offset) = nullptr;
        return;
    }
    unsigned char *ptr_to = to + (ptr_from - from);
    *reinterpret_cast<void **>(to + head_offset) = ptr_to;
    while (ptr_from != nullptr) {
        unsigned char *current = to + (ptr_from - from);
        const void *ptr_next = *reinterpret_cast<void *const *>(ptr_from);
        if (ptr_next == nullptr) {
            *reinterpret_cast<void **>(current) = nullptr;
            break;
        }
        const unsigned char *next = reinterpret_cast<const unsigned char *>(ptr_next);
        *reinterpret_cast<void **>(current) = to + (next - from);
        ptr_from = next;
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
    size_t allocator_size = 0;
    {
        std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(allocator_ptr + mutex_offset));
        allocator_size = *reinterpret_cast<size_t *>(allocator_ptr + size_offset) + data_offset;
        parent = *reinterpret_cast<std::pmr::memory_resource **>(allocator_ptr + parent_offset);
    }
    reinterpret_cast<std::mutex *>(allocator_ptr + mutex_offset)->~mutex();
    if (parent != nullptr) {
        parent->deallocate(allocator, allocator_size, alignof(std::max_align_t));
    } else {
        ::operator delete(allocator);
    }
    allocator = nullptr;
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
    void *allocator_ptr = nullptr;
    std::pmr::memory_resource *parent =
        *reinterpret_cast<std::pmr::memory_resource **>(ptr_other + parent_offset);
    if (parent != nullptr) {
        allocator_ptr = parent->allocate(total_size, alignof(std::max_align_t));
    } else {
        allocator_ptr = ::operator new(total_size);
    }
    copy_allocator(ptr_other, reinterpret_cast<unsigned char *>(allocator_ptr),
        total_size, mutex_offset, head_offset);
    return allocator_ptr;
}

allocator_sorted_list::~allocator_sorted_list()
{
    delete_allocator(_trusted_memory);
}

allocator_sorted_list::allocator_sorted_list(
    allocator_sorted_list &&other) noexcept
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

allocator_sorted_list &allocator_sorted_list::operator=(
    allocator_sorted_list &&other) noexcept
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
    void* old_this = _trusted_memory;
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

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size <= block_metadata_size) {
        throw std::invalid_argument("Запрашиваемый размер должен быть больше размера блока метаданных");
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    if (space_size > std::numeric_limits<size_t>::max() - data_offset) {
        throw std::bad_alloc();
    }
    size_t total_size = data_offset + space_size;
    if (parent_allocator != nullptr) {
        _trusted_memory = parent_allocator->allocate(total_size, alignof(std::max_align_t));
    } else {
        _trusted_memory = ::operator new(total_size);
    }
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    *reinterpret_cast<std::pmr::memory_resource **>(memory_ptr + parent_offset) = parent_allocator;
    *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + fit_offset) = allocate_fit_mode;
    *reinterpret_cast<size_t *>(memory_ptr + size_offset) = space_size;
    new (memory_ptr + mutex_offset) std::mutex();
    unsigned char *first = memory_ptr + data_offset;
    *reinterpret_cast<void **>(memory_ptr + head_offset) = first;
    *reinterpret_cast<void **>(first) = nullptr;
    *reinterpret_cast<size_t *>(first + sizeof(void *)) = space_size - block_metadata_size;
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr) {
        throw std::runtime_error("Пустой аллокатор");
    }
    if (size == 0) {
        size = 1;
    } else if (size > std::numeric_limits<size_t>::max() - alignof(std::max_align_t)){
        throw std::bad_alloc();
    }
    size = round_up(size, alignof(std::max_align_t));
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    allocator_with_fit_mode::fit_mode mode =
        *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory_ptr + fit_offset);
    void *head = *reinterpret_cast<void **>(memory_ptr + head_offset);
    void *best = nullptr;
    void *prev_best = nullptr;
    size_t best_size = 0;
    void *current = head;
    void *previous = nullptr;
    while (current != nullptr) {
        unsigned char *ptr_current = reinterpret_cast<unsigned char *>(current);
        size_t current_size = *reinterpret_cast<size_t *>(ptr_current + sizeof(void *));
        if (current_size >= size) {
            if (mode == allocator_with_fit_mode::fit_mode::first_fit) {
                best = current;
                prev_best = previous;
                best_size = current_size;
                break;
            }
            if (best == nullptr) {
                best = current;
                prev_best = previous;
                best_size = current_size;
            } else {
                if (mode == allocator_with_fit_mode::fit_mode::the_best_fit) {
                    if (current_size < best_size) {
                        best = current;
                        prev_best = previous;
                        best_size = current_size;
                    }
                } else if (mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
                    if (current_size > best_size) {
                        best = current;
                        prev_best = previous;
                        best_size = current_size;
                    }
                }
                else {
                    throw std::invalid_argument("Некорректный fit_mode");
                }
            }
        }
        previous = current;
        current = *reinterpret_cast<void **>(current);
    }
    if (best == nullptr) {
        throw std::bad_alloc();
    }
    unsigned char *ptr_best = reinterpret_cast<unsigned char *>(best);
    size_t remain_best = best_size - size;
    if (remain_best > block_metadata_size) {
        unsigned char *ptr_new = ptr_best + block_metadata_size + size;
        *reinterpret_cast<void **>(ptr_new) = *reinterpret_cast<void **>(ptr_best);
        *reinterpret_cast<size_t *>(ptr_new + sizeof(void *)) = remain_best - block_metadata_size;
        *reinterpret_cast<size_t *>(ptr_best + sizeof(void *)) = size;
        if (prev_best == nullptr) {
            *reinterpret_cast<void **>(memory_ptr + head_offset) = ptr_new;
        } else {
            *reinterpret_cast<void **>(prev_best) = ptr_new;
        }
    } else {
        if (prev_best == nullptr) {
            *reinterpret_cast<void **>(memory_ptr + head_offset) = *reinterpret_cast<void **>(ptr_best);
        } else {
            *reinterpret_cast<void **>(prev_best) = *reinterpret_cast<void **>(ptr_best);
        }
    }
    return ptr_best + block_metadata_size;
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
{
    _trusted_memory = copy_allocator_from(other._trusted_memory);
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other)
{
    if (this == &other) {
        return *this;
    }
    if (other._trusted_memory == nullptr) {
        delete_allocator(_trusted_memory);
        return *this;
    }
    void* new_memory = copy_allocator_from(other._trusted_memory);
    if (_trusted_memory == nullptr) {
        _trusted_memory = new_memory;
        return *this;
    }
    void* old_memory = _trusted_memory;
    _trusted_memory = new_memory;
    delete_allocator(old_memory);
    return *this;
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

void allocator_sorted_list::do_deallocate_sm(
    void *at)
{
    if (_trusted_memory == nullptr) {
        throw std::runtime_error("Пустой аллокатор");
    }
    if (at == nullptr) {
        throw std::invalid_argument("Нулевой указатель");
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(memory_ptr + mutex_offset));
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *begin = memory_ptr + data_offset;
    unsigned char *end = begin + space_size;
    unsigned char *ptr_at = reinterpret_cast<unsigned char *>(at);
    if (ptr_at < begin + block_metadata_size || ptr_at >= end) {
        throw std::invalid_argument("Указатель вне аллокатора");
    }
    unsigned char *metadata_ptr = ptr_at - block_metadata_size;
    unsigned char *ptr_current = begin;
    bool find_metadata = false;
    while (ptr_current < end) {
        if (ptr_current == metadata_ptr) {
            find_metadata = true;
            break;
        }
        size_t current_size = *reinterpret_cast<size_t *>(ptr_current + sizeof(void *));
        unsigned char *next_block = ptr_current + block_metadata_size + current_size;
        if (next_block <= ptr_current || next_block > end) {
            break;
        }
        ptr_current = next_block;
    }
    if (!find_metadata) {
        throw std::invalid_argument("Указатель вне аллокатора");
    }
    unsigned char *previous = nullptr;
    unsigned char *current =
        reinterpret_cast<unsigned char *>(*reinterpret_cast<void **>(memory_ptr + head_offset));
    while (current != nullptr && current < metadata_ptr) {
        previous = current;
        current = reinterpret_cast<unsigned char *>(*reinterpret_cast<void **>(current));
    }
    if (current == metadata_ptr) {
        throw std::invalid_argument("Память уже освобождена");
    }
    *reinterpret_cast<void **>(metadata_ptr) = current;
    if (previous == nullptr) {
        *reinterpret_cast<void **>(memory_ptr + head_offset) = metadata_ptr;
    } else {
        *reinterpret_cast<void **>(previous) = metadata_ptr;
    }
    if (current != nullptr) {
        unsigned char *metadata_end =
            metadata_ptr + block_metadata_size + *reinterpret_cast<size_t *>(metadata_ptr + sizeof(void *));
        if (metadata_end == current) {
            *reinterpret_cast<size_t *>(metadata_ptr + sizeof(void *)) +=
                block_metadata_size + *reinterpret_cast<size_t *>(current + sizeof(void *));
            *reinterpret_cast<void **>(metadata_ptr) = *reinterpret_cast<void **>(current);
        }
    }
    if (previous != nullptr) {
        size_t prev_size = *reinterpret_cast<size_t *>(previous + sizeof(void *));
        unsigned char *prev_end = previous + block_metadata_size + prev_size;
        if (prev_end == metadata_ptr) {
            *reinterpret_cast<size_t *>(previous + sizeof(void *)) +=
                block_metadata_size + *reinterpret_cast<size_t *>(metadata_ptr + sizeof(void *));
            *reinterpret_cast<void **>(previous) = *reinterpret_cast<void **>(metadata_ptr);
        }
    }
}

inline void allocator_sorted_list::set_fit_mode(
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

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    try {
        return get_blocks_info_inner();
    } catch (std::bad_alloc) {
        return {};
    }
}


std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
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
    unsigned char *ptr_current =
        reinterpret_cast<unsigned char *>(*reinterpret_cast<void **>(memory_ptr + head_offset));
    unsigned char *current = begin;
    while (current < end) {
        bool not_free = true;
        if (ptr_current != nullptr && current == ptr_current) {
            not_free = false;
            ptr_current = reinterpret_cast<unsigned char *>(*reinterpret_cast<void **>(ptr_current));
        }
        size_t current_size = *reinterpret_cast<size_t *>(current + sizeof(void *));
        blocks_info.push_back({current_size, not_free});
        unsigned char *next = current + block_metadata_size + current_size;
        if (next <= current || next > end) {
            break;
        }
        current = next;
    }
    return blocks_info;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    return sorted_free_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator();
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator();
}


bool allocator_sorted_list::sorted_free_iterator::operator==(
        const allocator_sorted_list::sorted_free_iterator & other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr != nullptr) {
        _free_ptr = *reinterpret_cast<void **>(_free_ptr);
    }
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    sorted_free_iterator previous = *this;
    ++(*this);
    return previous;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    if (_free_ptr == nullptr) {
        return 0;
    }
    return *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(_free_ptr) + sizeof(void *));
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return _free_ptr;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator()
{
    _free_ptr = nullptr;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *trusted)
{
    if (trusted == nullptr) {
        _free_ptr = nullptr;
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *ptr_trusted = reinterpret_cast<unsigned char *>(trusted);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(ptr_trusted + mutex_offset));
    _free_ptr = *reinterpret_cast<void **>(ptr_trusted + head_offset);
}

bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator & other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr == nullptr) {
        return *this;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *memory_ptr = reinterpret_cast<unsigned char *>(_trusted_memory);
    size_t space_size = *reinterpret_cast<size_t *>(memory_ptr + size_offset);
    unsigned char *end = memory_ptr + data_offset + space_size;
    if (_current_ptr == _free_ptr && _free_ptr != nullptr) {
        _free_ptr = *reinterpret_cast<void **>(_free_ptr);
    }
    size_t current_size =
        *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(_current_ptr) + sizeof(void *));
    unsigned char *next = reinterpret_cast<unsigned char *>(_current_ptr) +
        allocator_sorted_list::block_metadata_size + current_size;
    if (next >= end) {
        _current_ptr = nullptr;
        return *this;
    }
    _current_ptr = next;
    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    sorted_iterator previous = *this;
    ++(*this);
    return previous;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    if (_current_ptr == nullptr) {
        return 0;
    }
    return *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(_current_ptr) + sizeof(void *));
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    if (_current_ptr == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<unsigned char *>(_current_ptr) + allocator_sorted_list::block_metadata_size;
}

allocator_sorted_list::sorted_iterator::sorted_iterator()
{
    _free_ptr = nullptr;
    _current_ptr = nullptr;
    _trusted_memory = nullptr;
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted)
{
    _free_ptr = nullptr;
    _current_ptr = nullptr;
    _trusted_memory = trusted;
    if (trusted == nullptr) {
        return;
    }
    size_t parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset;
    calculate_offsets(parent_offset, fit_offset, size_offset, mutex_offset, head_offset, data_offset);
    unsigned char *ptr_trusted = reinterpret_cast<unsigned char *>(trusted);
    std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex *>(ptr_trusted + mutex_offset));
    _free_ptr = *reinterpret_cast<void **>(ptr_trusted + head_offset);
    _current_ptr = ptr_trusted + data_offset;
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    if (_current_ptr == nullptr) {
        return false;
    }
    return _current_ptr != _free_ptr;
}
