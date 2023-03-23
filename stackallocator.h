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

    void push_back(const T& value) {
        Node* node = new Node(end->prev, alloc.allocate(1));
        try {
            new (node->value) T(value);
        } catch (...) {
            alloc.deallocate(node->value, 1);
            delete node;
            throw;
        }
    }

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

    List(Allocator alloc) : alloc(alloc), end(Node()), sz(0) {}
    List(size_t n, const T& value, Allocator alloc)
        : alloc(alloc), end(Node()), sz(0) {
        for (int i = 0; i < n; i++) {
            push_back(value);
        }  // TODO
    }
    List(size_t n, Allocator alloc) : List(n, T(), alloc) {}
    List() : List(Allocator()) {}
    List(size_t n, const T& value) : List(n, value, Allocator()) {}
    List(size_t n, Allocator alloc) : List(n, T(), Allocator()) {}

    Allocator& get_allocator() { return alloc; }

    using iterator = BaseIterator<T>;
    using const_iterator = BaseIterator<const T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
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

/*
Конструкторы: без параметров; от одного числа; от числа и const T&; от одного
аллокатора; от числа и аллокатора; от числа, const T& и аллокатора. Если
создается пустой лист, то не должно быть никаких выделений динамической памяти,
независимо от того, на каком аллокаторе построен этот лист. Метод
get_allocator(), возвращающий объект аллокатора, используемый в листе на данный
момент; Конструктор копирования, деструктор, копирующий оператор присваивания;
Метод size(), работающий за O(1);
Методы push_back, push_front, pop_back, pop_front;
Двунаправленные итераторы, удовлетворяющие требованиям
https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator. Также
поддержите константные и reverse-итераторы; Методы begin, end, cbegin, cend,
rbegin, rend, crbegin, crend; Методы insert(iterator, const T&), а также
erase(iterator) - для удаления и добавления одиночных элементов в список. Если
аллокатор является структурой без каких-либо полей, то такой аллокатор не должен
увеличивать sizeof объекта листа. То есть тривиальный аллокатор как поле листа
должен занимать 0 байт.

Все методы листа должны быть строго безопасны относительно исключений. Это
означает, что при выбросе исключения из любого метода класса T во время
произвольной операции X над листом лист должен вернуться в состояние, которое
было до начала выполнения X, не допустив UB и утечек памяти, и пробросить
исключение наверх в вызывающую функцию. Можно считать, что конструкторы и
операторы присваивания у аллокаторов исключений никогда не кидают (это является
частью требований к Allocator).
*/