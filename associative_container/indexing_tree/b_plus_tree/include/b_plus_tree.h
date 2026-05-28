#include <iterator>
#include <utility>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <concepts>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list>
#include <optional>

#ifndef SYS_PROG_B_PLUS_TREE_H
#define SYS_PROG_B_PLUS_TREE_H

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class BP_tree final : private compare //EBCO
{
public:

    using tree_data_type = std::pair<tkey, tvalue>;
    using tree_data_type_const = std::pair<const tkey, tvalue>;
    using value_type = tree_data_type_const;

private:

    static constexpr const size_t minimum_keys_in_node = t - 1;
    static constexpr const size_t maximum_keys_in_node = 2 * t - 1;

    // region comparators declaration

    inline bool compare_keys(const tkey& lhs, const tkey& rhs) const;
    inline bool compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const;
    inline bool equal_keys(const tkey& lhs, const tkey& rhs) const noexcept;

    // endregion comparators declaration

    struct bptree_node_base
    {
        bool _is_terminate;
        
        explicit bptree_node_base(bool is_terminate) noexcept;
        virtual ~bptree_node_base() = default;
    };

    struct bptree_node_term : public bptree_node_base
    {
        bptree_node_term* _next;
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _data;
        bptree_node_term() noexcept;
    };

    struct bptree_node_middle : public bptree_node_base
    {
        boost::container::static_vector<tkey, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<bptree_node_base*, maximum_keys_in_node + 2> _pointers;
        bptree_node_middle() noexcept;
    };

    pp_allocator<value_type> _allocator;
    bptree_node_base* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

    bptree_node_term* create_leaf_node();
    bptree_node_middle* create_middle_node();
    void destroy_node(bptree_node_base* node) noexcept;
    void clear_nodes() noexcept;
    bptree_node_term* find_left_leaf() const noexcept;
    bptree_node_term* find_leaf_by_key(const tkey& key) const noexcept;
    std::pair<bptree_node_term*, size_t> find_leaf_and_index(const tkey& key);
    std::pair<const bptree_node_term*, size_t> find_leaf_and_index(const tkey& key) const;
    size_t find_leaf_position(const bptree_node_term* leaf, const tkey& key) const noexcept;
    size_t find_child_index(const bptree_node_middle* node, const tkey& key) const noexcept;
    size_t node_size(const bptree_node_base* node) const noexcept;

    struct insert_result
    {
        bool inserted;
        bool split;
        tkey promote_key;
        bptree_node_base* right;
    };

    insert_result insert_into_node(bptree_node_base* node, value_type&& data);

    struct delete_result
    {
        bool deleted;
        bool underfull;
        bool first_key_changed;
        tkey first_key;
    };

    delete_result delete_from_node(bptree_node_base* node, const tkey& key);

    bool take_from_prev(bptree_node_middle* parent, size_t index);
    bool take_from_next(bptree_node_middle* parent, size_t index);
    void merge(bptree_node_middle* parent, size_t index);

public:

    // region constructors declaration

    explicit BP_tree(const compare& cmp = compare(), pp_allocator<value_type> alloc = pp_allocator<value_type>());
    explicit BP_tree(pp_allocator<value_type> alloc, const compare& cmp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit BP_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> alloc = pp_allocator<value_type>());

    BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> alloc = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    BP_tree(const BP_tree& other);
    BP_tree(BP_tree&& other) noexcept;
    BP_tree& operator=(const BP_tree& other);
    BP_tree& operator=(BP_tree&& other) noexcept;
    ~BP_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class bptree_iterator;
    class bptree_const_iterator;

    class bptree_iterator final
    {
        bptree_node_term* _node;
        size_t _index;
        mutable std::optional<value_type> _value;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type;
        using pointer = value_type*;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bptree_iterator;

        friend class BP_tree;
        friend class bptree_const_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t current_node_keys_count() const noexcept;
        size_t index() const noexcept;

        explicit bptree_iterator(bptree_node_term* node = nullptr, size_t index = 0);
    };

    class bptree_const_iterator final
    {
        const bptree_node_term* _node;
        size_t _index;
        mutable std::optional<value_type> _value;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type;
        using pointer = const value_type*;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bptree_const_iterator;

        friend class BP_tree;
        friend class bptree_iterator;

        bptree_const_iterator(const bptree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t current_node_keys_count() const noexcept;
        size_t index() const noexcept;

        explicit bptree_const_iterator(const bptree_node_term* node = nullptr, size_t index = 0);
    };

    // endregion iterators declaration

    // region element access declaration

    tvalue& at(const tkey& key);
    const tvalue& at(const tkey& key) const;

    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration

    // region iterator begins declaration

    bptree_iterator begin();
    bptree_iterator end();

    bptree_const_iterator begin() const;
    bptree_const_iterator end() const;

    bptree_const_iterator cbegin() const;
    bptree_const_iterator cend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    bptree_iterator find(const tkey& key);
    bptree_const_iterator find(const tkey& key) const;

    bptree_iterator lower_bound(const tkey& key);
    bptree_const_iterator lower_bound(const tkey& key) const;

    bptree_iterator upper_bound(const tkey& key);
    bptree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    std::pair<bptree_iterator, bool> insert(const tree_data_type& data);
    std::pair<bptree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<bptree_iterator, bool> emplace(Args&&... args);

    bptree_iterator insert_or_assign(const tree_data_type& data);
    bptree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    bptree_iterator emplace_or_assign(Args&&... args);

    bptree_iterator erase(bptree_iterator pos);
    bptree_iterator erase(bptree_const_iterator pos);

    bptree_iterator erase(bptree_iterator beg, bptree_iterator en);
    bptree_iterator erase(bptree_const_iterator beg, bptree_const_iterator en);

    bptree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
BP_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> BP_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> BP_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::compare_keys(const tkey& lhs, const tkey& rhs) const
{
    return compare::operator()(lhs, rhs);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::compare_pairs(const tree_data_type& lhs, const tree_data_type& rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::equal_keys(const tkey& lhs, const tkey& rhs) const noexcept
{
    return !compare_keys(lhs, rhs) && !compare_keys(rhs, lhs);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_base::bptree_node_base(bool is_terminate) noexcept
    : _is_terminate(is_terminate)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_term::bptree_node_term() noexcept
    : bptree_node_base(true), _next(nullptr), _data()
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_node_middle::bptree_node_middle() noexcept
    : bptree_node_base(false), _keys(), _pointers()
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename BP_tree<tkey, tvalue, compare, t>::value_type>
BP_tree<tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term*
BP_tree<tkey, tvalue, compare, t>::create_leaf_node()
{
    bptree_node_term* node = _allocator.template allocate_object<bptree_node_term>();
    _allocator.template construct<bptree_node_term>(node);
    return node;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_node_middle*
BP_tree<tkey, tvalue, compare, t>::create_middle_node()
{
    bptree_node_middle* node = _allocator.template allocate_object<bptree_node_middle>();
    _allocator.template construct<bptree_node_middle>(node);
    return node;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::destroy_node(bptree_node_base* node) noexcept
{
    if (node == nullptr) {
        return;
    }
    if (node->_is_terminate) {
        _allocator.template delete_object<bptree_node_term>(static_cast<bptree_node_term*>(node));
        return;
    }
    bptree_node_middle* middle = static_cast<bptree_node_middle*>(node);
    for (size_t i = 0; i < middle->_pointers.size(); ++i) {
        destroy_node(middle->_pointers[i]);
    }
    _allocator.template delete_object<bptree_node_middle>(middle);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::clear_nodes() noexcept
{
    destroy_node(_root);
    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term*
BP_tree<tkey, tvalue, compare, t>::find_left_leaf() const noexcept
{
    if (_root == nullptr) {
        return nullptr;
    }
    bptree_node_base* node = _root;
    while (!node->_is_terminate) {
        bptree_node_middle* middle = static_cast<bptree_node_middle*>(node);
        node = middle->_pointers.front();
    }
    return static_cast<bptree_node_term*>(node);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term*
BP_tree<tkey, tvalue, compare, t>::find_leaf_by_key(const tkey& key) const noexcept
{
    bptree_node_base* node = _root;
    while (node != nullptr && !node->_is_terminate) {
        bptree_node_middle* middle = static_cast<bptree_node_middle*>(node);
        size_t index = find_child_index(middle, key);
        node = middle->_pointers[index];
    }
    return static_cast<bptree_node_term*>(node);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term*, size_t>
BP_tree<tkey, tvalue, compare, t>::find_leaf_and_index(const tkey& key)
{
    bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return {nullptr, 0};
    }
    size_t index = find_leaf_position(leaf, key);
    if (index >= leaf->_data.size() || !equal_keys(leaf->_data[index].first, key)) {
        return {nullptr, 0};
    }
    return {leaf, index};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<const typename BP_tree<tkey, tvalue, compare, t>::bptree_node_term*, size_t>
BP_tree<tkey, tvalue, compare, t>::find_leaf_and_index(const tkey& key) const
{
    const bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return {nullptr, 0};
    }
    size_t index = find_leaf_position(leaf, key);
    if (index >= leaf->_data.size() || !equal_keys(leaf->_data[index].first, key)) {
        return {nullptr, 0};
    }
    return {leaf, index};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::find_leaf_position(const bptree_node_term* leaf, const tkey& key) const noexcept
{
    size_t left = 0;
    size_t right = leaf->_data.size();
    while (left < right) {
        size_t middle = left + ((right - left) >> 1);
        if (compare_keys(leaf->_data[middle].first, key)) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::find_child_index(const bptree_node_middle* node, const tkey& key) const noexcept
{
    size_t index = 0;
    while (index < node->_keys.size() && !compare_keys(key, node->_keys[index])) {
        ++index;
    }
    return index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::node_size(const bptree_node_base* node) const noexcept
{
    if (node->_is_terminate) {
        return static_cast<const bptree_node_term*>(node)->_data.size();
    }
    return static_cast<const bptree_node_middle*>(node)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::insert_result
BP_tree<tkey, tvalue, compare, t>::insert_into_node(bptree_node_base* node, value_type&& data)
{
    if (node == nullptr) {
        return {true, true, std::move(data.first), nullptr};
    }
    if (node->_is_terminate) {
        bptree_node_term* leaf = static_cast<bptree_node_term*>(node);
        size_t index = find_leaf_position(leaf, data.first);
        if (index < leaf->_data.size() && equal_keys(leaf->_data[index].first, data.first)) {
            return {false, false, tkey(), nullptr};
        }
        leaf->_data.insert(leaf->_data.begin() + static_cast<std::ptrdiff_t>(index), std::move(data));
        if (leaf->_data.size() <= maximum_keys_in_node) {
            return {true, false, tkey(), nullptr};
        }
        bptree_node_term* sibling = create_leaf_node();
        for (size_t i = t; i < leaf->_data.size(); ++i) {
            sibling->_data.push_back(std::move(leaf->_data[i]));
        }
        leaf->_data.erase(leaf->_data.begin() + static_cast<std::ptrdiff_t>(t), leaf->_data.end());
        sibling->_next = leaf->_next;
        leaf->_next = sibling;
        return {true, true, sibling->_data.front().first, sibling};
    }
    bptree_node_middle* middle = static_cast<bptree_node_middle*>(node);
    size_t child_index = find_child_index(middle, data.first);
    insert_result child_result = insert_into_node(middle->_pointers[child_index], std::move(data));
    if (!child_result.inserted) {
        return child_result;
    }
    if (!child_result.split) {
        return child_result;
    }
    middle->_keys.insert(middle->_keys.begin() + static_cast<std::ptrdiff_t>(child_index),
        std::move(child_result.promote_key));
    middle->_pointers.insert(middle->_pointers.begin() + static_cast<std::ptrdiff_t>(child_index) + 1,
        child_result.right);
    if (middle->_keys.size() <= maximum_keys_in_node) {
        return {true, false, tkey(), nullptr};
    }
    bptree_node_middle* sibling = create_middle_node();
    tkey promoted_key = std::move(middle->_keys[t]);
    for (size_t i = t + 1; i < middle->_keys.size(); ++i) {
        sibling->_keys.push_back(std::move(middle->_keys[i]));
    }
    for (size_t i = t + 1; i < middle->_pointers.size(); ++i) {
        sibling->_pointers.push_back(middle->_pointers[i]);
    }
    middle->_keys.erase(middle->_keys.begin() + static_cast<std::ptrdiff_t>(t), middle->_keys.end());
    middle->_pointers.erase(middle->_pointers.begin() + static_cast<std::ptrdiff_t>(t) + 1, middle->_pointers.end());
    return {true, true, promoted_key, sibling};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::delete_result
BP_tree<tkey, tvalue, compare, t>::delete_from_node(bptree_node_base* node, const tkey& key)
{
    if (node->_is_terminate) {
        bptree_node_term* leaf = static_cast<bptree_node_term*>(node);
        size_t index = find_leaf_position(leaf, key);
        if (index >= leaf->_data.size() || !equal_keys(leaf->_data[index].first, key)) {
            return {false, false, false, tkey()};
        }
        bool first_key_removed = (index == 0);
        leaf->_data.erase(leaf->_data.begin() + static_cast<std::ptrdiff_t>(index));
        bool underfull = leaf != _root && leaf->_data.size() < minimum_keys_in_node;
        bool first_key_changed = first_key_removed && !leaf->_data.empty();
        tkey new_first_key = first_key_changed ? leaf->_data.front().first : tkey();
        return {true, underfull, first_key_changed, std::move(new_first_key)};
    }

    bptree_node_middle* middle = static_cast<bptree_node_middle*>(node);
    size_t child_index = find_child_index(middle, key);
    delete_result result = delete_from_node(middle->_pointers[child_index], key);
    if (!result.deleted) {
        return result;
    }
    if (result.first_key_changed && child_index > 0) {
        middle->_keys[child_index - 1] = std::move(result.first_key);
    }
    if (!result.underfull) {
        return {true, false, false, tkey()};
    }
    if (child_index > 0 && take_from_prev(middle, child_index)) {
        return {true, false, false, tkey()};
    }
    if (child_index + 1 < middle->_pointers.size() && take_from_next(middle, child_index)) {
        return {true, false, false, tkey()};
    }
    if (child_index == 0) {
        merge(middle, child_index);
    } else if (child_index + 1 < middle->_pointers.size()) {
        size_t left_size = node_size(middle->_pointers[child_index - 1]);
        size_t right_size = node_size(middle->_pointers[child_index + 1]);
        if (right_size >= left_size) {
            merge(middle, child_index);
        } else {
            merge(middle, child_index - 1);
        }
    } else {
        merge(middle, child_index - 1);
    }
    bool underfull = middle != _root && middle->_keys.size() < minimum_keys_in_node;
    return {true, underfull, false, tkey()};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::take_from_prev(bptree_node_middle* parent, size_t index)
{
    bptree_node_base* child = parent->_pointers[index];
    bptree_node_base* left_sibling = parent->_pointers[index - 1];
    if (child->_is_terminate) {
        bptree_node_term* leaf = static_cast<bptree_node_term*>(child);
        bptree_node_term* left_leaf = static_cast<bptree_node_term*>(left_sibling);
        if (left_leaf->_data.size() <= minimum_keys_in_node) {
            return false;
        }
        leaf->_data.insert(leaf->_data.begin(), std::move(left_leaf->_data.back()));
        left_leaf->_data.pop_back();
        parent->_keys[index - 1] = leaf->_data.front().first;
        return true;
    }
    bptree_node_middle* internal_child = static_cast<bptree_node_middle*>(child);
    bptree_node_middle* left_internal = static_cast<bptree_node_middle*>(left_sibling);
    if (left_internal->_keys.size() < t) {
        return false;
    }
    internal_child->_keys.insert(internal_child->_keys.begin(), std::move(parent->_keys[index - 1]));
    internal_child->_pointers.insert(internal_child->_pointers.begin(), left_internal->_pointers.back());
    left_internal->_pointers.pop_back();
    parent->_keys[index - 1] = std::move(left_internal->_keys.back());
    left_internal->_keys.pop_back();
    return true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::take_from_next(bptree_node_middle* parent, size_t index)
{
    bptree_node_base* child = parent->_pointers[index];
    bptree_node_base* right_sibling = parent->_pointers[index + 1];
    if (child->_is_terminate) {
        bptree_node_term* leaf = static_cast<bptree_node_term*>(child);
        bptree_node_term* right_leaf = static_cast<bptree_node_term*>(right_sibling);
        if (right_leaf->_data.size() <= minimum_keys_in_node) {
            return false;
        }
        leaf->_data.push_back(std::move(right_leaf->_data.front()));
        right_leaf->_data.erase(right_leaf->_data.begin());
        parent->_keys[index] = right_leaf->_data.front().first;
        return true;
    }
    bptree_node_middle* internal_child = static_cast<bptree_node_middle*>(child);
    bptree_node_middle* right_internal = static_cast<bptree_node_middle*>(right_sibling);
    if (right_internal->_keys.size() < t) {
        return false;
    }
    internal_child->_keys.push_back(std::move(parent->_keys[index]));
    internal_child->_pointers.push_back(right_internal->_pointers.front());
    right_internal->_pointers.erase(right_internal->_pointers.begin());
    parent->_keys[index] = std::move(right_internal->_keys.front());
    right_internal->_keys.erase(right_internal->_keys.begin());
    return true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::merge(bptree_node_middle* parent, size_t index)
{
    bptree_node_base* child = parent->_pointers[index];
    bptree_node_base* sibling = parent->_pointers[index + 1];
    if (child->_is_terminate) {
        bptree_node_term* leaf = static_cast<bptree_node_term*>(child);
        bptree_node_term* right_leaf = static_cast<bptree_node_term*>(sibling);
        for (size_t i = 0; i < right_leaf->_data.size(); ++i) {
            leaf->_data.push_back(std::move(right_leaf->_data[i]));
        }
        leaf->_next = right_leaf->_next;
        parent->_pointers.erase(parent->_pointers.begin() + static_cast<std::ptrdiff_t>(index) + 1);
        parent->_keys.erase(parent->_keys.begin() + static_cast<std::ptrdiff_t>(index));
        _allocator.template delete_object<bptree_node_term>(right_leaf);
        return;
    }
    bptree_node_middle* internal_child = static_cast<bptree_node_middle*>(child);
    bptree_node_middle* internal_sibling = static_cast<bptree_node_middle*>(sibling);
    internal_child->_keys.push_back(std::move(parent->_keys[index]));
    for (size_t i = 0; i < internal_sibling->_keys.size(); ++i) {
        internal_child->_keys.push_back(std::move(internal_sibling->_keys[i]));
    }
    for (size_t i = 0; i < internal_sibling->_pointers.size(); ++i) {
        internal_child->_pointers.push_back(internal_sibling->_pointers[i]);
    }
    parent->_pointers.erase(parent->_pointers.begin() + static_cast<std::ptrdiff_t>(index) + 1);
    parent->_keys.erase(parent->_keys.begin() + static_cast<std::ptrdiff_t>(index));
    _allocator.template delete_object<bptree_node_middle>(internal_sibling);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(const compare& cmp, pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(pp_allocator<value_type> alloc, const compare& cmp)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
BP_tree<tkey, tvalue, compare, t>::BP_tree(iterator begin, iterator end, const compare& cmp, pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
    for (; begin != end; ++begin) {
        emplace(begin->first, begin->second);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp, pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
    for (const std::pair<tkey, tvalue>* element = data.begin(); element != data.end(); ++element) {
        emplace(element->first, element->second);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(const BP_tree& other)
    : compare(static_cast<const compare&>(other)), _allocator(other._allocator), _root(nullptr), _size(0)
{
    for (bptree_const_iterator it = other.cbegin(); it != other.cend(); ++it) {
        emplace(it->first, it->second);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::BP_tree(BP_tree&& other) noexcept
    : compare(std::move(static_cast<compare&>(other))), _allocator(std::move(other._allocator)), _root(other._root), _size(other._size)
{
    other._root = nullptr;
    other._size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>& BP_tree<tkey, tvalue, compare, t>::operator=(const BP_tree& other)
{
    if (this != &other) {
        BP_tree tmp(other);
        std::swap(static_cast<compare&>(*this), static_cast<compare&>(tmp));
        std::swap(_allocator, tmp._allocator);
        std::swap(_root, tmp._root);
        std::swap(_size, tmp._size);
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>& BP_tree<tkey, tvalue, compare, t>::operator=(BP_tree&& other) noexcept
{
    if (this != &other) {
        clear_nodes();
        static_cast<compare&>(*this) = std::move(static_cast<compare&>(other));
        _allocator = std::move(other._allocator);
        _root = other._root;
        _size = other._size;
        other._root = nullptr;
        other._size = 0;
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::~BP_tree() noexcept
{
    clear_nodes();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::bptree_iterator(bptree_node_term* node, size_t index)
    : _node(node), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::reference
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator*() const noexcept
{
    _value.emplace(_node->_data[_index].first, _node->_data[_index].second);
    return *_value;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::pointer
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator->() const noexcept
{
    _value.emplace(_node->_data[_index].first, _node->_data[_index].second);
    return &_value.value();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::self&
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator++()
{
    if (_node == nullptr) {
        return *this;
    }
    ++_index;
    if (_index < _node->_data.size()) {
        return *this;
    }
    _node = _node->_next;
    _index = 0;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator::self
BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator++(int)
{
    self result = *this;
    ++*this;
    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator==(const self& other) const noexcept
{
    return _node == other._node && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_iterator::current_node_keys_count() const noexcept
{
    if (_node == nullptr) {
        return 0;
    }
    return _node->_data.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::bptree_const_iterator(const bptree_node_term* node, size_t index)
    : _node(node), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::bptree_const_iterator(const bptree_iterator& it) noexcept
    : _node(it._node), _index(it._index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::reference
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator*() const noexcept
{
    _value.emplace(_node->_data[_index].first, _node->_data[_index].second);
    return *_value;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::pointer
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator->() const noexcept
{
    _value.emplace(_node->_data[_index].first, _node->_data[_index].second);
    return &_value.value();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::self&
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator++()
{
    if (_node == nullptr) {
        return *this;
    }
    ++_index;
    if (_index < _node->_data.size()) {
        return *this;
    }
    _node = _node->_next;
    _index = 0;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::self
BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator++(int)
{
    self result = *this;
    ++*this;
    return result;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator==(const self& other) const noexcept
{
    return _node == other._node && _index == other._index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::current_node_keys_count() const noexcept
{
    if (_node == nullptr) {
        return 0;
    }
    return _node->_data.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& BP_tree<tkey, tvalue, compare, t>::at(const tkey& key)
{
    std::pair<bptree_node_term*, size_t> found = find_leaf_and_index(key);
    bptree_node_term* leaf = found.first;
    size_t index = found.second;
    if (leaf == nullptr) {
        throw std::out_of_range("key not found");
    }
    return leaf->_data[index].second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& BP_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    std::pair<const bptree_node_term*, size_t> found = find_leaf_and_index(key);
    const bptree_node_term* leaf = found.first;
    size_t index = found.second;
    if (leaf == nullptr) {
        throw std::out_of_range("key not found");
    }
    return leaf->_data[index].second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& BP_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    emplace(key, tvalue());
    std::pair<bptree_node_term*, size_t> found = find_leaf_and_index(key);
    return found.first->_data[found.second].second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& BP_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    tkey lookup_key = key;
    emplace(std::move(key), tvalue());
    std::pair<bptree_node_term*, size_t> found = find_leaf_and_index(lookup_key);
    return found.first->_data[found.second].second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::begin()
{
    return bptree_iterator(find_left_leaf(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::end()
{
    return bptree_iterator(nullptr, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::begin() const
{
    return cbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::end() const
{
    return cend();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return bptree_const_iterator(find_left_leaf(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::cend() const
{
    return bptree_const_iterator(nullptr, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t BP_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return _size == 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return end();
    }
    size_t index = find_leaf_position(leaf, key);
    if (index >= leaf->_data.size() || !equal_keys(leaf->_data[index].first, key)) {
        return end();
    }
    return bptree_iterator(leaf, index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    const bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return cend();
    }
    size_t index = find_leaf_position(leaf, key);
    if (index >= leaf->_data.size() || !equal_keys(leaf->_data[index].first, key)) {
        return cend();
    }
    return bptree_const_iterator(leaf, index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return end();
    }
    size_t index = find_leaf_position(leaf, key);
    if (index < leaf->_data.size()) {
        return bptree_iterator(leaf, index);
    }
    if (leaf->_next == nullptr) {
        return end();
    }
    return bptree_iterator(leaf->_next, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    const bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return cend();
    }
    size_t index = find_leaf_position(leaf, key);
    if (index < leaf->_data.size()) {
        return bptree_const_iterator(leaf, index);
    }
    if (leaf->_next == nullptr) {
        return cend();
    }
    return bptree_const_iterator(leaf->_next, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator BP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return end();
    }
    size_t index = find_leaf_position(leaf, key);
    if (index < leaf->_data.size() && equal_keys(leaf->_data[index].first, key)) {
        ++index;
    }
    if (index < leaf->_data.size()) {
        return bptree_iterator(leaf, index);
    }
    if (leaf->_next == nullptr) {
        return end();
    }
    return bptree_iterator(leaf->_next, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_const_iterator BP_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    const bptree_node_term* leaf = find_leaf_by_key(key);
    if (leaf == nullptr) {
        return cend();
    }
    size_t index = find_leaf_position(leaf, key);
    if (index < leaf->_data.size() && equal_keys(leaf->_data[index].first, key)) {
        ++index;
    }
    if (index < leaf->_data.size()) {
        return bptree_const_iterator(leaf, index);
    }
    if (leaf->_next == nullptr) {
        return cend();
    }
    return bptree_const_iterator(leaf->_next, 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool BP_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != cend();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void BP_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    clear_nodes();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool>
BP_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    return insert(tree_data_type(data.first, data.second));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool>
BP_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    tkey key = data.first;
    if (_root == nullptr) {
        bptree_node_term* leaf = create_leaf_node();
        leaf->_data.push_back(std::move(data));
        _root = leaf;
        _size = 1;
        return {bptree_iterator(leaf, 0), true};
    }
    insert_result result = insert_into_node(_root, std::move(data));
    if (!result.inserted) {
        return {find(key), false};
    }
    if (result.split) {
        bptree_node_middle* new_root = create_middle_node();
        new_root->_keys.push_back(std::move(result.promote_key));
        new_root->_pointers.push_back(_root);
        new_root->_pointers.push_back(result.right);
        _root = new_root;
    }
    ++_size;
    return {find(key), true};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
std::pair<typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator, bool>
BP_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    tree_data_type temp(std::forward<Args>(args)...);
    return insert(std::move(temp));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    return insert_or_assign(tree_data_type(data.first, data.second));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    std::pair<bptree_node_term*, size_t> found = find_leaf_and_index(data.first);
    if (found.first != nullptr) {
        found.first->_data[found.second].second = std::move(data.second);
        return bptree_iterator(found.first, found.second);
    }
    return insert(std::move(data)).first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    tree_data_type temp(std::forward<Args>(args)...);
    std::pair<bptree_node_term*, size_t> found = find_leaf_and_index(temp.first);
    if (found.first != nullptr) {
        found.first->_data[found.second].second = std::move(temp.second);
        return bptree_iterator(found.first, found.second);
    }
    return insert(std::move(temp)).first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::erase(bptree_iterator pos)
{
    if (pos == end()) {
        return end();
    }
    std::optional<tkey> next_key;
    bptree_iterator next = pos;
    ++next;
    if (next != end()) {
        next_key = next->first;
    }
    tkey key = pos->first;
    delete_result result = delete_from_node(_root, key);
    if (result.deleted) {
        --_size;
    }
    if (_root != nullptr) {
        if (_root->_is_terminate) {
            bptree_node_term* leaf = static_cast<bptree_node_term*>(_root);
            if (leaf->_data.empty()) {
                _allocator.template delete_object<bptree_node_term>(leaf);
                _root = nullptr;
            }
        } else {
            bptree_node_middle* root_internal = static_cast<bptree_node_middle*>(_root);
            if (root_internal->_keys.empty()) {
                bptree_node_base* new_root = root_internal->_pointers.empty() ? nullptr : root_internal->_pointers.front();
                root_internal->_pointers.clear();
                _allocator.template delete_object<bptree_node_middle>(root_internal);
                _root = new_root;
            }
        }
    }
    if (next_key.has_value()) {
        return find(next_key.value());
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::erase(bptree_const_iterator pos)
{
    return erase(bptree_iterator(const_cast<bptree_node_term*>(pos._node), pos._index));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::erase(bptree_iterator beg, bptree_iterator en)
{
    if (beg == en) {
        return en;
    }
    std::optional<tkey> next_key;
    if (en != end()) {
        next_key = en->first;
    }
    std::vector<tkey> keys;
    for (bptree_iterator it = beg; it != en; ++it) {
        keys.push_back(it->first);
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        delete_result result = delete_from_node(_root, keys[i]);
        if (result.deleted) {
            --_size;
        }
    }
    if (next_key.has_value()) {
        return find(next_key.value());
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::erase(bptree_const_iterator beg, bptree_const_iterator en)
{
    return erase(bptree_iterator(const_cast<bptree_node_term*>(beg._node), beg._index),
                 bptree_iterator(const_cast<bptree_node_term*>(en._node), en._index));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename BP_tree<tkey, tvalue, compare, t>::bptree_iterator
BP_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    bptree_iterator it = find(key);
    if (it == end()) {
        return end();
    }
    bptree_iterator next = it;
    ++next;
    std::optional<tkey> next_key;
    if (next != end()) {
        next_key = next->first;
    }
    delete_result result = delete_from_node(_root, key);
    if (result.deleted) {
        --_size;
    }
    if (_root != nullptr) {
        if (_root->_is_terminate) {
            bptree_node_term* leaf = static_cast<bptree_node_term*>(_root);
            if (leaf->_data.empty()) {
                _allocator.template delete_object<bptree_node_term>(leaf);
                _root = nullptr;
            }
        } else {
            bptree_node_middle* root_internal = static_cast<bptree_node_middle*>(_root);
            if (root_internal->_keys.empty()) {
                bptree_node_base* new_root = root_internal->_pointers.empty() ? nullptr : root_internal->_pointers.front();
                root_internal->_pointers.clear();
                _allocator.template delete_object<bptree_node_middle>(root_internal);
                _root = new_root;
            }
        }
    }
    if (next_key.has_value()) {
        return find(next_key.value());
    }
    return end();
}

#endif
