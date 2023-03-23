#pragma once

#include <iostream>
#include <list>
#include <memory>
#include <type_traits>

template <typename T, typename Allocator = std::allocator<T>>
class List {
    class Node {
       public:
        Node* next;
        T* value;
        Node* prev;

        Node() : next(this), prev(this), value(nullptr) {}
        Node(Node* prev) : next(prev->next), prev(prev), value(nullptr) {
            prev->next = this;
            next->prev = this;
        }
        Node(Node* prev, T* value) : Node(prev) { this->value = value; }

        void clear(Allocator& alloc) {
            delete value;
            alloc.deallocate(value, 1);
        }

        ~Node() {
            prev->next = next;
            next->prev = prev;
        }
    };
    Allocator alloc;
    Node* end;
    size_t sz;

   public:
    template <typename T1>
    class BaseIterator {
        friend class List;

       private:
        List::Node* node;
        BaseIterator(List::Node* ptr) : node(ptr) {}

       public:
        n, using difference_type = int;
        using value_type = T1;
        using reference = T1&;
        using pointer = T1*;
        using iterator_category = std::bidirectional_iterator_tag;

        BaseIterator& operator++() {
            node = node->next;
            return this;
        }

        BaseIterator operator++(int) {
            BaseIterator copy = *this;
            ++*this;
            return copy;
        }
        BaseIterator& operator--() {
            node = node->prev;
            return this;
        }

        BaseIterator operator--(int) {
            BaseIterator copy = *this;
            --*this;
            return copy;
        }

        bool operator==(const BaseIterator& other) const = default;

        T1& operator*() { return *reinterpret_cast<T1*>(node->value); }
        T1* operator->() { return reinterpret_cast<T1*>(node->value); }

        operator BaseIterator<const T1>() const {
            BaseIterator<const T1> res(node);
            return res;
        }
    };

    void clear() {
        while (end->next != end) {
            end->next->clear(alloc);
            delete end->next;
        }
    }

    List(Allocator alloc) : alloc(alloc), end(Node()), sz(0) {}
    List(size_t n, const T& value, Allocator alloc)
        : alloc(alloc), end(Node()), sz(0) {
        try {
            for (int i = 0; i < n; i++) {
                push_back(value);
            }
        } catch (...) {
            clear();
            throw;
        }
    }
    List(size_t n, Allocator alloc) : List(n, T(), alloc) {}
    List() : List(Allocator()) {}
    List(size_t n, const T& value) : List(n, value, Allocator()) {}
    List(size_t n, Allocator alloc) : List(n, T(), Allocator()) {}

    List(const List& other) : alloc(std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc)), end(Node()), sz(0) {
        if (&other == this)
            return;
        try {
            for (iterator it = other.begin(); it != other.end(); it++) {
                push_back(*it);
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    List& operator=(const List& other) {
        if (&other == this)
            return;
        if (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
            alloc = std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc);
        } else
            alloc = other.alloc;
        end = Node();
        sz = 0;
        try {  // TODO:copypaste
            for (iterator it = other.begin(); it != other.end(); it++) {
                push_back(*it);
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    Allocator& get_allocator() const { return alloc; }

    using iterator = BaseIterator<T>;
    using const_iterator = BaseIterator<const T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() { return iterator(end->next); }
    const_iterator begin() const { return cbegin(); }
    iterator end() { return iterator(end); }
    const_iterator end() const { return cend(); }
    const_iterator cbegin() const { return const_iterator(end->next); }
    const_iterator cend() const { return const_iterator(end); }
    reverse_iterator rbegin() { return reverse_iterator(iterator(end->prev)); }
    const_reverse_iterator rbegin() const { return crbegin(); }
    reverse_iterator rend() { return reverse_iterator(iterator(end)); }
    const_reverse_iterator rend() const { return crend(); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(const_iterator(end->prev)); }
    const_reverse_iterator crend() const { return const_reverse_iterator(const_iterator(end)); }

    size_t size() const { return sz; }

    iterator insert(const_iterator pos, const T& value) {
        Node* node = new Node(pos->node, alloc.allocate(1));
        try {
            std::allocator_traits<Allocator>::construct(alloc, node->value, value);
        } catch (...) {
            alloc.deallocate(node->value, 1);
            delete node;
            throw;
        }
        sz++;
        return iterator(node);
    }

    void push_back(const T& value) {
        insert(const_iterator(end->prev), value);
    }
    void push_front(const T& value) {
        insert(const_iterator(end), value);
    }

    iterator erase(iterator pos) {
        pos->node->clear(alloc);
        iterator res(pos->node->next);
        delete pos->node;
    }
    void pop_front() {
        erase(iterator(end->next));
    }
    void pop_back() {
        erase(iterator(end->prev));
    }

    ~List() {
        clear();
        delete end;
    }
};

template <std::size_t N>
struct StackStorage {
    char data[N];
    char* end;

    StackStorage() : data(), end(data) {}
    StackStorage& operator=(const StackStorage&) = delete;
    StackStorage(const StackStorage&) = delete;
    ~StackStorage() {}
};

template <typename T, std::size_t N>
struct StackAllocator {
    using value_type = T;

    StackStorage<N>& storage;

    StackAllocator(StackStorage<N>& storage) : storage(storage) {}

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& other)
        : storage(other.storage) {}

    /*
    ~StackAllocator(){}
    */

    T* allocate(size_t n) {
        unsigned long long delta =
            reinterpret_cast<unsigned long long>(storage.end) % alignof(T);
        if (delta != 0) {
            storage.end += alignof(T) - delta;
            if (static_cast<size_t>(storage.end - storage.data) > N) {
                storage.end -= alignof(T) - delta;
                throw std::bad_alloc();
            }
        }
        T* res = reinterpret_cast<T*>(storage.end);
        storage.end += n * sizeof(T);
        if (static_cast<size_t>(storage.end - storage.data) > N) {
            storage.end -= n * sizeof(T);
            throw std::bad_alloc();
        }
        return res;
    }

    void deallocate(T*, size_t) {}

    bool operator==(const StackAllocator& other) const {
        return &storage == &(other.storage);
    }

    template <typename U>
    // NOLINTNEXTLINE
    struct rebind {
        using other = StackAllocator<U, N>;
    };
};