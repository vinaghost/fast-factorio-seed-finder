#pragma once

#include <exception>
#include <stdexcept>
#include <array>
#include <cstdint>
#include <cmath>
#include <queue>
#include <vector>
#include <functional>
#include <mutex>
#include <concepts>
#include <string>
#include <algorithm>
#include <cassert>

enum ResourceType {
    IRON,
    COPPER,
    COAL,
    STONE,
    OIL,
    NB_RESOURCE_TYPE
};

enum PatchType {
    STARTER,
    REGULAR,
    BITER,
    NB_PATCH_TYPE
};

constexpr ResourceType& operator++(ResourceType& type) {
    assert(type < NB_RESOURCE_TYPE);
    return type = (ResourceType)(1 + ((int)type));
}

constexpr ResourceType operator++(ResourceType& type, int) {
    ResourceType tmp(type);
    ++type;
    return tmp;
}

enum ElevationType {
    ELEVATION_2_0,
    ELEVATION_1_1,
    ELEVATION_ISLAND,
    NB_ELEVATION_TYPE   
};

constexpr ElevationType& operator++(ElevationType& type) {
    assert(type < NB_ELEVATION_TYPE);
    return type = (ElevationType)(1 + ((int)type));
}

constexpr ElevationType operator++(ElevationType& type, int) {
    ElevationType tmp(type);
    ++type;
    return tmp;
}

struct MapGenSettings {
    std::array<float, NB_RESOURCE_TYPE> frequencies{ 1.f, 1.f, 1.f, 1.f, 1.f };
    std::array<float, NB_RESOURCE_TYPE> sizes{ 1.f, 1.f, 1.f, 1.f, 1.f };
    std::array<float, NB_RESOURCE_TYPE> richness{ 1.f, 1.f, 1.f, 1.f, 1.f };

    ElevationType elevation_type = ELEVATION_2_0;
    float water_scale = 1.f;    // inverse of control:water:frequency
    float water_coverage = 1.f; // same as control:water:size

    float biter_frequency = 1.f;
    float biter_size = 1.f;
};

constexpr std::string thousands_separator(std::integral auto n) {
    std::string out;
    std::string num = std::to_string(n);

    int count = 0;

    for (int i = (int)num.size() - 1; i >= 0; i--) {
        count++;
        out.push_back(num[i]);

        if (count % 3 == 0) {
            out.push_back(',');
            count = 0;
        }
    }

    std::reverse(out.begin(), out.end());

    if (out.size() % 4 == 0) {
        out.erase(out.begin());
    }

    return out;
}

// A fixed size container that keeps the top-K largest elements
template <typename T, typename Compare = std::greater<T>>
class TopK {
public:
    TopK(size_t max_size = 0) : _max_size(max_size) {}

    TopK(TopK&& other) noexcept : _heap(std::move(other._heap)), _max_size(other._max_size) {}
    TopK& operator=(TopK&& other) noexcept {
        _heap = std::move(other._heap);
        _max_size = other._max_size;
		return *this;
	}

    void insert(const T& value) {
        std::scoped_lock lock(_mutex);

        if (_heap.size() < _max_size) {
            _heap.push(value);
        } else if (Compare()(value, _heap.top())) {
            _heap.pop();
            _heap.push(value);
        }
    }
    
    T top() const {
        std::scoped_lock lock(_mutex);

        return _heap.top();
    }
    
    void pop() {
        std::scoped_lock lock(_mutex);

        _heap.pop();
    }

    T get_pop() {
        std::scoped_lock lock(_mutex);

        T value = _heap.top();
        _heap.pop();
        return value;
    }

    bool empty() const {
        std::scoped_lock lock(_mutex);

        return _heap.empty();
    }

    void clear() {
        std::scoped_lock lock(_mutex);

        while (!_heap.empty()) {
            _heap.pop();
        }
    }

    size_t size() const {
        std::scoped_lock lock(_mutex);

        return _heap.size();
    }

    size_t max_size() const {
        std::scoped_lock lock(_mutex);

        return _max_size;
    }

    void set_max_size(size_t new_max_size) {
        std::scoped_lock lock(_mutex);

        _max_size = new_max_size;
        while (_heap.size() >= new_max_size) {
            _heap.pop();
        }
    }

private:
    size_t _max_size;
    std::priority_queue<T, std::vector<T>, Compare> _heap;
    mutable std::mutex _mutex;
};

enum Direction {
    EAST,
    SOUTH,
    WEST,
    NORTH,
    NB_DIRECTIONS
};

constexpr Direction operator-(Direction direction) {
    return (Direction)(((int)direction + 2) % NB_DIRECTIONS);
}

constexpr Direction& operator++(Direction& direction) {
    return direction = (Direction)(((int)direction + 1) % NB_DIRECTIONS);
}

constexpr Direction operator++(Direction& direction, int) {
    Direction tmp(direction);
    ++direction;
    return tmp;
}

constexpr Direction rotated_direction(Direction& a, Direction& b) {
    return (Direction)(((int)a + (int)b) % NB_DIRECTIONS);
}

constexpr Direction& operator--(Direction& direction) {
    return direction = (Direction)(((int)direction - 1 + (int)NB_DIRECTIONS) % NB_DIRECTIONS);
}

constexpr Direction operator--(Direction& direction, int) {
    Direction tmp(direction);
    --direction;
    return tmp;
}

constexpr Direction rotated_direction_counter_clockwise(Direction& a, Direction& b) {
    return (Direction)(((int)a - (int)b + (int)NB_DIRECTIONS) % NB_DIRECTIONS);
}

// Used to iterate over directions
constexpr Direction next_direction(Direction direction) {
    return (Direction)(((int)direction + 1));
}

template<typename T> requires (std::is_arithmetic_v<T>)
struct Position {
    T x;
    T y;

    constexpr Position() : x(0), y(0) {}
    constexpr Position(T x_, T y_) : x(x_), y(y_) {}
    constexpr Position(Direction direction) {
        switch (direction) {
            case EAST: x = 1; y = 0; break;
            case SOUTH: x = 0; y = 1; break;
            case WEST: x = -1; y = 0; break;
            case NORTH: x = 0; y = -1; break;
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    template<typename K> requires (std::is_arithmetic_v<K>)
    constexpr operator Position<K>() const {
        return Position<K>((K)x, (K)y);
    }

    constexpr Position operator-() const {
        return { -x, -y };
    }

    constexpr Position& operator+=(Position other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    friend constexpr Position operator+(Position a, Position b) {
        a += b;
        return a;
    }

    constexpr Position& operator-=(Position other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    friend constexpr Position operator-(Position a, Position b) {
        a -= b;
        return a;
    }

    constexpr Position& operator*=(T other) {
        x *= other;
        y *= other;
        return *this;
    }

    friend constexpr Position operator*(Position a, T b) {
        a *= b;
        return a;
    }

    constexpr Position& operator/=(T other) {
        x /= other;
        y /= other;
        return *this;
    }

    friend constexpr Position operator/(Position a, T b) {
        a /= b;
        return a;
    }

    friend constexpr bool operator==(Position a, Position b) {
        return a.x == b.x && a.y == b.y;
    }

    friend constexpr bool operator!=(Position a, Position b) {
        return !(a == b);
    }

    constexpr T length_2() const {
        return x*x + y*y;
    }

    static constexpr T distance_2(Position a, Position b) {
        return (a - b).length_2();
    }

    auto length() const {
        return std::sqrt((float)x*x + (float)y*y);
    }

    static constexpr auto distance(Position a, Position b) {
        return (a - b).length();
    }

    inline auto angle() const {
        return std::atan2(y, x);
    }

    constexpr auto manhattan_length() const {
        return std::abs(x) + std::abs(y);
    }

    static constexpr auto manhattan_distance(Position a, Position b) {
        return (a - b).manhattan_length();
    }

    constexpr auto chebyshev_length() const {
        return std::max(std::abs(x), std::abs(y));
    }

    static constexpr auto chebyshev_distance(Position a, Position b) {
        return (a - b).chebyshev_length();
    }

    // direction EAST is means no rotation, south means 90 degrees clockwise
    constexpr Position rotated(Direction direction) const {
        switch (direction) {
            case EAST: return *this;
            case SOUTH: return { -y, x };
            case WEST: return { -x, -y };
            case NORTH: return { y, -x };
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    // direction EAST is means no rotation, south means 90 degrees counter clockwise
    constexpr Position rotated_counter_clockwise(Direction direction) const {
        switch (direction) {
            case EAST: return *this;
            case SOUTH: return { y, -x };
            case WEST: return { -x, -y };
            case NORTH: return { -y, x };
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    constexpr Position flipped(Direction direction) const {
        switch (direction) {
            case EAST:
            case WEST: return Position(x, -y);
            case SOUTH:
            case NORTH: return Position(-x, y);
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    constexpr Position flipped(Direction direction, bool is_flipped) const {
        return is_flipped ? flipped(direction) : *this;
    }

    constexpr Position abs() const {
        return { std::abs(x), std::abs(y) };
    }

    static constexpr Position min(Position a, Position b) {
        return { std::min(a.x, b.x), std::min(a.y, b.y) };
    }

    static constexpr Position max(Position a, Position b) {
        return { std::max(a.x, b.x), std::max(a.y, b.y) };
    }
};

using PositionI32 = Position<int32_t>;
using PositionF32 = Position<float>;

template<typename T> requires (std::is_arithmetic_v<T>)
struct Box {
    Position<T> left_top;
    Position<T> right_bottom;

    constexpr Box() = default;
    constexpr Box(Position<T> left_top_, Position<T> right_bottom_) : left_top(left_top_), right_bottom(right_bottom_) {}
    constexpr Box(T x1, T y1, T x2, T y2) : left_top(x1, y1), right_bottom(x2, y2) {}
    constexpr Box(Position<T> center, T radius) {
        Position<T> shift(radius, radius);
        left_top = center - shift;
        right_bottom = center + shift;
    }

    template<typename K> requires (std::is_arithmetic_v<K>)
    constexpr operator Box<K>() const {
        return Box<K>(Position<K>(left_top), Position<K>(right_bottom));
    }

    constexpr Position<T> get_left_bottom() const {
        return { left_top.x, right_bottom.y };
    }

    constexpr Position<T> get_right_top() const {
        return { right_bottom.x, left_top.y };
    }

    constexpr Box &operator+=(Position<T> pos) {
        left_top += pos;
        right_bottom += pos;
        return *this;
    }

    friend constexpr Box operator+(Box box, Position<T> pos) {
        box += pos;
        return box;
    }

    constexpr Box &operator-=(Position<T> pos) {
        left_top -= pos;
        right_bottom -= pos;
        return *this;
    }

    friend constexpr Box operator-(Box box, Position<T> pos) {
        box -= pos;
        return box;
    }

    constexpr Position<T> center() const {
        return (left_top + right_bottom) / 2;
    }

    constexpr T area() const {
        return (T)std::abs((left_top.x - right_bottom.x) * (left_top.y - right_bottom.y));
    }

    constexpr Position<T> size() const {
        return Position<T>(right_bottom.x - left_top.x, right_bottom.y - left_top.y);
    }

    constexpr Box rotated(Direction direction) const {
        const T x1 = left_top.x, y1 = left_top.y, x2 = right_bottom.x, y2 = right_bottom.y;
        switch (direction) {
            case Direction::EAST: return *this;
            case Direction::SOUTH: return Box(-y2, x1, -y1, x2);
            case Direction::WEST: return Box(-x2, -y2, -x1, -y1);
            case Direction::NORTH: return Box(y1, -x2, y2, -x1);
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    constexpr Box rotated_counter_clockwise(Direction direction) const {
        const T x1 = left_top.x, y1 = left_top.y, x2 = right_bottom.x, y2 = right_bottom.y;
        switch (direction) {
            case Direction::EAST: return *this;
            case Direction::SOUTH: return Box(y1, -x2, y2, -x1);
            case Direction::WEST: return Box(-x2, -y2, -x1, -y1);
            case Direction::NORTH: return Box(-y2, x1, -y1, x2);
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    constexpr bool contains(Position<T> pos) const {
        return pos.x > left_top.x && pos.y > left_top.y && pos.x < right_bottom.x && pos.y < right_bottom.y;
    }

    constexpr Box extended(T x, T y) const {
        Position<T> shift(x, y);
        return Box(left_top - shift, right_bottom + shift);
    }

    constexpr Box extended(Position<T> size) const {
        return extended(size.x, size.y);
    }

    static constexpr Box combined(Box a, Box b) {
        return Box(
            Position<T>::min(a.left_top, b.left_top),
            Position<T>::max(a.right_bottom, b.right_bottom)
        );
    }

    // returns the axial symmetric of the box along the direction
    constexpr Box flipped(Direction direction) const {
        switch (direction) {
            case EAST:
            case WEST: return Box(left_top.x, -right_bottom.y, right_bottom.x, -left_top.y);
            case SOUTH:
            case NORTH: return Box(-right_bottom.x, left_top.y, -left_top.x, right_bottom.y);
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    constexpr Box flipped(Direction direction, bool is_flipped) const {
        return is_flipped ? flipped(direction) : *this;
    }

    constexpr T side_position(Direction direction) const {
        switch (direction) {
            case EAST: return right_bottom.x;
            case SOUTH: return right_bottom.y;
            case WEST: return left_top.x;
            case NORTH: return left_top.y;
            default: throw std::runtime_error("Invalid direction value.");
        }
    }

    inline T distance_2(Position<T> point) const {
        auto x = std::max({left_top.x - point.x, (T)0, point.x - right_bottom.x});
        auto y = std::max({left_top.y - point.y, (T)0, point.y - right_bottom.y});

        return x*x + y*y;
    }

    inline float distance(Position<T> point) const {
        return std::sqrt((float)distance_2(point));
    }
};

using BoxI32 = Box<int32_t>;
using BoxF32 = Box<float>;
