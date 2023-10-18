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

inline bool ray_circle_intersection(const glm::vec2 ray_origin, const glm::vec2 ray_direction, const glm::vec2 circle_center, float circle_radius, glm::vec2 &intersection) {
    glm::vec2 ray_dir = glm::normalize(ray_direction);

    // Check if origin is inside the circle
    if (glm::length(ray_origin - circle_center) <= circle_radius) {
        intersection = ray_origin;
        return true;
    }

    // Compute the nearest point on the ray to the circle's center
    float t = glm::dot(circle_center - ray_origin, ray_dir);

    if (t < 0.0f) {
        return false;
    }

    glm::vec2 P = ray_origin + t * ray_dir;

    // If the nearest point is inside the circle, calculate intersection
    if (glm::length(P - circle_center) <= circle_radius) {
        // Distance from P to circle boundary along the ray
        float h = sqrt(circle_radius * circle_radius - glm::length(P - circle_center) * glm::length(P - circle_center));
        intersection = P - h * ray_dir;
        return true;
    }

    return false;
}

inline bool ray_line_intersection(const glm::vec2 ray_origin, const glm::vec2 ray_direction, const glm::vec2 p1, const glm::vec2 p2, glm::vec2 &intersection) {
    const glm::vec2 line_dir = p2 - p1;
    float det = ray_direction.x * (-line_dir.y) - ray_direction.y * (-line_dir.x);

    // parallel
    if (fabs(det) < 1e-6) {
        return false;
    }

    float t = ((p1.x - ray_origin.x) * (-line_dir.y) + (p1.y - ray_origin.y) * line_dir.x) / det;
    float u = -((ray_origin.x - p1.x) * (-ray_direction.y) + (ray_origin.y - p1.y) * ray_direction.x) / det;

    if (t >= 0 && u >= 0 && u <= 1) {
        intersection = ray_origin + t * ray_direction;
        return true;
    }

    return false;
}

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
