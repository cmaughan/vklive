#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
#include <iostream>

namespace Zest {

// ----------------------------
// Core writer / reader
// ----------------------------

class binary_writer {
public:
    explicit binary_writer(std::ostream& os) : os_(os) {}

    bool write_bytes(const void* data, std::size_t n) {
        os_.write(static_cast<const char*>(data), static_cast<std::streamsize>(n));
        if (!os_) {
            return false;
        }
        return true;
    }

private:
    std::ostream& os_;

    template<typename T> friend void serialize(binary_writer&, const T&);
};

class binary_reader {
public:
    explicit binary_reader(std::vector<uint8_t>& v) : stream(v) {}

    bool read_bytes(void* data, std::size_t n) {
        if (offset >= stream.size())
        {
            return false;
        }
        memcpy(data, stream.data() + offset, n);
        offset += n;
        return true;
    }

private:
    std::vector<uint8_t>& stream;
    size_t offset = 0;

    template<typename T> friend void deserialize(binary_reader&, T&);
};

// ----------------------------
// POD helpers
// ----------------------------

// Fallback for "simple" types: int, bool, int64_t, float, POD structs, etc.
template<typename T>
void serialize(binary_writer& w, const T& v) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "bba::serialize: T must be trivially copyable "
                  "or have an overload of serialize()");
    w.write_bytes(&v, sizeof(T));
}

template<typename T>
void deserialize(binary_reader& r, T& v) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "bba::deserialize: T must be trivially copyable "
                  "or have an overload of deserialize()");
    r.read_bytes(&v, sizeof(T));
}

// ----------------------------
// std::string
// ----------------------------

inline void serialize(binary_writer& w, const std::string& s) {
    std::uint32_t size = static_cast<std::uint32_t>(s.size());
    serialize(w, size);
    if (!s.empty()) {
        w.write_bytes(s.data(), size);
    }
}

inline void deserialize(binary_reader& r, std::string& s) {
    std::uint32_t size = 0;
    deserialize(r, size);
    s.resize(size);
    if (size != 0) {
        r.read_bytes(s.data(), size);
    }
}

// ----------------------------
// std::vector<T> (nested ok)
// ----------------------------

template<typename T>
void serialize(binary_writer& w, const std::vector<T>& vec) {
    std::uint32_t size = static_cast<std::uint32_t>(vec.size());
    serialize(w, size);

    /*
    if constexpr (std::is_trivially_copyable_v<T>) {
        if (!vec.empty()) {
            w.write_bytes(vec.data(), sizeof(T) * vec.size());
        }
    } else {*/
        for (auto const& elem : vec) {
            serialize(w, elem); // recursive
        }
    //}
}

template<typename T>
void deserialize(binary_reader& r, std::vector<T>& vec) {
    std::uint32_t size = 0;
    deserialize(r, size);
    vec.resize(size);

    /*
    if constexpr (std::is_trivially_copyable_v<T>) {
        if (!vec.empty()) {
            r.read_bytes(vec.data(), sizeof(T) * vec.size());
        }
    } else {*/
        for (auto& elem : vec) {
            deserialize(r, elem); // recursive
        }
   // }
}

// ----------------------------
// User-defined types
// ----------------------------
//
// You implement:
//   void serialize(binary_writer& w, const YourType& v);
//   void deserialize(binary_reader& r, YourType& v);
// using the above building blocks.
//

} // namespace Zest
