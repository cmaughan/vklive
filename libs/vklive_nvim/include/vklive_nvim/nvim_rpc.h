#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
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

enum class RpcErrorKind
{
    None,
    IoError,
    RpcError,
};

struct RpcError
{
    RpcErrorKind kind = RpcErrorKind::None;
    std::string message;
};

class RpcResult
{
public:
    RpcResult() = default;

    static RpcResult ok(MpackValue value);
    static RpcResult err(RpcErrorKind kind, std::string message);

    explicit operator bool() const
    {
        return m_ok;
    }

    const MpackValue& value() const
    {
        return m_value;
    }

    const RpcError& error() const
    {
        return m_error;
    }

private:
    bool m_ok = false;
    MpackValue m_value;
    RpcError m_error;
};

struct RpcNotification
{
    std::string method;
    std::vector<MpackValue> params;
};

class IRpcTransport
{
public:
    virtual ~IRpcTransport() = default;
    virtual bool write(const std::uint8_t* data, std::size_t len) = 0;
    virtual int read(std::uint8_t* buffer, std::size_t max_len) = 0;
    virtual bool is_running() const = 0;
};

class IRpcChannel
{
public:
    virtual ~IRpcChannel() = default;
    virtual RpcResult request(const std::string& method, const std::vector<MpackValue>& params) = 0;
    virtual void notify(const std::string& method, const std::vector<MpackValue>& params) = 0;
};

struct RpcCallbacks
{
    std::function<void()> on_notification_available;
    std::function<MpackValue(const std::string& method, const std::vector<MpackValue>& params)> on_request;
};

class NvimRpc final : public IRpcChannel
{
public:
    NvimRpc();
    ~NvimRpc() override;

    NvimRpc(const NvimRpc&) = delete;
    NvimRpc& operator=(const NvimRpc&) = delete;

    bool initialize(IRpcTransport& transport, RpcCallbacks callbacks = {});
    void shutdown();
    void close();

    RpcResult request(const std::string& method, const std::vector<MpackValue>& params) override;
    void notify(const std::string& method, const std::vector<MpackValue>& params) override;

    std::vector<RpcNotification> drain_notifications();
    std::size_t notification_queue_depth() const;
    bool connection_failed() const;

    static MpackValue make_int(int64_t v);
    static MpackValue make_uint(uint64_t v);
    static MpackValue make_str(const std::string& v);
    static MpackValue make_bool(bool v);
    static MpackValue make_array(std::vector<MpackValue> v);
    static MpackValue make_map(std::vector<std::pair<MpackValue, MpackValue>> v);
    static MpackValue make_nil();

private:
    void reader_thread_func();
    void dispatch_rpc_message(const MpackValue& msg);
    void dispatch_rpc_response(const std::vector<MpackValue>& msg_array);
    void dispatch_rpc_request(const std::vector<MpackValue>& msg_array);
    void dispatch_rpc_notification(const std::vector<MpackValue>& msg_array);
    void reply_to_request(std::uint32_t msgid, const MpackValue& error, const MpackValue& result);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    RpcCallbacks m_callbacks;
};

} // namespace vklive_nvim
