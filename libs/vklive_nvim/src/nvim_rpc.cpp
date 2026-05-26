#include <vklive_nvim/nvim_rpc.h>

namespace vklive_nvim
{

MpackValue::Type MpackValue::type() const
{
    if (std::holds_alternative<std::monostate>(storage))
    {
        return Nil;
    }
    if (std::holds_alternative<bool>(storage))
    {
        return Bool;
    }
    if (std::holds_alternative<int64_t>(storage))
    {
        return Int;
    }
    if (std::holds_alternative<uint64_t>(storage))
    {
        return UInt;
    }
    if (std::holds_alternative<double>(storage))
    {
        return Float;
    }
    if (std::holds_alternative<std::string>(storage))
    {
        return String;
    }
    if (std::holds_alternative<ArrayStorage>(storage))
    {
        return Array;
    }
    if (std::holds_alternative<MapStorage>(storage))
    {
        return Map;
    }
    return Ext;
}

bool MpackValue::is_nil() const
{
    return std::holds_alternative<std::monostate>(storage);
}

int64_t MpackValue::as_int() const
{
    if (auto value = std::get_if<int64_t>(&storage))
    {
        return *value;
    }
    if (auto value = std::get_if<uint64_t>(&storage))
    {
        if (*value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        {
            throw std::range_error("uint64 value exceeds int64 range in MpackValue::as_int()");
        }
        return static_cast<int64_t>(*value);
    }
    throw std::bad_variant_access();
}

const std::string& MpackValue::as_str() const
{
    return std::get<std::string>(storage);
}

bool MpackValue::as_bool() const
{
    return std::get<bool>(storage);
}

const MpackValue::ArrayStorage& MpackValue::as_array() const
{
    return std::get<ArrayStorage>(storage);
}

const MpackValue::MapStorage& MpackValue::as_map() const
{
    return std::get<MapStorage>(storage);
}

const MpackValue::ExtValue& MpackValue::as_ext() const
{
    return std::get<ExtValue>(storage);
}

MpackValue NvimRpc::make_int(int64_t v)
{
    MpackValue value;
    value.storage = v;
    return value;
}

MpackValue NvimRpc::make_uint(uint64_t v)
{
    MpackValue value;
    value.storage = v;
    return value;
}

MpackValue NvimRpc::make_str(const std::string& v)
{
    MpackValue value;
    value.storage = v;
    return value;
}

MpackValue NvimRpc::make_bool(bool v)
{
    MpackValue value;
    value.storage = v;
    return value;
}

MpackValue NvimRpc::make_array(std::vector<MpackValue> v)
{
    MpackValue value;
    value.storage = std::move(v);
    return value;
}

MpackValue NvimRpc::make_map(std::vector<std::pair<MpackValue, MpackValue>> v)
{
    MpackValue value;
    value.storage = std::move(v);
    return value;
}

MpackValue NvimRpc::make_nil()
{
    return MpackValue{};
}

} // namespace vklive_nvim
