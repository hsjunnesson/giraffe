#pragma once

#include <array.h>
#include <cassert>

// Deletes the copy constructor, the copy assignment operator, the move constructor, and the move assignment operator.
#define DELETE_COPY_AND_MOVE(T)       \
    T(const T &) = delete;            \
    T &operator=(const T &) = delete; \
    T(T &&) = delete;                 \
    T &operator=(T &&) = delete;

namespace foundation {

// Swaps the element at index to the end and pops it.
// This will change the order of the elements in the array.
template <typename T>
void swap_pop(Array<T> &a, uint32_t index) {
    assert(array::size(a) > index);

    std::swap(a[index], array::back(a));
    array::pop_back(a);
}

// Shifts the element at index to the end and pops it.
// This will retain the order of the remaining elements.
// This is at worst O(n) complexity.
template <typename T>
void shift_pop(Array<T> &a, uint32_t index) {
    uint32_t size = array::size(a);
    assert(size > index);

    for (uint32_t i = index; i < size - 1; ++i) {
        std::swap(a[i], a[i + 1]);
    }

    array::pop_back(a);
}

} // namespace foundation
