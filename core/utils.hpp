#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <queue>
#include <vector>
#include <functional>
#include <mutex>
#include <concepts>

enum ResourceType {
    IRON,
    COPPER,
    COAL,
    STONE,
    NB_RESOURCE_TYPE
};

enum PatchType {
    STARTER,
    REGULAR,
    NB_PATCH_TYPE
};

constexpr ResourceType& operator++(ResourceType& type) {
    return type = (ResourceType)(1 + ((int)type));
}

constexpr ResourceType operator++(ResourceType& type, int) {
    ResourceType tmp(type);
    ++type;
    return tmp;
}

struct MapGenSettings {
    std::array<float, NB_RESOURCE_TYPE> frequencies{ 1.f, 1.f, 1.f, 1.f };
    std::array<float, NB_RESOURCE_TYPE> sizes{ 1.f, 1.f, 1.f, 1.f };
    std::array<float, NB_RESOURCE_TYPE> richness{ 1.f, 1.f, 1.f, 1.f };

    float water_scale = 1.f;
    float water_coverage = 1.f;
};

constexpr std::string thousands_separator(std::integral auto n) {
    std::string out;
    std::string num = std::to_string(n);

    int count = 0;

    for (int i = num.size() - 1; i >= 0; i--) {
        count++;
        out.push_back(num[i]);

        if (count % 3 == 0) {
            out.push_back(',');
            count = 0;
        }
    }

    reverse(out.begin(), out.end());

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
        } else if (value > _heap.top()) {
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