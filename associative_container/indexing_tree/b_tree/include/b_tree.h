#ifndef SYS_PROG_B_TREE_H
#define SYS_PROG_B_TREE_H

#include <iterator>
#include <utility>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <not_implemented.h>
#include <initializer_list>
#include <optional>

template <typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5>
class B_tree final : private compare // EBCO
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

    // endregion comparators declaration


    struct btree_node
    {
        boost::container::static_vector<tree_data_type, maximum_keys_in_node + 1> _keys;
        boost::container::static_vector<btree_node*, maximum_keys_in_node + 2> _pointers;
        btree_node() noexcept;
    };

    pp_allocator<value_type> _allocator;
    btree_node* _root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

    btree_node* create_node();
    void delete_node(btree_node* node) noexcept;
    btree_node* copy_node(const btree_node* other);
    bool equal(const tkey& lhs, const tkey& rhs) const noexcept;
    size_t find_key_index(const btree_node* node, const tkey& key) const noexcept;
    std::pair<btree_node*, size_t> find_key(const tkey& key);
    std::pair<const btree_node*, size_t> find_key(const tkey& key) const;
    std::stack<std::pair<btree_node**, size_t>> path_to_min();
    std::stack<std::pair<btree_node* const*, size_t>> path_to_min() const;
    std::stack<std::pair<btree_node**, size_t>> path_to_max();
    std::stack<std::pair<btree_node* const*, size_t>> path_to_max() const;
    std::stack<std::pair<btree_node**, size_t>> path_to_key(const tkey& key);
    std::stack<std::pair<btree_node* const*, size_t>> path_to_key(const tkey& key) const;
    void split(btree_node* parent, size_t index);
    void insert_with_space(btree_node* node, tree_data_type&& data);
    bool remove_key(btree_node* node, const tkey& key);
    void remove_from_leaf(btree_node* node, size_t index);
    void remove_from_node(btree_node* node, size_t index);
    tree_data_type get_prev(btree_node* node, size_t index) const;
    tree_data_type get_next(btree_node* node, size_t index) const;
    void rebalance(btree_node* node, size_t index);
    void take_from_prev(btree_node* node, size_t index);
    void take_from_next(btree_node* node, size_t index);
    void merge(btree_node* node, size_t index);

public:

    // region constructors declaration

    explicit B_tree(const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit B_tree(pp_allocator<value_type> alloc, const compare& comp = compare());

    template<input_iterator_for_pair<tkey, tvalue> iterator>
    explicit B_tree(iterator begin, iterator end, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare& cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    B_tree(const B_tree& other);

    B_tree(B_tree&& other) noexcept;

    B_tree& operator=(const B_tree& other);

    B_tree& operator=(B_tree&& other) noexcept;

    ~B_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class btree_iterator;
    class btree_reverse_iterator;
    class btree_const_iterator;
    class btree_const_reverse_iterator;

    class btree_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;
        mutable std::optional<value_type> _key_and_value;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);

    };

    class btree_const_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;
        mutable std::optional<value_type> _key_and_value;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_iterator;
        friend class btree_const_reverse_iterator;

        btree_const_iterator(const btree_iterator& it) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    class btree_reverse_iterator final
    {
        std::stack<std::pair<btree_node**, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_reverse_iterator;

        friend class B_tree;
        friend class btree_iterator;
        friend class btree_const_iterator;
        friend class btree_const_reverse_iterator;

        btree_reverse_iterator(const btree_iterator& it) noexcept;
        operator btree_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_reverse_iterator(const std::stack<std::pair<btree_node**, size_t>>& path = std::stack<std::pair<btree_node**, size_t>>(), size_t index = 0);
    };

    class btree_const_reverse_iterator final
    {
        std::stack<std::pair<btree_node* const*, size_t>> _path;
        size_t _index;

    public:

        using value_type = tree_data_type_const;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = btree_const_reverse_iterator;

        friend class B_tree;
        friend class btree_reverse_iterator;
        friend class btree_const_iterator;
        friend class btree_iterator;

        btree_const_reverse_iterator(const btree_reverse_iterator& it) noexcept;
        operator btree_const_iterator() const noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        self& operator++();
        self operator++(int);

        self& operator--();
        self operator--(int);

        bool operator==(const self& other) const noexcept;
        bool operator!=(const self& other) const noexcept;

        size_t depth() const noexcept;
        size_t current_node_keys_count() const noexcept;
        bool is_terminate_node() const noexcept;
        size_t index() const noexcept;

        explicit btree_const_reverse_iterator(const std::stack<std::pair<btree_node* const*, size_t>>& path = std::stack<std::pair<btree_node* const*, size_t>>(), size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;
    friend class btree_reverse_iterator;
    friend class btree_const_reverse_iterator;

    // endregion iterators declaration

    // region element access declaration

    /*
     * Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
     */
    tvalue& at(const tkey&);
    const tvalue& at(const tkey&) const;

    /*
     * If key not exists, makes default initialization of value
     */
    tvalue& operator[](const tkey& key);
    tvalue& operator[](tkey&& key);

    // endregion element access declaration
    // region iterator begins declaration

    btree_iterator begin();
    btree_iterator end();

    btree_const_iterator begin() const;
    btree_const_iterator end() const;

    btree_const_iterator cbegin() const;
    btree_const_iterator cend() const;

    btree_reverse_iterator rbegin();
    btree_reverse_iterator rend();

    btree_const_reverse_iterator rbegin() const;
    btree_const_reverse_iterator rend() const;

    btree_const_reverse_iterator crbegin() const;
    btree_const_reverse_iterator crend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    size_t size() const noexcept;
    bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    btree_iterator find(const tkey& key);
    btree_const_iterator find(const tkey& key) const;

    btree_iterator lower_bound(const tkey& key);
    btree_const_iterator lower_bound(const tkey& key) const;

    btree_iterator upper_bound(const tkey& key);
    btree_const_iterator upper_bound(const tkey& key) const;

    bool contains(const tkey& key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<btree_iterator, bool> insert(const tree_data_type& data);
    std::pair<btree_iterator, bool> insert(tree_data_type&& data);

    template <typename ...Args>
    std::pair<btree_iterator, bool> emplace(Args&&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    btree_iterator insert_or_assign(const tree_data_type& data);
    btree_iterator insert_or_assign(tree_data_type&& data);

    template <typename ...Args>
    btree_iterator emplace_or_assign(Args&&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    btree_iterator erase(btree_iterator pos);
    btree_iterator erase(btree_const_iterator pos);

    btree_iterator erase(btree_iterator beg, btree_iterator en);
    btree_iterator erase(btree_const_iterator beg, btree_const_iterator en);


    btree_iterator erase(const tkey& key);

    // endregion modifiers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type> compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>,
        std::size_t t = 5, typename U>
B_tree(iterator begin, iterator end, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<typename std::iterator_traits<iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey>, std::size_t t = 5, typename U>
B_tree(std::initializer_list<std::pair<tkey, tvalue>> data, const compare &cmp = compare(), pp_allocator<U> = pp_allocator<U>()) -> B_tree<tkey, tvalue, compare, t>;

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_pairs(const B_tree::tree_data_type &lhs,
                                                     const B_tree::tree_data_type &rhs) const
{
    return compare_keys(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::compare_keys(const tkey &lhs, const tkey &rhs) const
{
    return compare::operator()(lhs, rhs);
}


template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_node::btree_node() noexcept
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
pp_allocator<typename B_tree<tkey, tvalue, compare, t>::value_type> B_tree<tkey, tvalue, compare, t>::get_allocator() const noexcept
{
    return _allocator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::equal(const tkey &lhs, const tkey &rhs) const noexcept
{
    return !compare_keys(lhs, rhs) && !compare_keys(rhs, lhs);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_node* B_tree<tkey, tvalue, compare, t>::create_node()
{
    return _allocator.template new_object<btree_node>();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::delete_node(btree_node* node) noexcept
{
    if (node == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < node->_pointers.size(); ++i) {
        delete_node(node->_pointers[i]);
    }
    _allocator.delete_object(node);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_node* B_tree<tkey, tvalue, compare, t>::copy_node(const btree_node* other)
{
    if (other == nullptr) {
        return nullptr;
    }
    btree_node* node = create_node();
    node->_keys = other->_keys;
    if (!other->_pointers.empty()) {
        node->_pointers.reserve(other->_pointers.size());
        for (std::size_t i = 0; i < other->_pointers.size(); ++i) {
            node->_pointers.push_back(copy_node(other->_pointers[i]));
        }
    }
    return node;
}

// region constructors implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        pp_allocator<value_type> alloc,\
        const compare& comp)
    : compare(comp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<input_iterator_for_pair<tkey, tvalue> iterator>
B_tree<tkey, tvalue, compare, t>::B_tree(
        iterator begin,
        iterator end,
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
    for (; begin != end; ++begin) {
        emplace(begin->first, begin->second);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(
        std::initializer_list<std::pair<tkey, tvalue>> data,
        const compare& cmp,
        pp_allocator<value_type> alloc)
    : compare(cmp), _allocator(std::move(alloc)), _root(nullptr), _size(0)
{
    for (const std::pair<tkey, tvalue>* el = data.begin(); el != data.end(); ++el) {
        emplace(el->first, el->second);
    }
}

// endregion constructors implementation

// region five implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::~B_tree() noexcept
{
    delete_node(_root);
    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(const B_tree& other)
    : compare(static_cast<const compare&>(other)), _allocator(other._allocator), _root(copy_node(other._root)), _size(other._size)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(const B_tree& other)
{
    if (this != &other) {
        B_tree tree(other);
        std::swap(static_cast<compare&>(*this), static_cast<compare&>(tree));
        std::swap(_allocator, tree._allocator);
        std::swap(_root, tree._root);
        std::swap(_size, tree._size);
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::B_tree(B_tree&& other) noexcept
    : compare(std::move(static_cast<compare&>(other))), _allocator(std::move(other._allocator)), _root(other._root), _size(other._size)
{
    other._root = nullptr;
    other._size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>& B_tree<tkey, tvalue, compare, t>::operator=(B_tree&& other) noexcept
{
    if (this != &other) {
        delete_node(_root);
        static_cast<compare&>(*this) = std::move(static_cast<compare&>(other));
        _allocator = std::move(other._allocator);
        _root = other._root;
        _size = other._size;
        other._root = nullptr;
        other._size = 0;
    }
    return *this;
}

// endregion five implementation

// region iterators implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_iterator::btree_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
    : _path(path), _index(index), _key_and_value(std::nullopt)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator*() const noexcept
{
    const btree_node* node = *(_path.top().first);
    const tree_data_type& el = node->_keys[_index];
    _key_and_value.emplace(el.first, el.second);
    return *_key_and_value;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator->() const noexcept
{
    return std::addressof(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* node = *(_path.top().first);
    if (node->_pointers.empty()) {
        if (_index + 1 < node->_keys.size()) {
            ++_index;
            return *this;
        }
    } else {
        btree_node** child_ptr = &node->_pointers[_index + 1];
        btree_node* child = *child_ptr;
        _path.push({child_ptr, _index + 1});
        while (!child->_pointers.empty()) {
            child_ptr = &child->_pointers[0];
            child = *child_ptr;
            _path.push({child_ptr, 0});
        }
        _index = 0;
        return *this;
    }
    while (!_path.empty()) {
        std::pair<btree_node**, std::size_t> el = _path.top();
        btree_node** node_ptr = el.first;
        std::size_t index = el.second;
        _path.pop();
        if (_path.empty()) {
            _index = 0;
            return *this;
        }
        std::pair<btree_node**, std::size_t>& parent = _path.top();
        btree_node* parent_node = *parent.first;
        if (index < parent_node->_keys.size()) {
            _index = index;
            return *this;
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator++(int)
{
    self prev = *this;
    ++*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator&
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node** node_ptr = _path.top().first;
    btree_node* node = *node_ptr;
    if (!node->_pointers.empty()) {
        btree_node** child_ptr = &node->_pointers[_index];
        btree_node* child = *child_ptr;
        _path.push({child_ptr, _index});
        while (!child->_pointers.empty()) {
            child_ptr = &child->_pointers.back();
            child = *child_ptr;
            _path.push({child_ptr, child->_keys.size() - 1});
        }
        _index = child->_keys.size() - 1;
        return *this;
    }
    if (_index > 0) {
        --_index;
        return *this;
    }
    _path.pop();
    while (!_path.empty()) {
        std::pair<btree_node**, std::size_t> el = _path.top();
        btree_node** parent_ptr = el.first;
        std::size_t index = el.second;
        if (index > 0) {
            _index = index - 1;
            return *this;
        }
        _path.pop();
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::btree_iterator::operator--(int)
{
    self prev = *this;
    --*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }
    std::stack<std::pair<btree_node**, size_t>> left = _path;
    std::stack<std::pair<btree_node**, size_t>> right = other._path;
    while (!left.empty()) {
        if (left.top() != right.top()) {
            return false;
        }
        left.pop();
        right.pop();
    }
    return true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::depth() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return true;
    }
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
    : _path(path), _index(index), _key_and_value(std::nullopt)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::btree_const_iterator(
        const btree_iterator& it) noexcept
    : _path(), _index(it._index), _key_and_value(std::nullopt)
{
    std::stack<std::pair<btree_node**, size_t>> path = it._path;
    std::vector<std::pair<btree_node* const*, size_t>> elements;
    while (!path.empty()) {
        std::pair<btree_node**, size_t> el = path.top();
        elements.emplace_back(el.first, el.second);
        path.pop();
    }
    for (std::size_t i = elements.size(); i > 0; --i) {
        _path.push(elements[i - 1]);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator*() const noexcept
{
    const btree_node* node = *(_path.top().first);
    const tree_data_type& el = node->_keys[_index];
    _key_and_value.emplace(el.first, el.second);
    return *_key_and_value;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator->() const noexcept
{
    return std::addressof(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++()
{
    if (_path.empty()) {
        return *this;
    }
    const btree_node* node = *(_path.top().first);
    if (node->_pointers.empty()) {
        if (_index + 1 < node->_keys.size()) {
            ++_index;
            return *this;
        }
    } else {
        btree_node* const* child_ptr = &node->_pointers[_index + 1];
        const btree_node* child = *child_ptr;
        _path.push({child_ptr, _index + 1});
        while (!child->_pointers.empty()) {
            child_ptr = &child->_pointers[0];
            child = *child_ptr;
            _path.push({child_ptr, 0});
        }
        _index = 0;
        return *this;
    }
    while (!_path.empty()) {
        std::pair<btree_node* const*, std::size_t> el = _path.top();
        std::size_t index = el.second;
        _path.pop();
        if (_path.empty()) {
            _index = 0;
            return *this;
        }
        std::pair<btree_node* const*, std::size_t>& parent = _path.top();
        const btree_node* parent_node = *parent.first;
        if (index < parent_node->_keys.size()) {
            _index = index;
            return *this;
        }
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator++(int)
{
    self prev = *this;
    ++*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--()
{
    if (_path.empty()) {
        return *this;
    }
    btree_node* const* node_ptr = _path.top().first;
    const btree_node* node = *node_ptr;
    if (!node->_pointers.empty()) {
        btree_node* const* child_ptr = &node->_pointers[_index];
        const btree_node* child = *child_ptr;
        _path.push({child_ptr, _index});
        while (!child->_pointers.empty()) {
            child_ptr = &child->_pointers.back();
            child = *child_ptr;
            _path.push({child_ptr, child->_keys.size() - 1});
        }
        _index = child->_keys.size() - 1;
        return *this;
    }
    if (_index > 0) {
        --_index;
        return *this;
    }
    _path.pop();
    while (!_path.empty()) {
        std::pair<btree_node* const*, std::size_t> el = _path.top();
        std::size_t index = el.second;
        if (index > 0) {
            _index = index - 1;
            return *this;
        }
        _path.pop();
    }
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator--(int)
{
    self prev = *this;
    --*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }
    std::stack<std::pair<btree_node* const*, size_t>> left = _path;
    std::stack<std::pair<btree_node* const*, size_t>> right = other._path;
    while (!left.empty()) {
        if (left.top() != right.top()) {
            return false;
        }
        left.pop();
        right.pop();
    }
    return true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::depth() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return _path.size() - 1;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return true;
    }
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const std::stack<std::pair<btree_node**, size_t>>& path, size_t index)
    : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::btree_reverse_iterator(
        const btree_iterator& it) noexcept
    : _path(it._path), _index(it._index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_iterator() const noexcept
{
    return btree_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator*() const noexcept
{
    const btree_node* node = *(_path.top().first);
    const tree_data_type& el = node->_keys[_index];
    static thread_local std::optional<value_type> key_and_value;
    key_and_value.emplace(el.first, el.second);
    return *key_and_value;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator->() const noexcept
{
    return std::addressof(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++()
{
    btree_iterator iterator(_path, _index);
    --iterator;
    _path = iterator._path;
    _index = iterator._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator++(int)
{
    self prev = *this;
    ++*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--()
{
    btree_iterator iterator(_path, _index);
    ++iterator;
    _path = iterator._path;
    _index = iterator._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator--(int)
{
    self prev = *this;
    --*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }
    std::stack<std::pair<btree_node**, size_t>> left = _path;
    std::stack<std::pair<btree_node**, size_t>> right = other._path;
    while (!left.empty()) {
        if (left.top() != right.top()) {
            return false;
        }
        left.pop();
        right.pop();
    }
    return true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::depth() const noexcept
{
    return _path.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return true;
    }
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator::index() const noexcept
{
    return _index;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const std::stack<std::pair<btree_node* const*, size_t>>& path, size_t index)
    : _path(path), _index(index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::btree_const_reverse_iterator(
        const btree_reverse_iterator& it) noexcept
    : _path(it._path), _index(it._index)
{
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator B_tree<tkey, tvalue, compare, t>::btree_const_iterator() const noexcept
{
    return btree_const_iterator(_path, _index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::reference
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator*() const noexcept
{
    const btree_node* node = *(_path.top().first);
    const tree_data_type& el = node->_keys[_index];
    static thread_local std::optional<value_type> key_and_value;
    key_and_value.emplace(el.first, el.second);
    return *key_and_value;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::pointer
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator->() const noexcept
{
    return std::addressof(operator*());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++()
{
    btree_const_iterator iterator(_path, _index);
    --iterator;
    _path = iterator._path;
    _index = iterator._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator++(int)
{
    self prev = *this;
    ++*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator&
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--()
{
    btree_const_iterator iterator(_path, _index);
    ++iterator;
    _path = iterator._path;
    _index = iterator._index;
    return *this;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator
B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator--(int)
{
    self prev = *this;
    --*this;
    return prev;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator==(const self& other) const noexcept
{
    if (_index != other._index || _path.size() != other._path.size()) {
        return false;
    }
    std::stack<std::pair<btree_node* const*, size_t>> left = _path;
    std::stack<std::pair<btree_node* const*, size_t>> right = other._path;
    while (!left.empty()) {
        if (left.top() != right.top()) {
            return false;
        }
        left.pop();
        right.pop();
    }
    return true;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::operator!=(const self& other) const noexcept
{
    return !(*this == other);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::depth() const noexcept
{
    return _path.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::current_node_keys_count() const noexcept
{
    if (_path.empty()) {
        return 0;
    }
    return (*_path.top().first)->_keys.size();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::is_terminate_node() const noexcept
{
    if (_path.empty()) {
        return true;
    }
    return (*_path.top().first)->_pointers.empty();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator::index() const noexcept
{
    return _index;
}

// endregion iterators implementation

// region element access implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key)
{
    std::pair<btree_node*, size_t> found = find_key(key);
    btree_node* node = found.first;
    size_t index = found.second;
    if (node == nullptr) {
        throw std::out_of_range("Key not found");
    }
    return node->_keys[index].second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
const tvalue& B_tree<tkey, tvalue, compare, t>::at(const tkey& key) const
{
    std::pair<const btree_node*, size_t> found = find_key(key);
    const btree_node* node = found.first;
    size_t index = found.second;
    if (node == nullptr) {
        throw std::out_of_range("Key not found");
    }
    return node->_keys[index].second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](const tkey& key)
{
    std::pair<btree_iterator, bool> pair = emplace(key, tvalue());
    btree_iterator iterator = pair.first;
    return iterator->second;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
tvalue& B_tree<tkey, tvalue, compare, t>::operator[](tkey&& key)
{
    std::pair<btree_iterator, bool> pair = emplace(std::move(key), tvalue());
    btree_iterator iterator = pair.first;
    return iterator->second;
}

// endregion element access implementation

// region iterator begins implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::begin()
{
    return btree_iterator(path_to_min(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::end()
{
    return btree_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::begin() const
{
    return btree_const_iterator(path_to_min(), 0);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::end() const
{
    return btree_const_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cbegin() const
{
    return begin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::cend() const
{
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin()
{
    std::stack<std::pair<btree_node**, size_t>> path = path_to_max();
    if (path.empty()) {
        return btree_reverse_iterator();
    }
    return btree_reverse_iterator(path, path.top().second);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend()
{
    return btree_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rbegin() const
{
    std::stack<std::pair<btree_node* const*, size_t>> path = path_to_max();
    if (path.empty()) {
        return btree_const_reverse_iterator();
    }
    return btree_const_reverse_iterator(path, path.top().second);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::rend() const
{
    return btree_const_reverse_iterator();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crbegin() const
{
    return rbegin();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_reverse_iterator B_tree<tkey, tvalue, compare, t>::crend() const
{
    return rend();
}

// endregion iterator begins implementation

// region lookup implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::size() const noexcept
{
    return _size;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::empty() const noexcept
{
    return _size == 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::stack<std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node**, size_t>>
B_tree<tkey, tvalue, compare, t>::path_to_min()
{
    std::stack<std::pair<btree_node**, size_t>> path;
    if (_root == nullptr) {
        return path;
    }
    btree_node** node = &_root;
    while (*node != nullptr) {
        path.push({node, 0});
        if ((*node)->_pointers.empty()) {
            break;
        }
        node = &(*node)->_pointers[0];
    }
    return path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::stack<std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node* const*, size_t>>
B_tree<tkey, tvalue, compare, t>::path_to_min() const
{
    std::stack<std::pair<btree_node* const*, size_t>> path;
    if (_root == nullptr) {
        return path;
    }
    btree_node* const* node = &_root;
    while (*node != nullptr) {
        path.push({node, 0});
        if ((*node)->_pointers.empty()) {
            break;
        }
        node = &(*node)->_pointers[0];
    }
    return path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::stack<std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node**, size_t>>
B_tree<tkey, tvalue, compare, t>::path_to_max()
{
    std::stack<std::pair<btree_node**, size_t>> path;
    if (_root == nullptr) {
        return path;
    }
    btree_node** node = &_root;
    while (*node != nullptr) {
        size_t index = 0;
        if (!(*node)->_keys.empty()) {
            index = (*node)->_keys.size() - 1;
        }
        path.push({node, index});
        if ((*node)->_pointers.empty()) {
            break;
        }
        node = &(*node)->_pointers.back();
    }
    return path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::stack<std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node* const*, size_t>>
B_tree<tkey, tvalue, compare, t>::path_to_max() const
{
    std::stack<std::pair<btree_node* const*, size_t>> path;
    if (_root == nullptr) {
        return path;
    }
    btree_node* const* node = &_root;
    while (*node != nullptr) {
        size_t index = 0;
        if (!(*node)->_keys.empty()) {
            index = (*node)->_keys.size() - 1;
        }
        path.push({node, index});
        if ((*node)->_pointers.empty()) {
            break;
        }
        node = &(*node)->_pointers.back();
    }
    return path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::stack<std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node**, size_t>>
B_tree<tkey, tvalue, compare, t>::path_to_key(const tkey& key)
{
    std::stack<std::pair<btree_node**, size_t>> path;
    if (_root == nullptr) {
        return path;
    }
    btree_node** node = &_root;
    while (*node != nullptr) {
        btree_node* current = *node;
        size_t index = find_key_index(current, key);
        path.push({node, index});
        if (index < current->_keys.size() && equal(current->_keys[index].first, key)) {
            break;
        }
        if (current->_pointers.empty()) {
            break;
        }
        node = &current->_pointers[index];
    }
    return path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::stack<std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node* const*, size_t>>
B_tree<tkey, tvalue, compare, t>::path_to_key(const tkey& key) const
{
    std::stack<std::pair<btree_node* const*, size_t>> path;
    if (_root == nullptr) {
        return path;
    }
    btree_node* const* node = &_root;
    while (*node != nullptr) {
        const btree_node* current = *node;
        size_t index = find_key_index(current, key);
        path.push({node, index});
        if (index < current->_keys.size() && equal(current->_keys[index].first, key)) {
            break;
        }
        if (current->_pointers.empty()) {
            break;
        }
        node = &current->_pointers[index];
    }
    return path;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
size_t B_tree<tkey, tvalue, compare, t>::find_key_index(const btree_node* node, const tkey& key) const noexcept
{
    size_t first = 0;
    size_t count = node->_keys.size();
    while (count > 0) {
        size_t half = count / 2;
        size_t mid = first + half;
        if (compare_keys(node->_keys[mid].first, key)) {
            first = mid + 1;
            count = count - half - 1;
        } else {
            count = half;
        }
    }
    return first;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_node*, size_t>
B_tree<tkey, tvalue, compare, t>::find_key(const tkey& key)
{
    std::stack<std::pair<btree_node**, size_t>> path = path_to_key(key);
    if (path.empty()) {
        return {nullptr, 0};
    }
    std::pair<btree_node**, size_t> pair = path.top();
    btree_node* node = *pair.first;
    size_t index = pair.second;
    if (index < node->_keys.size() && equal(node->_keys[index].first, key)) {
        return {node, index};
    }
    return {nullptr, 0};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<const typename B_tree<tkey, tvalue, compare, t>::btree_node*, size_t>
B_tree<tkey, tvalue, compare, t>::find_key(const tkey& key) const
{
    std::stack<std::pair<btree_node* const*, size_t>> path = path_to_key(key);
    if (path.empty()) {
        return {nullptr, 0};
    }
    std::pair<btree_node* const*, size_t> pair = path.top();
    const btree_node* node = *pair.first;
    size_t index = pair.second;
    if (index < node->_keys.size() && equal(node->_keys[index].first, key)) {
        return {node, index};
    }
    return {nullptr, 0};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key)
{
    std::stack<std::pair<btree_node**, size_t>> path = path_to_key(key);
    if (path.empty()) {
        return end();
    }
    std::pair<btree_node**, size_t> pair = path.top();
    btree_node* node = *pair.first;
    size_t index = pair.second;
    if (index < node->_keys.size() && equal(node->_keys[index].first, key)) {
        return btree_iterator(path, index);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::find(const tkey& key) const
{
    std::stack<std::pair<btree_node* const*, size_t>> path = path_to_key(key);
    if (path.empty()) {
        return end();
    }
    std::pair<btree_node* const*, size_t> pair = path.top();
    const btree_node* node = *pair.first;
    size_t index = pair.second;
    if (index < node->_keys.size() && equal(node->_keys[index].first, key)) {
        return btree_const_iterator(path, index);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }
    btree_iterator iterator = begin();
    btree_iterator the_end = end();
    while (iterator != the_end && compare_keys(iterator->first, key)) {
        ++iterator;
    }
    return iterator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::lower_bound(const tkey& key) const
{
    return const_cast<B_tree*>(this)->lower_bound(key);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key)
{
    btree_iterator iterator = lower_bound(key);
    btree_iterator the_end = end();
    while (iterator != the_end && !compare_keys(key, iterator->first)) {
        ++iterator;
    }
    return iterator;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_const_iterator B_tree<tkey, tvalue, compare, t>::upper_bound(const tkey& key) const
{
    return const_cast<B_tree*>(this)->upper_bound(key);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::contains(const tkey& key) const
{
    return find(key) != end();
}

// endregion lookup implementation

// region modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::clear() noexcept
{
    delete_node(_root);
    _root = nullptr;
    _size = 0;
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::split(btree_node* parent, size_t index)
{
    btree_node* child = parent->_pointers[index];
    btree_node* sibling = create_node();
    tree_data_type mid = std::move(child->_keys[t]);
    sibling->_keys.insert(sibling->_keys.end(), child->_keys.begin() + t + 1, child->_keys.end());
    child->_keys.erase(child->_keys.begin() + t, child->_keys.end());
    if (!child->_pointers.empty()) {
        sibling->_pointers.insert(sibling->_pointers.end(), child->_pointers.begin() + t + 1, child->_pointers.end());
        child->_pointers.erase(child->_pointers.begin() + t + 1, child->_pointers.end());
    }
    parent->_keys.insert(parent->_keys.begin() + index, std::move(mid));
    parent->_pointers.insert(parent->_pointers.begin() + index + 1, sibling);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::insert_with_space(btree_node* node, tree_data_type&& data)
{
    size_t index = find_key_index(node, data.first);
    if (node->_pointers.empty()) {
        if (index < node->_keys.size() && equal(node->_keys[index].first, data.first)) {
            return;
        }
        node->_keys.insert(node->_keys.begin() + index, std::move(data));
        return;
    }
    if (index < node->_keys.size() && equal(node->_keys[index].first, data.first)) {
        node->_keys[index].second = std::move(data.second);
        return;
    }
    btree_node* child = node->_pointers[index];
    insert_with_space(child, std::move(data));
    if (child->_keys.size() == maximum_keys_in_node + 1) {
        split(node, index);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(const tree_data_type& data)
{
    return insert(tree_data_type(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::insert(tree_data_type&& data)
{
    if (_root == nullptr) {
        _root = create_node();
        _root->_keys.push_back(std::move(data));
        ++_size;
        return {begin(), true};
    }
    btree_iterator iterator = find(data.first);
    if (iterator != end()) {
        return {iterator, false};
    }
    tkey key = data.first;
    insert_with_space(_root, std::move(data));
    if (_root->_keys.size() == maximum_keys_in_node + 1) {
        btree_node* old_root = _root;
        _root = create_node();
        _root->_pointers.push_back(old_root);
        split(_root, 0);
    }
    ++_size;
    return {find(key), true};
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
std::pair<typename B_tree<tkey, tvalue, compare, t>::btree_iterator, bool>
B_tree<tkey, tvalue, compare, t>::emplace(Args&&... args)
{
    return insert(tree_data_type(std::forward<Args>(args)...));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(const tree_data_type& data)
{
    return insert_or_assign(tree_data_type(data));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::insert_or_assign(tree_data_type&& data)
{
    if (_root == nullptr) {
        _root = create_node();
        _root->_keys.push_back(std::move(data));
        ++_size;
        return begin();
    }
    std::stack<std::pair<btree_node**, size_t>> path = path_to_key(data.first);
    if (!path.empty()) {
        std::pair<btree_node**, size_t> pair = path.top();
        btree_node* node = *pair.first;
        size_t index = pair.second;
        if (index < node->_keys.size() && equal(node->_keys[index].first, data.first)) {
            node->_keys[index].second = std::move(data.second);
            return btree_iterator(path, index);
        }
    }
    if (_root->_keys.size() == maximum_keys_in_node) {
        btree_node* old_root = _root;
        _root = create_node();
        _root->_pointers.push_back(old_root);
        split(_root, 0);
    }
    tkey key = data.first;
    insert_with_space(_root, std::move(data));
    ++_size;
    return find(key);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
template<typename... Args>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::emplace_or_assign(Args&&... args)
{
    return insert_or_assign(tree_data_type(std::forward<Args>(args)...));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::tree_data_type
B_tree<tkey, tvalue, compare, t>::get_prev(btree_node* node, size_t index) const
{
    btree_node* the_node = node->_pointers[index];
    while (!the_node->_pointers.empty()) {
        the_node = the_node->_pointers.back();
    }
    return the_node->_keys.back();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::tree_data_type
B_tree<tkey, tvalue, compare, t>::get_next(btree_node* node, size_t index) const
{
    btree_node* the_node = node->_pointers[index + 1];
    while (!the_node->_pointers.empty()) {
        the_node = the_node->_pointers.front();
    }
    return the_node->_keys.front();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::take_from_prev(btree_node* node, size_t index)
{
    btree_node* child = node->_pointers[index];
    btree_node* sibling = node->_pointers[index - 1];
    child->_keys.insert(child->_keys.begin(), std::move(node->_keys[index - 1]));
    if (!sibling->_pointers.empty()) {
        child->_pointers.insert(child->_pointers.begin(), sibling->_pointers.back());
        sibling->_pointers.pop_back();
    }
    node->_keys[index - 1] = std::move(sibling->_keys.back());
    sibling->_keys.pop_back();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::take_from_next(btree_node* node, size_t index)
{
    btree_node* child = node->_pointers[index];
    btree_node* sibling = node->_pointers[index + 1];
    child->_keys.push_back(std::move(node->_keys[index]));
    if (!sibling->_pointers.empty()) {
        child->_pointers.push_back(sibling->_pointers.front());
        sibling->_pointers.erase(sibling->_pointers.begin());
    }
    node->_keys[index] = std::move(sibling->_keys.front());
    sibling->_keys.erase(sibling->_keys.begin());
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::merge(btree_node* node, size_t index)
{
    btree_node* child = node->_pointers[index];
    btree_node* sibling = node->_pointers[index + 1];
    child->_keys.push_back(std::move(node->_keys[index]));
    child->_keys.insert(child->_keys.end(), sibling->_keys.begin(), sibling->_keys.end());
    if (!sibling->_pointers.empty()) {
        child->_pointers.insert(child->_pointers.end(), sibling->_pointers.begin(), sibling->_pointers.end());
        sibling->_pointers.clear();
    }
    node->_keys.erase(node->_keys.begin() + index);
    node->_pointers.erase(node->_pointers.begin() + index + 1);
    delete_node(sibling);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::rebalance(btree_node* node, size_t index)
{
    btree_node* child = node->_pointers[index];
    if (index != 0 && node->_pointers[index - 1]->_keys.size() >= t) {
        take_from_prev(node, index);
    } else if (index != node->_pointers.size() - 1 && node->_pointers[index + 1]->_keys.size() >= t) {
        take_from_next(node, index);
    } else {
        if (index != node->_pointers.size() - 1) {
            merge(node, index);
        } else {
            merge(node, index - 1);
        }
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::remove_from_leaf(btree_node* node, size_t index)
{
    node->_keys.erase(node->_keys.begin() + index);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
void B_tree<tkey, tvalue, compare, t>::remove_from_node(btree_node* node, size_t index)
{
    const tkey key = node->_keys[index].first;
    if (node->_pointers[index]->_keys.size() >= t) {
        tree_data_type prev = get_prev(node, index);
        node->_keys[index] = prev;
        remove_key(node->_pointers[index], prev.first);
    } else if (node->_pointers[index + 1]->_keys.size() >= t) {
        tree_data_type next = get_next(node, index);
        node->_keys[index] = next;
        remove_key(node->_pointers[index + 1], next.first);
    } else {
        merge(node, index);
        remove_key(node->_pointers[index], key);
    }
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool B_tree<tkey, tvalue, compare, t>::remove_key(btree_node* node, const tkey& key)
{
    size_t index = find_key_index(node, key);
    if (index < node->_keys.size() && equal(node->_keys[index].first, key)) {
        if (node->_pointers.empty()) {
            remove_from_leaf(node, index);
            return true;
        }
        remove_from_node(node, index);
        return true;
    }
    if (node->_pointers.empty()) {
        return false;
    }
    bool last = (index == node->_keys.size());
    if (node->_pointers[index]->_keys.size() < t) {
        rebalance(node, index);
    }
    if (last && index > node->_keys.size()) {
        index = node->_keys.size();
    }
    return remove_key(node->_pointers[index], key);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator pos)
{
    if (pos == end()) {
        return end();
    }
    tkey key = pos->first;
    btree_iterator next = pos;
    ++next;
    std::optional<tkey> next_key;
    if (next != end()) {
        next_key = next->first;
    }
    if (_root == nullptr) {
        return end();
    }
    if (!remove_key(_root, key)) {
        return end();
    }
    --_size;
    if (_root != nullptr && _root->_keys.empty()) {
        if (!_root->_pointers.empty()) {
            btree_node* old_root = _root;
            _root = _root->_pointers[0];
            old_root->_pointers.clear();
            delete_node(old_root);
        } else {
            delete_node(_root);
            _root = nullptr;
        }
    }
    if (next_key) {
        return lower_bound(*next_key);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator pos)
{
    if (pos == end()) {
        return end();
    }
    return erase(btree_iterator(pos._path, pos._index));
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_iterator beg, btree_iterator en)
{
    if (beg == en) {
        return en;
    }
    std::optional<tkey> key;
    if (en != end()) {
        key = en->first;
    }
    std::vector<tkey> keys;
    for (btree_iterator iterator = beg; iterator != en; ++iterator) {
        keys.push_back(iterator->first);
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        erase(keys[i]);
    }
    if (key) {
        return lower_bound(*key);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(btree_const_iterator beg, btree_const_iterator en)
{
    if (beg == en) {
        return end();
    }
    std::optional<tkey> key;
    if (en != end()) {
        key = en->first;
    }
    std::vector<tkey> keys;
    for (btree_const_iterator iterator = beg; iterator != en; ++iterator) {
        keys.push_back(iterator->first);
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        erase(keys[i]);
    }
    if (key) {
        return lower_bound(*key);
    }
    return end();
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
typename B_tree<tkey, tvalue, compare, t>::btree_iterator
B_tree<tkey, tvalue, compare, t>::erase(const tkey& key)
{
    if (_root == nullptr) {
        return end();
    }
    btree_iterator found = find(key);
    if (found == end()) {
        return end();
    }
    btree_iterator next = found;
    ++next;
    std::optional<tkey> next_key;
    if (next != end()) {
        next_key = next->first;
    }
    if (!remove_key(_root, key)) {
        return end();
    }
    --_size;
    if (_root != nullptr && _root->_keys.empty()) {
        if (!_root->_pointers.empty()) {
            btree_node* old_root = _root;
            _root = _root->_pointers[0];
            old_root->_pointers.clear();
            delete_node(old_root);
        } else {
            delete_node(_root);
            _root = nullptr;
        }
    }
    
    if (next_key) {
        return lower_bound(*next_key);
    }
    return end();
}

// endregion modifiers implementation

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_pairs(const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &lhs,
                   const typename B_tree<tkey, tvalue, compare, t>::tree_data_type &rhs)
{
    return compare()(lhs.first, rhs.first);
}

template<typename tkey, typename tvalue, comparator<tkey> compare, std::size_t t>
bool compare_keys(const tkey &lhs, const tkey &rhs)
{
    return compare()(lhs, rhs);
}


#endif