#include <chrono>
#include <sstream>
#include <string>

#include "vklive/logger/logger.h"
#include "vklive/time/timer.h"

using namespace std::chrono;

timer globalTimer;
uint32_t globalFrameCount = 0;

double timer_to_seconds(uint64_t value)
{
    return double(value / 1000000000.0);
}

double timer_to_ms(uint64_t value)
{
    return double(value / 1000000.0);
}

DateTime datetime_from_seconds(uint64_t t)
{
    return DateTime(std::chrono::seconds(t));
}

DateTime datetime_from_seconds(std::chrono::seconds s)
{
    return DateTime(s);
}

DateTime datetime_from_timer_start(timer& timer)
{
    return DateTime(seconds(timer_to_epoch_utc_seconds(timer)));
}

