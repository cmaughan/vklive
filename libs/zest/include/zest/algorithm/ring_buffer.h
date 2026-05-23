#pragma once

#include <cstdint>
#include <vector>

namespace Zest
{

template <class T>
struct ring_buffer
{
    std::vector<T> data;
    size_t head;
    size_t tail;
    uint32_t capacity;
};

template <class T>
void ring_buffer_init(ring_buffer<T>& buffer, uint32_t capacity)
{
    buffer.capacity = capacity;
    buffer.data.resize(capacity + 1);
    buffer.tail = buffer.head = 0U;
}

template <class T>
bool ring_buffer_empty(const ring_buffer<T>& buffer)
{
    return buffer.head == buffer.tail;
}

template <class T>
size_t ring_buffer_size(const ring_buffer<T>& buffer)
{
    return (buffer.head >= buffer.tail) ? (buffer.head - buffer.tail) : (buffer.data.size() - (buffer.tail - buffer.head));
}

template <class T>
void ring_buffer_add(ring_buffer<T>& buffer, const T& item)
{
    buffer.data[buffer.head] = item;
    buffer.head = (buffer.head + 1) % buffer.data.size();
    if (buffer.head == buffer.tail)
    {
        // Drop oldest entry, keep the rest
        buffer.tail = (buffer.tail + 1) % buffer.data.size();
    }
}

template <class T>
const T ring_buffer_drain(ring_buffer<T>& buffer)
{
    assert(!ring_buffer_empty(buffer));
    size_t old_tail = buffer.tail;
    buffer.tail = (buffer.tail + 1) % buffer.data.size();
    return buffer.data[old_tail];
}

template <class T>
void ring_buffer_assign_ordered(const ring_buffer<T>& buffer, std::vector<T>& dest, uint32_t count)
{
    dest.resize(count);
    size_t idx = buffer.tail;
    for (uint32_t i = 0; i < count; ++i)
    {
        dest[i] = buffer.data[idx];
        idx = (idx + 1) % buffer.data.size();
    }
}

template <class T>
void ring_buffer_assign_ordered_newest(const ring_buffer<T>& buffer, std::vector<T>& dest, uint32_t count)
{
    dest.resize(count);
    size_t idx = buffer.head;
    for (uint32_t i = 0; i < count; ++i)
    {
        dest[i] = buffer.data[idx];
        idx = (idx + 1) % buffer.data.size();
    }
}

template <class T>
void ring_buffer_drain_n(ring_buffer<T>& buffer, uint32_t count)
{
    if (count == 0)
        return;
    const size_t available = ring_buffer_size(buffer);
    const size_t toDrain = std::min<size_t>(count, available);
    buffer.tail = (buffer.tail + toDrain) % buffer.data.size();
}

} //namespace Zest
