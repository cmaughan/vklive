#include <vklive_nvim/mpack_codec.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

const vklive_nvim::MpackValue::MapStorage& as_map(const vklive_nvim::MpackValue& value)
{
    return value.as_map();
}

} // namespace

int main()
{
    using namespace vklive_nvim;

    MpackValue original = NvimRpc::make_map({
        { NvimRpc::make_str("ok"), NvimRpc::make_bool(true) },
        { NvimRpc::make_str("count"), NvimRpc::make_int(42) },
        { NvimRpc::make_str("items"), NvimRpc::make_array({
                                          NvimRpc::make_nil(),
                                          NvimRpc::make_str("hello"),
                                          NvimRpc::make_uint(7),
                                      }) },
    });

    std::vector<char> encoded;
    assert(encode_mpack_value(original, encoded));

    MpackValue decoded;
    size_t consumed = 0;
    assert(decode_mpack_value({ reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size() }, decoded, &consumed));
    assert(consumed == encoded.size());
    assert(decoded.type() == MpackValue::Map);
    assert(as_map(decoded).size() == 3);
    assert(as_map(decoded)[0].first.as_str() == std::string("ok"));
    assert(as_map(decoded)[0].second.as_bool());
    assert(as_map(decoded)[1].second.as_int() == static_cast<int64_t>(42));
    assert(as_map(decoded)[2].second.as_array()[1].as_str() == std::string("hello"));

    assert(encode_rpc_request(99, "nvim_ui_attach", {
                                                     NvimRpc::make_int(80),
                                                     NvimRpc::make_int(24),
                                                 },
        encoded));
    assert(decode_mpack_value({ reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size() }, decoded));
    assert(decoded.type() == MpackValue::Array);
    assert(decoded.as_array().size() == 4);
    assert(decoded.as_array()[0].as_int() == static_cast<int64_t>(0));
    assert(decoded.as_array()[1].as_int() == static_cast<int64_t>(99));
    assert(decoded.as_array()[2].as_str() == std::string("nvim_ui_attach"));
    assert(decoded.as_array()[3].as_array()[0].as_int() == static_cast<int64_t>(80));

    const uint8_t bad[] = { 0xC1, 0x00, 0x00 };
    bool hard_error = false;
    assert(!decode_mpack_value({ bad, sizeof(bad) }, decoded, &consumed, &hard_error));
    assert(hard_error);
}
