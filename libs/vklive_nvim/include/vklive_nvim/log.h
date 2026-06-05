#pragma once

namespace vklive_nvim
{

enum class LogCategory
{
    Font,
};

} // namespace vklive_nvim

#define DRAXUL_LOG_DEBUG(...) ((void)0)
#define DRAXUL_LOG_WARN(...) ((void)0)
#define DRAXUL_LOG_ERROR(...) ((void)0)
