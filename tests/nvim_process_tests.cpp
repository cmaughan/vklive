#include <vklive_nvim/nvim_process.h>

#include <cassert>
#include <string>

namespace
{

std::string missing_nvim_path()
{
#ifdef _WIN32
    return "C:/definitely-missing/vklive-test-nvim.exe";
#else
    return "/definitely-missing/vklive-test-nvim";
#endif
}

} // namespace

int main()
{
    vklive_nvim::NvimProcess process;

    const auto result = process.spawn(missing_nvim_path());
    assert(!result);
    assert(!result.message.empty());
    assert(!process.is_running());

    process.shutdown();
    assert(!process.is_running());
}
