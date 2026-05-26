#include <vklive_nvim/nvim_host.h>
#include <vklive_nvim/render_model.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace
{

bool nvim_available()
{
#ifdef _WIN32
    return std::system("nvim --version >NUL 2>NUL") == 0;
#else
    return std::system("nvim --version >/dev/null 2>/dev/null") == 0;
#endif
}

} // namespace

int main()
{
    if (!nvim_available())
    {
        return 0;
    }

    vklive_nvim::NvimHost host;
    vklive_nvim::NvimHostOptions options;
    options.columns = 40;
    options.rows = 12;

    assert(host.start(options));
    assert(host.running());

    for (int i = 0; i < 20; ++i)
    {
        host.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(host.render_model().columns() == 40);
    assert(host.render_model().rows() == 12);

    host.stop();
    assert(!host.running());
}
