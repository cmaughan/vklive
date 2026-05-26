#include <vklive_nvim/nvim_rpc.h>

#include <vklive_nvim/mpack_codec.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <utility>

namespace vklive_nvim
{
namespace
{

constexpr auto kRpcRequestTimeout = std::chrono::seconds(5);
constexpr std::size_t kMaxNotificationQueueDepth = 4096;

struct RpcResponse
{
    std::uint32_t msgid = 0;
    MpackValue error;
    MpackValue result;
};

std::string stringify_rpc_error(const MpackValue& error)
{
    if (error.is_nil())
    {
        return "unknown rpc error";
    }

    if (error.type() == MpackValue::String)
    {
        return error.as_str();
    }

    if (error.type() == MpackValue::Array)
    {
        const auto& items = error.as_array();
        if (items.size() >= 2 && items[1].type() == MpackValue::String)
        {
            return items[1].as_str();
        }
        if (!items.empty() && items[0].type() == MpackValue::String)
        {
            return items[0].as_str();
        }
    }

    return "rpc error";
}

} // namespace

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

RpcResult RpcResult::ok(MpackValue value)
{
    RpcResult result;
    result.m_ok = true;
    result.m_value = std::move(value);
    return result;
}

RpcResult RpcResult::err(RpcErrorKind kind, std::string message)
{
    RpcResult result;
    result.m_ok = false;
    result.m_error = { kind, std::move(message) };
    return result;
}

struct NvimRpc::Impl
{
    IRpcTransport* transport = nullptr;
    std::thread reader_thread;
    std::atomic<bool> running{ false };
    std::atomic<bool> read_failed{ false };

    std::mutex write_mutex;

    std::mutex notification_mutex;
    std::deque<RpcNotification> notifications;

    std::mutex response_mutex;
    std::condition_variable response_cv;
    std::unordered_map<std::uint32_t, RpcResponse> responses;

    std::atomic<std::uint32_t> next_msgid{ 1 };
    std::vector<std::uint8_t> read_buffer;
};

NvimRpc::NvimRpc()
    : m_impl(std::make_unique<Impl>())
{
}

NvimRpc::~NvimRpc()
{
    if (m_impl && m_impl->reader_thread.joinable())
    {
        shutdown();
    }
}

bool NvimRpc::initialize(IRpcTransport& transport, RpcCallbacks callbacks)
{
    if (m_impl->running)
    {
        return false;
    }

    m_impl->transport = &transport;
    m_impl->read_failed = false;
    m_impl->read_buffer.assign(256 * 1024, 0);
    m_callbacks = std::move(callbacks);
    m_impl->running = true;
    m_impl->reader_thread = std::thread(&NvimRpc::reader_thread_func, this);
    return true;
}

void NvimRpc::close()
{
    m_impl->running = false;
    m_impl->response_cv.notify_all();
}

void NvimRpc::shutdown()
{
    close();
    if (m_impl->transport)
    {
        m_impl->transport->interrupt_read();
    }
    if (m_impl->reader_thread.joinable())
    {
        m_impl->reader_thread.join();
    }
}

RpcResult NvimRpc::request(const std::string& method, const std::vector<MpackValue>& params)
{
    if (!m_impl->transport || !m_impl->running)
    {
        return RpcResult::err(RpcErrorKind::IoError, "rpc transport not running");
    }

    const std::uint32_t msgid = m_impl->next_msgid++;

    std::vector<char> encoded;
    if (!encode_rpc_request(msgid, method, params, encoded))
    {
        return RpcResult::err(RpcErrorKind::IoError, "failed to encode rpc request: " + method);
    }

    {
        std::lock_guard lock(m_impl->write_mutex);
        if (!m_impl->running)
        {
            return RpcResult::err(RpcErrorKind::IoError, "rpc transport closed");
        }

        if (!m_impl->transport->write(reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size()))
        {
            m_impl->read_failed = true;
            m_impl->response_cv.notify_all();
            return RpcResult::err(RpcErrorKind::IoError, "rpc write failed: " + method);
        }
    }

    std::unique_lock lock(m_impl->response_mutex);
    const bool ready = m_impl->response_cv.wait_for(lock, kRpcRequestTimeout, [this, msgid]() {
        return m_impl->responses.contains(msgid) || !m_impl->running || m_impl->read_failed
            || (m_impl->transport && !m_impl->transport->is_running());
    });

    if (!ready || !m_impl->responses.contains(msgid))
    {
        m_impl->responses.erase(msgid);
        return RpcResult::err(RpcErrorKind::IoError, "rpc request timed out or aborted: " + method);
    }

    auto response = std::move(m_impl->responses[msgid]);
    m_impl->responses.erase(msgid);

    if (!response.error.is_nil())
    {
        return RpcResult::err(RpcErrorKind::RpcError, stringify_rpc_error(response.error));
    }

    return RpcResult::ok(std::move(response.result));
}

void NvimRpc::notify(const std::string& method, const std::vector<MpackValue>& params)
{
    if (!m_impl->transport || !m_impl->running)
    {
        return;
    }

    std::vector<char> encoded;
    if (!encode_rpc_notification(method, params, encoded))
    {
        return;
    }

    std::lock_guard lock(m_impl->write_mutex);
    if (!m_impl->running)
    {
        return;
    }

    if (!m_impl->transport->write(reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size()))
    {
        m_impl->read_failed = true;
        m_impl->response_cv.notify_all();
        if (m_callbacks.on_notification_available)
        {
            m_callbacks.on_notification_available();
        }
    }
}

std::vector<RpcNotification> NvimRpc::drain_notifications()
{
    std::lock_guard lock(m_impl->notification_mutex);
    std::vector<RpcNotification> result(
        std::make_move_iterator(m_impl->notifications.begin()),
        std::make_move_iterator(m_impl->notifications.end()));
    m_impl->notifications.clear();
    return result;
}

std::size_t NvimRpc::notification_queue_depth() const
{
    std::lock_guard lock(m_impl->notification_mutex);
    return m_impl->notifications.size();
}

bool NvimRpc::connection_failed() const
{
    return m_impl->read_failed;
}

void NvimRpc::dispatch_rpc_response(const std::vector<MpackValue>& msg_array)
{
    const auto rawId = msg_array[1].as_int();
    if (rawId < 0 || rawId > static_cast<int64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return;
    }

    RpcResponse response;
    response.msgid = static_cast<std::uint32_t>(rawId);
    response.error = msg_array[2];
    response.result = msg_array[3];

    {
        std::lock_guard lock(m_impl->response_mutex);
        m_impl->responses[response.msgid] = std::move(response);
    }
    m_impl->response_cv.notify_all();
}

void NvimRpc::dispatch_rpc_request(const std::vector<MpackValue>& msg_array)
{
    const auto rawId = msg_array[1].as_int();
    if (rawId < 0 || rawId > static_cast<int64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return;
    }

    const auto msgid = static_cast<std::uint32_t>(rawId);
    const std::string method = msg_array[2].as_str();
    std::vector<MpackValue> params;
    if (msg_array[3].type() == MpackValue::Array)
    {
        params = msg_array[3].as_array();
    }

    MpackValue error = NvimRpc::make_nil();
    MpackValue result = NvimRpc::make_nil();
    if (m_callbacks.on_request)
    {
        result = m_callbacks.on_request(method, params);
    }
    else
    {
        error = NvimRpc::make_str("no handler for: " + method);
    }

    reply_to_request(msgid, error, result);
}

void NvimRpc::dispatch_rpc_notification(const std::vector<MpackValue>& msg_array)
{
    RpcNotification notification;
    notification.method = msg_array[1].as_str();
    if (msg_array[2].type() == MpackValue::Array)
    {
        notification.params = msg_array[2].as_array();
    }

    {
        std::lock_guard lock(m_impl->notification_mutex);
        if (m_impl->notifications.size() >= kMaxNotificationQueueDepth)
        {
            m_impl->notifications.pop_front();
        }
        m_impl->notifications.push_back(std::move(notification));
    }

    if (m_callbacks.on_notification_available)
    {
        m_callbacks.on_notification_available();
    }
}

void NvimRpc::dispatch_rpc_message(const MpackValue& msg)
{
    if (msg.type() != MpackValue::Array || msg.as_array().size() < 3)
    {
        return;
    }

    const auto& msgArray = msg.as_array();
    const int type = static_cast<int>(msgArray[0].as_int());

    if (type == 0 && msgArray.size() >= 4)
    {
        dispatch_rpc_request(msgArray);
    }
    else if (type == 1 && msgArray.size() >= 4)
    {
        dispatch_rpc_response(msgArray);
    }
    else if (type == 2)
    {
        dispatch_rpc_notification(msgArray);
    }
}

void NvimRpc::reader_thread_func()
{
    std::vector<std::uint8_t> accumulated;
    accumulated.reserve(1024 * 1024);
    std::size_t readPosition = 0;

    while (m_impl->running)
    {
        const int n = m_impl->transport->read(m_impl->read_buffer.data(), m_impl->read_buffer.size());
        if (n <= 0)
        {
            if (m_impl->running)
            {
                m_impl->read_failed = true;
                m_impl->running = false;
                m_impl->response_cv.notify_all();
                if (m_callbacks.on_notification_available)
                {
                    m_callbacks.on_notification_available();
                }
            }
            break;
        }

        accumulated.insert(accumulated.end(), m_impl->read_buffer.begin(), m_impl->read_buffer.begin() + n);

        while (readPosition < accumulated.size())
        {
            MpackValue msg;
            std::size_t consumed = 0;
            bool hardError = false;
            const std::span<const std::uint8_t> remaining(accumulated.data() + readPosition, accumulated.size() - readPosition);
            if (!decode_mpack_value(remaining, msg, &consumed, &hardError) || consumed == 0)
            {
                if (!hardError)
                {
                    break;
                }

                ++readPosition;
                continue;
            }

            readPosition += consumed;

            if (readPosition == accumulated.size())
            {
                accumulated.clear();
                readPosition = 0;
            }

            try
            {
                dispatch_rpc_message(msg);
            }
            catch (const std::exception&)
            {
            }
        }

        if (readPosition > 65536 && readPosition < accumulated.size())
        {
            accumulated.erase(accumulated.begin(), accumulated.begin() + static_cast<std::ptrdiff_t>(readPosition));
            readPosition = 0;
        }
    }
}

void NvimRpc::reply_to_request(std::uint32_t msgid, const MpackValue& error, const MpackValue& result)
{
    std::vector<char> encoded;
    if (!encode_rpc_response(msgid, error, result, encoded))
    {
        return;
    }

    std::lock_guard lock(m_impl->write_mutex);
    if (m_impl->transport && !m_impl->transport->write(reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size()))
    {
        m_impl->read_failed = true;
        m_impl->response_cv.notify_all();
        if (m_callbacks.on_notification_available)
        {
            m_callbacks.on_notification_available();
        }
    }
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
