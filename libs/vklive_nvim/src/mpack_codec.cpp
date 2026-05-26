#include <vklive_nvim/mpack_codec.h>

#include <algorithm>
#include <utility>
#include <variant>

#include <mpack.h>

namespace vklive_nvim
{
namespace
{

constexpr uint32_t kMaxReserveCount = 65536;
constexpr uint32_t kMaxMpackStringLen = 64u * 1024u * 1024u;

size_t estimate_value_size(const MpackValue& value)
{
    switch (value.type())
    {
    case MpackValue::Nil:
    case MpackValue::Bool:
        return 1;
    case MpackValue::Int:
    case MpackValue::UInt:
    case MpackValue::Float:
        return 9;
    case MpackValue::String:
        return 5 + value.as_str().size();
    case MpackValue::Array:
    {
        size_t total = 5;
        for (const auto& child : value.as_array())
        {
            total += estimate_value_size(child);
        }
        return total;
    }
    case MpackValue::Map:
    {
        size_t total = 5;
        for (const auto& [key, child] : value.as_map())
        {
            total += estimate_value_size(key);
            total += estimate_value_size(child);
        }
        return total;
    }
    case MpackValue::Ext:
        return 10;
    }

    return 1;
}

void write_value(mpack_writer_t* writer, const MpackValue& value)
{
    switch (value.type())
    {
    case MpackValue::Nil:
        mpack_write_nil(writer);
        break;
    case MpackValue::Bool:
        mpack_write_bool(writer, value.as_bool());
        break;
    case MpackValue::Int:
        mpack_write_i64(writer, std::get<int64_t>(value.storage));
        break;
    case MpackValue::UInt:
        mpack_write_u64(writer, std::get<uint64_t>(value.storage));
        break;
    case MpackValue::Float:
        mpack_write_double(writer, std::get<double>(value.storage));
        break;
    case MpackValue::String:
        mpack_write_str(writer, value.as_str().c_str(), static_cast<uint32_t>(value.as_str().size()));
        break;
    case MpackValue::Array:
        mpack_start_array(writer, static_cast<uint32_t>(value.as_array().size()));
        for (const auto& child : value.as_array())
        {
            write_value(writer, child);
        }
        mpack_finish_array(writer);
        break;
    case MpackValue::Map:
        mpack_start_map(writer, static_cast<uint32_t>(value.as_map().size()));
        for (const auto& [key, child] : value.as_map())
        {
            write_value(writer, key);
            write_value(writer, child);
        }
        mpack_finish_map(writer);
        break;
    case MpackValue::Ext:
        mpack_write_nil(writer);
        break;
    }
}

MpackValue read_value(mpack_reader_t* reader)
{
    MpackValue value;
    mpack_tag_t tag = mpack_read_tag(reader);

    if (mpack_reader_error(reader) != mpack_ok)
    {
        return value;
    }

    switch (mpack_tag_type(&tag))
    {
    case mpack_type_nil:
        value.storage = std::monostate{};
        break;
    case mpack_type_bool:
        value.storage = mpack_tag_bool_value(&tag);
        break;
    case mpack_type_int:
        value.storage = mpack_tag_int_value(&tag);
        break;
    case mpack_type_uint:
        value.storage = mpack_tag_uint_value(&tag);
        break;
    case mpack_type_float:
        value.storage = static_cast<double>(mpack_tag_float_value(&tag));
        break;
    case mpack_type_double:
        value.storage = mpack_tag_double_value(&tag);
        break;
    case mpack_type_str:
    {
        const uint32_t len = mpack_tag_str_length(&tag);
        if (len > kMaxMpackStringLen)
        {
            mpack_reader_flag_error(reader, mpack_error_invalid);
            return value;
        }
        std::string text(len, '\0');
        mpack_read_bytes(reader, text.data(), len);
        mpack_done_str(reader);
        value.storage = std::move(text);
        break;
    }
    case mpack_type_bin:
    {
        const uint32_t len = mpack_tag_bin_length(&tag);
        if (len > kMaxMpackStringLen)
        {
            mpack_reader_flag_error(reader, mpack_error_invalid);
            return value;
        }
        std::string text(len, '\0');
        mpack_read_bytes(reader, text.data(), len);
        mpack_done_bin(reader);
        value.storage = std::move(text);
        break;
    }
    case mpack_type_ext:
    {
        const int8_t ext_type = mpack_tag_ext_exttype(&tag);
        const uint32_t len = mpack_tag_ext_length(&tag);
        if (len <= 8)
        {
            std::string ext_data(8, '\0');
            mpack_read_bytes(reader, ext_data.data(), len);
            int64_t ext_value = 0;
            for (uint32_t i = 0; i < len; ++i)
            {
                ext_value = (ext_value << 8) | static_cast<uint8_t>(ext_data[i]);
            }
            value.storage = MpackValue::ExtValue{ ext_type, ext_value };
        }
        else
        {
            mpack_skip_bytes(reader, len);
            value.storage = std::monostate{};
        }
        mpack_done_ext(reader);
        break;
    }
    case mpack_type_array:
    {
        const uint32_t count = mpack_tag_array_count(&tag);
        MpackValue::ArrayStorage items;
        items.reserve(std::min(count, kMaxReserveCount));
        for (uint32_t i = 0; i < count; ++i)
        {
            items.push_back(read_value(reader));
            if (mpack_reader_error(reader) != mpack_ok)
            {
                return value;
            }
        }
        mpack_done_array(reader);
        value.storage = std::move(items);
        break;
    }
    case mpack_type_map:
    {
        const uint32_t count = mpack_tag_map_count(&tag);
        MpackValue::MapStorage items;
        items.reserve(std::min(count, kMaxReserveCount));
        for (uint32_t i = 0; i < count; ++i)
        {
            auto key = read_value(reader);
            auto child = read_value(reader);
            if (mpack_reader_error(reader) != mpack_ok)
            {
                return value;
            }
            items.emplace_back(std::move(key), std::move(child));
        }
        mpack_done_map(reader);
        value.storage = std::move(items);
        break;
    }
    default:
        value.storage = std::monostate{};
        break;
    }

    return value;
}

} // namespace

bool encode_mpack_value(const MpackValue& value, std::vector<char>& out)
{
    out.assign(estimate_value_size(value), '\0');

    mpack_writer_t writer;
    mpack_writer_init(&writer, out.data(), out.size());
    write_value(&writer, value);

    const size_t used = mpack_writer_buffer_used(&writer);
    const mpack_error_t error = mpack_writer_destroy(&writer);
    if (error != mpack_ok)
    {
        return false;
    }

    out.resize(used);
    return true;
}

bool decode_mpack_value(std::span<const uint8_t> bytes, MpackValue& value, size_t* consumed, bool* hard_error)
{
    if (hard_error)
    {
        *hard_error = false;
    }
    if (bytes.empty())
    {
        return false;
    }

    if (bytes[0] == 0xC1)
    {
        if (hard_error)
        {
            *hard_error = true;
        }
        return false;
    }

    mpack_reader_t reader;
    mpack_reader_init_data(&reader, reinterpret_cast<const char*>(bytes.data()), bytes.size());
    value = read_value(&reader);

    if (mpack_reader_error(&reader) != mpack_ok)
    {
        mpack_reader_destroy(&reader);
        return false;
    }

    const size_t remaining = mpack_reader_remaining(&reader, nullptr);
    mpack_reader_destroy(&reader);

    if (consumed)
    {
        *consumed = bytes.size() - remaining;
    }

    return true;
}

bool encode_rpc_request(uint32_t msgid, const std::string& method, const std::vector<MpackValue>& params, std::vector<char>& out)
{
    MpackValue payload = NvimRpc::make_array({
        NvimRpc::make_uint(0),
        NvimRpc::make_uint(msgid),
        NvimRpc::make_str(method),
        NvimRpc::make_array(params),
    });
    return encode_mpack_value(payload, out);
}

bool encode_rpc_notification(const std::string& method, const std::vector<MpackValue>& params, std::vector<char>& out)
{
    MpackValue payload = NvimRpc::make_array({
        NvimRpc::make_uint(2),
        NvimRpc::make_str(method),
        NvimRpc::make_array(params),
    });
    return encode_mpack_value(payload, out);
}

bool encode_rpc_response(uint32_t msgid, const MpackValue& error, const MpackValue& result, std::vector<char>& out)
{
    MpackValue payload = NvimRpc::make_array({
        NvimRpc::make_uint(1),
        NvimRpc::make_uint(msgid),
        error,
        result,
    });
    return encode_mpack_value(payload, out);
}

} // namespace vklive_nvim
