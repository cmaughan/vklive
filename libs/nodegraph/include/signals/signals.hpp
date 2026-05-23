#pragma once

#include <functional>
#include <vector>

namespace fteng
{

template <typename Signature>
class signal;

template <typename... Args>
class signal<void(Args...)>
{
public:
    using Slot = std::function<void(Args...)>;

    void connect(Slot slot)
    {
        m_slots.push_back(std::move(slot));
    }

    void operator()(Args... args) const
    {
        for (const auto& slot : m_slots)
        {
            slot(args...);
        }
    }

private:
    std::vector<Slot> m_slots;
};

} // namespace fteng
