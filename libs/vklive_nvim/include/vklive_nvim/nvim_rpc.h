#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace vklive_nvim
{

struct MpackValue
{
    enum Type
    {
        Nil,
        Bool,
        Int,
        UInt,
        Float,
        String,
        Array,
        Map,
        Ext
    };

    struct ExtValue
    {
        int8_t type = 0;
        int64_t data = 0;
    };

    using ArrayStorage = std::vector<MpackValue>;
    using MapStorage = std::vector<std::pair<MpackValue, MpackValue>>;
    using Storage = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, ArrayStorage, MapStorage, ExtValue>;

    Storage storage = std::monostate{};

    Type type() const;
    bool is_nil() const;
    int64_t as_int() const;
    const std::string& as_str() const;
    bool as_bool() const;
    const ArrayStorage& as_array() const;
    const MapStorage& as_map() const;
    const ExtValue& as_ext() const;
};

class NvimRpc
{
public:
    static MpackValue make_int(int64_t v);
    static MpackValue make_uint(uint64_t v);
    static MpackValue make_str(const std::string& v);
    static MpackValue make_bool(bool v);
    static MpackValue make_array(std::vector<MpackValue> v);
    static MpackValue make_map(std::vector<std::pair<MpackValue, MpackValue>> v);
    static MpackValue make_nil();
};

} // namespace vklive_nvim
