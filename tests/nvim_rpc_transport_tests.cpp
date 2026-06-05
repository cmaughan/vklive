#include <vklive_nvim/mpack_codec.h>
#include <vklive_nvim/nvim_rpc.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace
{

class FakeTransport final : public vklive_nvim::IRpcTransport
{
public:
    bool write(const std::uint8_t* data, std::size_t len) override
    {
        std::lock_guard lock(m_mutex);
        m_writes.emplace_back(data, data + len);
        m_cv.notify_all();
        return true;
    }

    int read(std::uint8_t* buffer, std::size_t max_len) override
    {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this]() {
            return !m_reads.empty() || !m_running;
        });

        if (m_reads.empty())
        {
            return 0;
        }

        auto& front = m_reads.front();
        const std::size_t count = std::min(max_len, front.size());
        std::copy_n(front.begin(), count, buffer);
        front.erase(front.begin(), front.begin() + static_cast<std::ptrdiff_t>(count));
        if (front.empty())
        {
            m_reads.erase(m_reads.begin());
        }
        return static_cast<int>(count);
    }

    bool is_running() const override
    {
        return m_running;
    }

    void push_message(const vklive_nvim::MpackValue& value)
    {
        std::vector<char> encoded;
        const bool encodedOk = vklive_nvim::encode_mpack_value(value, encoded);
        assert(encodedOk);
        if (!encodedOk)
        {
            return;
        }

        std::lock_guard lock(m_mutex);
        m_reads.emplace_back(encoded.begin(), encoded.end());
        m_cv.notify_all();
    }

    void close()
    {
        m_running = false;
        m_cv.notify_all();
    }

    bool wait_for_write_count(std::size_t expected)
    {
        std::unique_lock lock(m_mutex);
        return m_cv.wait_for(lock, std::chrono::seconds(2), [this, expected]() {
            return m_writes.size() >= expected;
        });
    }

    std::vector<std::uint8_t> written_message(std::size_t index) const
    {
        std::lock_guard lock(m_mutex);
        return m_writes.at(index);
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<std::vector<std::uint8_t>> m_reads;
    std::vector<std::vector<std::uint8_t>> m_writes;
    std::atomic<bool> m_running{ true };
};

vklive_nvim::MpackValue array(std::vector<vklive_nvim::MpackValue> values)
{
    return vklive_nvim::NvimRpc::make_array(std::move(values));
}

vklive_nvim::MpackValue str(const std::string& value)
{
    return vklive_nvim::NvimRpc::make_str(value);
}

vklive_nvim::MpackValue integer(int value)
{
    return vklive_nvim::NvimRpc::make_int(value);
}

} // namespace

int main()
{
    {
        FakeTransport transport;
        std::atomic<int> wakeups = 0;

        vklive_nvim::NvimRpc rpc;
        const bool initialized = rpc.initialize(transport, { [&wakeups]() { ++wakeups; }, {} });
        if (!initialized)
        {
            return 1;
        }

        transport.push_message(array({ integer(2), str("redraw"), array({ str("payload") }) }));

        for (int i = 0; i < 100 && wakeups.load() == 0; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        const auto notifications = rpc.drain_notifications();
        if (notifications.size() != 1 || notifications[0].method != "redraw"
            || notifications[0].params.size() != 1 || notifications[0].params[0].as_str() != std::string("payload"))
        {
            return 1;
        }

        transport.close();
        rpc.shutdown();
    }

    {
        FakeTransport transport;
        vklive_nvim::NvimRpc rpc;
        const bool initialized = rpc.initialize(transport);
        if (!initialized)
        {
            return 1;
        }

        vklive_nvim::RpcResult result;
        std::thread requester([&]() {
            result = rpc.request("nvim_command", { str("set number") });
        });

        if (!transport.wait_for_write_count(1))
        {
            return 1;
        }

        vklive_nvim::MpackValue request;
        std::size_t consumed = 0;
        const auto written = transport.written_message(0);
        const bool decoded = vklive_nvim::decode_mpack_value(std::span<const std::uint8_t>(written.data(), written.size()), request, &consumed);
        if (!decoded || consumed != written.size() || request.as_array()[2].as_str() != std::string("nvim_command"))
        {
            return 1;
        }

        transport.push_message(array({ integer(1), integer(1), vklive_nvim::NvimRpc::make_nil(), str("ok") }));
        requester.join();

        if (!result || result.value().as_str() != std::string("ok"))
        {
            return 1;
        }

        transport.close();
        rpc.shutdown();
    }
}
