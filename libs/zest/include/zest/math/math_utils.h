#pragma once

#include <zest/math/math.h>
#include <algorithm>

namespace Zest
{
float SmoothStep(float val);

template <class T>
bool IsRectEmpty(const T& rect)
{
    return (rect.z == 0 || rect.w == 0);
}

template <typename T, typename P>
bool RectContains(const T& rect, const P& point)
{
    return ((rect.x <= point.x && (rect.x + rect.z) >= point.x) && (rect.y <= point.y && (rect.y + rect.w) >= point.y));
}

double RandRange(double begin, double end);
float RandRange(float begin, float end);
uint32_t RandRange(uint32_t min, uint32_t max);
int32_t RandRange(int32_t begin, int32_t end);

template<class T>
void GetBounds(const glm::vec<3, T, glm::defaultp>* coords, uint32_t count, glm::vec<3, T, glm::defaultp>& min, glm::vec<3, T, glm::defaultp>& max)
{
    if (count == 0 || coords == nullptr)
    {
        min = glm::vec<3, T, glm::defaultp>();
        max = glm::vec<3, T, glm::defaultp>();
        return;
    }

    min = glm::vec<3, T, glm::defaultp>(std::numeric_limits<float>::max());
    max = glm::vec<3, T, glm::defaultp>(-std::numeric_limits<float>::max());
    for (uint32_t i = 0; i < count; i++)
    {
        min = glm::min(min, coords[i]);
        max = glm::max(max, coords[i]);
    }
}

//glm::quat QuatFromVectors(glm::vec3 u, glm::vec3 v);
glm::vec4 RectClip(const glm::vec4& rect, const glm::vec4& clip);
float Luminance(const glm::vec4& color);
float Luminance(const glm::vec3& color);
float LuminanceABGR(const uint32_t& color);
float LuminanceARGB(const uint32_t& color);
glm::vec4 Saturate(const glm::vec4& col);
glm::vec4 Desaturate(const glm::vec4& col);

template <typename T>
T Clamp(const T& val, const T& min, const T& max)
{
    return std::max(min, std::min(max, val));
}

template <typename T>
inline T AlignUpWithMask(T value, size_t mask)
{
    return (T)(((size_t)value + mask) & ~mask);
}

template <typename T>
inline T AlignDownWithMask(T value, size_t mask)
{
    return (T)((size_t)value & ~mask);
}

template <typename T>
inline T AlignUp(T value, size_t alignment)
{
    return AlignUpWithMask(value, alignment - 1);
}

template <typename T>
inline T AlignDown(T value, size_t alignment)
{
    return AlignDownWithMask(value, alignment - 1);
}

template <typename T>
inline bool IsAligned(T value, size_t alignment)
{
    return 0 == ((size_t)value & (alignment - 1));
}

template <typename T>
inline T DivideByMultiple(T value, size_t alignment)
{
    return (T)((value + alignment - 1) / alignment);
}

template <typename T>
inline bool IsPowerOfTwo(T value)
{
    return 0 == (value & (value - 1));
}

template <typename T>
inline bool IsDivisible(T value, T divisor)
{
    return (value / divisor) * divisor == value;
}

template <typename T>
inline T Cube(T f)
{
    return f * f * f;
}

template <typename T>
inline T Square(T f)
{
    return f * f;
}

// Transcribed from here: explicit form and derivative
// https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Cubic_B%C3%A9zier_curves
template <typename T>
inline T Bezier(float t, T p0, T p1, T p2, T p3)
{
    return Cube(1 - t) * p0 + 3 * Square(1 - t) * t * p1 + 3 * (1 - t) * Square(t) * p2 + Cube(t) * p3;
}
template <typename T>
inline glm::vec<2, T, glm::defaultp> Bezier(float t, const glm::vec<2, T, glm::defaultp>& p0, const glm::vec<2, T, glm::defaultp>& p1, const glm::vec<2, T, glm::defaultp>& p2, const glm::vec<2, T, glm::defaultp>& p3)
{
    return glm::vec<2, T, glm::defaultp>(Bezier(t, p0.x, p1.x, p2.x, p3.x), Bezier(t, p0.y, p1.y, p2.y, p3.y));
}

template <typename T>
inline T BezierDerivative(float t, T p0, T p1, T p2, T p3)
{
    return 3 * Square(1 - t) * (p1 - p0) + 6 * (1 - t) * t * (p2 - p1) + 3 * Square(t) * (p3 - p2);
}

// Tangent
template <typename T>
inline glm::vec<2, T, glm::defaultp> BezierDerivative(float t, const glm::vec<2, T, glm::defaultp>& p0, const glm::vec<2, T, glm::defaultp>& p1, const glm::vec<2, T, glm::defaultp>& p2, const glm::vec<2, T, glm::defaultp>& p3)
{
    return glm::vec<2, T, glm::defaultp>(BezierDerivative(t, p0.x, p1.x, p2.x, p3.x), BezierDerivative(t, p0.y, p1.y, p2.y, p3.y));
}

// Normal
template <typename T>
inline glm::vec<2, T, glm::defaultp> BezierNormal(float t, const glm::vec<2, T, glm::defaultp>& p0, const glm::vec<2, T, glm::defaultp>& p1, const glm::vec<2, T, glm::defaultp>& p2, const glm::vec<2, T, glm::defaultp>& p3)
{
    auto deriv = BezierDerivative(t, p0, p1, p2, p3);
    return Normalized(glm::vec<2, T, glm::defaultp>(-deriv.y, deriv.x));
}

inline glm::vec4 HSVToRGB(float h, float s, float v)
{
    auto r = 0.0f, g = 0.0f, b = 0.0f;

    if (s == 0)
    {
        r = v;
        g = v;
        b = v;
    }
    else
    {
        int i;
        float f, p, q, t;

        if (h == 360)
            h = 0;
        else
            h = h / 60.0f;

        i = (int)trunc(h);
        f = h - i;

        p = v * (1.0f - s);
        q = v * (1.0f - (s * f));
        t = v * (1.0f - (s * (1.0f - f)));

        switch (i)
        {
        case 0:
            r = v;
            g = t;
            b = p;
            break;

        case 1:
            r = q;
            g = v;
            b = p;
            break;

        case 2:
            r = p;
            g = v;
            b = t;
            break;

        case 3:
            r = p;
            g = q;
            b = v;
            break;

        case 4:
            r = t;
            g = p;
            b = v;
            break;

        default:
            r = v;
            g = p;
            b = q;
            break;
        }
    }

    return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

inline uint32_t ToPacked(const glm::vec4& val)
{
    uint32_t col = 0;
    col |= uint32_t(val.x * 255.0f) << 24;
    col |= uint32_t(val.y * 255.0f) << 16;
    col |= uint32_t(val.z * 255.0f) << 8;
    col |= uint32_t(val.w * 255.0f);
    return col;
}

inline uint32_t ToPackedARGB(const glm::vec4& val)
{
    uint32_t col = 0;
    col |= uint32_t(val.w * 255.0f) << 24;
    col |= uint32_t(val.x * 255.0f) << 16;
    col |= uint32_t(val.y * 255.0f) << 8;
    col |= uint32_t(val.z * 255.0f);
    return col;
}

inline uint32_t ToPackedABGR(const glm::vec4& val)
{
    uint32_t col = 0;
    col |= uint32_t(val.w * 255.0f) << 24;
    col |= uint32_t(val.x * 255.0f);
    col |= uint32_t(val.y * 255.0f) << 8;
    col |= uint32_t(val.z * 255.0f) << 16;
    return col;
}

inline uint32_t ToPackedBGRA(const glm::vec4& val)
{
    uint32_t col = 0;
    col |= uint32_t(val.w * 255.0f) << 8;
    col |= uint32_t(val.x * 255.0f) << 16;
    col |= uint32_t(val.y * 255.0f) << 24;
    col |= uint32_t(val.z * 255.0f);
    return col;
}

inline float Luminosity(const glm::vec4& intensity)
{
    return (0.2126f * intensity.x + 0.7152f * intensity.y + 0.0722f * intensity.z);
}

inline glm::vec4 Mix(const glm::vec4& c1, const glm::vec4& c2, float factor)
{
    glm::vec4 ret = c1 * (1.0f - factor);
    ret = ret + (c2 * factor);
    return ret;
}

/*
inline uint8_t Log2(uint64_t value)
{
    unsigned long mssb; // most significant set bit
    unsigned long lssb; // least significant set bit

    // If perfect power of two (only one set bit), return index of bit.  Otherwise round up
    // fractional log by adding 1 to most signicant set bit's index.
    if (_BitScanReverse64(&mssb, value) > 0 && _BitScanForward64(&lssb, value) > 0)
        return uint8_t(mssb + (mssb == lssb ? 0 : 1));
    else
        return 0;
}

template <typename T> inline T AlignPowerOfTwo(T value)
{
    return value == 0 ? 0 : 1 << Log2(value);
}
*/

} // Zest

