#include <vklive_nvim/mpack_codec.h>
#include <vklive_nvim/nvim_host.h>
#include <vklive_nvim/nvim_rpc.h>
#include <vklive_nvim/render_model.h>

#include <algorithm>
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

class AutoReplyTransport final : public vklive_nvim::IRpcTransport
{
public:
    bool write(const std::uint8_t* data, std::size_t len) override
    {
        vklive_nvim::MpackValue message;
        std::size_t consumed = 0;
        assert(vklive_nvim::decode_mpack_value(std::span<const std::uint8_t>(data, len), message, &consumed));
        assert(consumed == len);

        std::vector<char> response;
        {
            std::lock_guard lock(m_mutex);
            const auto& items = message.as_array();
            const int type = static_cast<int>(items[0].as_int());
            if (type == 0)
            {
                m_methods.push_back(items[2].as_str());
                if (items[2].as_str() == std::string("nvim_command") && items[3].type() == vklive_nvim::MpackValue::Array && !items[3].as_array().empty())
                {
                    m_commands.push_back(items[3].as_array()[0].as_str());
                }

                assert(vklive_nvim::encode_mpack_value(
                    vklive_nvim::NvimRpc::make_array({
                        vklive_nvim::NvimRpc::make_int(1),
                        items[1],
                        vklive_nvim::NvimRpc::make_nil(),
                        vklive_nvim::NvimRpc::make_nil(),
                    }),
                    response));
                m_reads.emplace_back(response.begin(), response.end());
            }
            else if (type == 2)
            {
                m_methods.push_back(items[1].as_str());
            }
        }
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

    void interrupt_read() override
    {
        m_running = false;
        m_cv.notify_all();
    }

    void push_message(const vklive_nvim::MpackValue& value)
    {
        std::vector<char> encoded;
        assert(vklive_nvim::encode_mpack_value(value, encoded));

        std::lock_guard lock(m_mutex);
        m_reads.emplace_back(encoded.begin(), encoded.end());
        m_cv.notify_all();
    }

    std::vector<std::string> methods() const
    {
        std::lock_guard lock(m_mutex);
        return m_methods;
    }

    std::vector<std::string> commands() const
    {
        std::lock_guard lock(m_mutex);
        return m_commands;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<std::vector<std::uint8_t>> m_reads;
    std::vector<std::string> m_methods;
    std::vector<std::string> m_commands;
    bool m_running = true;
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

bool contains(const std::vector<std::string>& values, const std::string& expected)
{
    return std::find(values.begin(), values.end(), expected) != values.end();
}

} // namespace

int main()
{
    AutoReplyTransport transport;

    vklive_nvim::NvimHost host;
    vklive_nvim::NvimHostOptions options;
    options.columns = 100;
    options.rows = 40;
    options.transport = &transport;

    assert(host.start(options));
    assert(host.running());

    auto methods = transport.methods();
    assert(!methods.empty());
    assert(methods[0] == std::string("nvim_ui_attach"));
    assert(contains(methods, "nvim_command"));

    vklive_nvim::NvimProjectFiles project;
    project.project_root = "D:/projects/demo";
    project.files = { "D:/projects/demo/a.frag", "D:/projects/demo/b.vert" };
    host.open_project_files(project);

    const auto commands = transport.commands();
    assert(contains(commands, "set termguicolors"));
    assert(contains(commands, "set noshowmode"));
    assert(contains(commands, "set mouse=a"));
    assert(contains(commands, "silent! tabonly"));
    assert(contains(commands, "tabedit D:/projects/demo/a.frag"));
    assert(contains(commands, "tabedit D:/projects/demo/b.vert"));
    assert(contains(commands, "tabfirst"));

    transport.push_message(array({
        integer(2),
        str("redraw"),
        array({
            array({ str("grid_resize"), array({ integer(1), integer(3), integer(1) }) }),
            array({ str("grid_line"), array({ integer(1), integer(0), integer(0), array({ array({ str("X"), integer(0) }) }) }) }),
        }),
    }));

    for (int i = 0; i < 100 && host.render_model().cell(0, 0).text != std::string("X"); ++i)
    {
        host.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    host.pump();
    assert(host.render_model().columns() == 3);
    assert(host.render_model().cell(0, 0).text == std::string("X"));

    host.stop();
    assert(!host.running());
}
