#pragma once

#include <array.h>
#include <cassert>
#include <glm/glm.hpp>

// Deletes the copy constructor, the copy assignment operator, the move constructor, and the move assignment operator.
#define DELETE_COPY_AND_MOVE(T)       \
    T(const T &) = delete;            \
    T &operator=(const T &) = delete; \
    T(T &&) = delete;                 \
    T &operator=(T &&) = delete;

inline bool circles_overlap(const glm::vec2 p1, const glm::vec2 p2, float r1, float r2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float dist_sqrd = dx * dx + dy * dy;
    float sqrd_radii = (r1 + r2) * (r1 + r2);
    return dist_sqrd <= sqrd_radii;
}

inline glm::vec2 truncate(const glm::vec2 &vector, float max_length) {
    float length = glm::length(vector);
    if (length > max_length && length > 0) {
        return vector / length * max_length;
    } else {
        return vector;
    }
}

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
