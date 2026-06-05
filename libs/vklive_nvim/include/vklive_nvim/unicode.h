#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vklive_nvim/types.h>
#include <string_view>

namespace vklive_nvim
{

struct UnicodeRange
{
    uint32_t first;
    uint32_t last;
};

template <size_t N>
constexpr bool in_ranges(uint32_t cp, const std::array<UnicodeRange, N>& ranges)
{
    for (const auto& range : ranges)
    {
        if (cp >= range.first && cp <= range.last)
            return true;
    }
    return false;
}

inline int utf8_sequence_length(uint8_t lead)
{
    if (lead < 0x80)
        return 1;
    if (lead >= 0xC2 && lead <= 0xDF)
        return 2;
    if (lead >= 0xE0 && lead <= 0xEF)
        return 3;
    if (lead >= 0xF0 && lead <= 0xF4)
        return 4;
    return 1;
}

inline bool utf8_decode_next(std::string_view text, size_t& offset, uint32_t& cp)
{
    if (offset >= text.size())
        return false;

    const auto* bytes = reinterpret_cast<const uint8_t*>(text.data());
    const uint8_t lead = bytes[offset];
    const size_t remaining = text.size() - offset;
    auto is_cont = [](uint8_t byte) { return (byte & 0xC0) == 0x80; };

    if (lead < 0x80)
    {
        cp = lead;
        offset += 1;
        return true;
    }

    if (lead >= 0xC2 && lead <= 0xDF && remaining >= 2 && is_cont(bytes[offset + 1]))
    {
        cp = ((lead & 0x1F) << 6) | (bytes[offset + 1] & 0x3F);
        offset += 2;
        return true;
    }

    if (remaining >= 3)
    {
        const uint8_t b1 = bytes[offset + 1];
        const uint8_t b2 = bytes[offset + 2];
        if (lead == 0xE0 && b1 >= 0xA0 && b1 <= 0xBF && is_cont(b2))
        {
            cp = ((lead & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            offset += 3;
            return true;
        }
        if (lead >= 0xE1 && lead <= 0xEC && is_cont(b1) && is_cont(b2))
        {
            cp = ((lead & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            offset += 3;
            return true;
        }
        if (lead == 0xED && b1 >= 0x80 && b1 <= 0x9F && is_cont(b2))
        {
            cp = ((lead & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            offset += 3;
            return true;
        }
        if (lead >= 0xEE && lead <= 0xEF && is_cont(b1) && is_cont(b2))
        {
            cp = ((lead & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            offset += 3;
            return true;
        }
    }

    if (remaining >= 4)
    {
        const uint8_t b1 = bytes[offset + 1];
        const uint8_t b2 = bytes[offset + 2];
        const uint8_t b3 = bytes[offset + 3];
        if (lead == 0xF0 && b1 >= 0x90 && b1 <= 0xBF && is_cont(b2) && is_cont(b3))
        {
            cp = ((lead & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
            offset += 4;
            return true;
        }
        if (lead >= 0xF1 && lead <= 0xF3 && is_cont(b1) && is_cont(b2) && is_cont(b3))
        {
            cp = ((lead & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
            offset += 4;
            return true;
        }
        if (lead == 0xF4 && b1 >= 0x80 && b1 <= 0x8F && is_cont(b2) && is_cont(b3))
        {
            cp = ((lead & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
            offset += 4;
            return true;
        }
    }

    cp = 0xFFFD;
    offset += 1;
    return true;
}

inline uint32_t utf8_first_codepoint(std::string_view text)
{
    if (text.empty())
        return ' ';

    size_t offset = 0;
    uint32_t cp = ' ';
    utf8_decode_next(text, offset, cp);
    return cp;
}

inline bool is_east_asian_wide(uint32_t cp)
{
    return (cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A || (cp >= 0x2E80 && cp <= 0x303E)
        || (cp >= 0x3040 && cp <= 0xA4CF && cp != 0x303F) || (cp >= 0xAC00 && cp <= 0xD7A3)
        || (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE19)
        || (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF01 && cp <= 0xFF60)
        || (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x2FFFD)
        || (cp >= 0x30000 && cp <= 0x3FFFD);
}

inline bool is_east_asian_ambiguous(uint32_t cp)
{
    static constexpr std::array kRanges = {
        UnicodeRange{ 0x00A1, 0x00A1 },
        UnicodeRange{ 0x00A4, 0x00A4 },
        UnicodeRange{ 0x00A7, 0x00A8 },
        UnicodeRange{ 0x00AA, 0x00AA },
        UnicodeRange{ 0x00AD, 0x00AE },
        UnicodeRange{ 0x00B0, 0x00B4 },
        UnicodeRange{ 0x00B6, 0x00BA },
        UnicodeRange{ 0x00BC, 0x00BF },
        UnicodeRange{ 0x00C6, 0x00C6 },
        UnicodeRange{ 0x00D0, 0x00D0 },
        UnicodeRange{ 0x00D7, 0x00D8 },
        UnicodeRange{ 0x00DE, 0x00E1 },
        UnicodeRange{ 0x00E6, 0x00E6 },
        UnicodeRange{ 0x00E8, 0x00EA },
        UnicodeRange{ 0x00EC, 0x00ED },
        UnicodeRange{ 0x00F0, 0x00F0 },
        UnicodeRange{ 0x00F2, 0x00F3 },
        UnicodeRange{ 0x00F7, 0x00FA },
        UnicodeRange{ 0x00FC, 0x00FC },
        UnicodeRange{ 0x00FE, 0x00FE },
        UnicodeRange{ 0x0101, 0x0101 },
        UnicodeRange{ 0x0111, 0x0111 },
        UnicodeRange{ 0x0113, 0x0113 },
        UnicodeRange{ 0x011B, 0x011B },
        UnicodeRange{ 0x0126, 0x0127 },
        UnicodeRange{ 0x012B, 0x012B },
        UnicodeRange{ 0x0131, 0x0133 },
        UnicodeRange{ 0x0138, 0x0138 },
        UnicodeRange{ 0x013F, 0x0142 },
        UnicodeRange{ 0x0144, 0x0144 },
        UnicodeRange{ 0x0148, 0x014B },
        UnicodeRange{ 0x014D, 0x014D },
        UnicodeRange{ 0x0152, 0x0153 },
        UnicodeRange{ 0x0166, 0x0167 },
        UnicodeRange{ 0x016B, 0x016B },
        UnicodeRange{ 0x01CE, 0x01CE },
        UnicodeRange{ 0x01D0, 0x01D0 },
        UnicodeRange{ 0x01D2, 0x01D2 },
        UnicodeRange{ 0x01D4, 0x01D4 },
        UnicodeRange{ 0x01D6, 0x01D6 },
        UnicodeRange{ 0x01D8, 0x01D8 },
        UnicodeRange{ 0x01DA, 0x01DA },
        UnicodeRange{ 0x01DC, 0x01DC },
        UnicodeRange{ 0x0251, 0x0251 },
        UnicodeRange{ 0x0261, 0x0261 },
        UnicodeRange{ 0x02C4, 0x02C4 },
        UnicodeRange{ 0x02C7, 0x02C7 },
        UnicodeRange{ 0x02C9, 0x02CB },
        UnicodeRange{ 0x02CD, 0x02CD },
        UnicodeRange{ 0x02D0, 0x02D0 },
        UnicodeRange{ 0x02D8, 0x02DB },
        UnicodeRange{ 0x02DD, 0x02DD },
        UnicodeRange{ 0x02DF, 0x02DF },
        UnicodeRange{ 0x0300, 0x036F },
        UnicodeRange{ 0x0391, 0x03A1 },
        UnicodeRange{ 0x03A3, 0x03A9 },
        UnicodeRange{ 0x03B1, 0x03C1 },
        UnicodeRange{ 0x03C3, 0x03C9 },
        UnicodeRange{ 0x0401, 0x0401 },
        UnicodeRange{ 0x0410, 0x044F },
        UnicodeRange{ 0x0451, 0x0451 },
        UnicodeRange{ 0x2010, 0x2010 },
        UnicodeRange{ 0x2013, 0x2016 },
        UnicodeRange{ 0x2018, 0x2019 },
        UnicodeRange{ 0x201C, 0x201D },
        UnicodeRange{ 0x2020, 0x2022 },
        UnicodeRange{ 0x2024, 0x2027 },
        UnicodeRange{ 0x2030, 0x2030 },
        UnicodeRange{ 0x2032, 0x2033 },
        UnicodeRange{ 0x2035, 0x2035 },
        UnicodeRange{ 0x203B, 0x203B },
        UnicodeRange{ 0x203E, 0x203E },
        UnicodeRange{ 0x2074, 0x2074 },
        UnicodeRange{ 0x207F, 0x207F },
        UnicodeRange{ 0x2081, 0x2084 },
        UnicodeRange{ 0x20AC, 0x20AC },
        UnicodeRange{ 0x2103, 0x2103 },
        UnicodeRange{ 0x2105, 0x2105 },
        UnicodeRange{ 0x2109, 0x2109 },
        UnicodeRange{ 0x2113, 0x2113 },
        UnicodeRange{ 0x2116, 0x2116 },
        UnicodeRange{ 0x2121, 0x2122 },
        UnicodeRange{ 0x2126, 0x2126 },
        UnicodeRange{ 0x212B, 0x212B },
        UnicodeRange{ 0x2153, 0x2154 },
        UnicodeRange{ 0x215B, 0x215E },
        UnicodeRange{ 0x2160, 0x216B },
        UnicodeRange{ 0x2170, 0x2179 },
        UnicodeRange{ 0x2189, 0x2189 },
        UnicodeRange{ 0x2190, 0x2199 },
        UnicodeRange{ 0x21B8, 0x21B9 },
        UnicodeRange{ 0x21D2, 0x21D2 },
        UnicodeRange{ 0x21D4, 0x21D4 },
        UnicodeRange{ 0x21E7, 0x21E7 },
        UnicodeRange{ 0x2200, 0x2200 },
        UnicodeRange{ 0x2202, 0x2203 },
        UnicodeRange{ 0x2207, 0x2208 },
        UnicodeRange{ 0x220B, 0x220B },
        UnicodeRange{ 0x220F, 0x220F },
        UnicodeRange{ 0x2211, 0x2211 },
        UnicodeRange{ 0x2215, 0x2215 },
        UnicodeRange{ 0x221A, 0x221A },
        UnicodeRange{ 0x221D, 0x2220 },
        UnicodeRange{ 0x2223, 0x2223 },
        UnicodeRange{ 0x2225, 0x2225 },
        UnicodeRange{ 0x2227, 0x222C },
        UnicodeRange{ 0x222E, 0x222E },
        UnicodeRange{ 0x2234, 0x2237 },
        UnicodeRange{ 0x223C, 0x223D },
        UnicodeRange{ 0x2248, 0x2248 },
        UnicodeRange{ 0x224C, 0x224C },
        UnicodeRange{ 0x2252, 0x2252 },
        UnicodeRange{ 0x2260, 0x2261 },
        UnicodeRange{ 0x2264, 0x2267 },
        UnicodeRange{ 0x226A, 0x226B },
        UnicodeRange{ 0x226E, 0x226F },
        UnicodeRange{ 0x2282, 0x2283 },
        UnicodeRange{ 0x2286, 0x2287 },
        UnicodeRange{ 0x2295, 0x2295 },
        UnicodeRange{ 0x2299, 0x2299 },
        UnicodeRange{ 0x22A5, 0x22A5 },
        UnicodeRange{ 0x22BF, 0x22BF },
        UnicodeRange{ 0x2312, 0x2312 },
        UnicodeRange{ 0x2460, 0x24E9 },
        UnicodeRange{ 0x24EB, 0x254B },
        UnicodeRange{ 0x2550, 0x2573 },
        UnicodeRange{ 0x2580, 0x258F },
        UnicodeRange{ 0x2592, 0x2595 },
        UnicodeRange{ 0x25A0, 0x25A1 },
        UnicodeRange{ 0x25A3, 0x25A9 },
        UnicodeRange{ 0x25B2, 0x25B3 },
        UnicodeRange{ 0x25B6, 0x25B7 },
        UnicodeRange{ 0x25BC, 0x25BD },
        UnicodeRange{ 0x25C0, 0x25C1 },
        UnicodeRange{ 0x25C6, 0x25C8 },
        UnicodeRange{ 0x25CB, 0x25CB },
        UnicodeRange{ 0x25CE, 0x25D1 },
        UnicodeRange{ 0x25E2, 0x25E5 },
        UnicodeRange{ 0x25EF, 0x25EF },
        UnicodeRange{ 0x2605, 0x2606 },
        UnicodeRange{ 0x2609, 0x2609 },
        UnicodeRange{ 0x260E, 0x260F },
        UnicodeRange{ 0x2614, 0x2615 },
        UnicodeRange{ 0x261C, 0x261C },
        UnicodeRange{ 0x261E, 0x261E },
        UnicodeRange{ 0x2640, 0x2640 },
        UnicodeRange{ 0x2642, 0x2642 },
        UnicodeRange{ 0x2660, 0x2661 },
        UnicodeRange{ 0x2663, 0x2665 },
        UnicodeRange{ 0x2667, 0x266A },
        UnicodeRange{ 0x266C, 0x266D },
        UnicodeRange{ 0x266F, 0x266F },
        UnicodeRange{ 0x269E, 0x269F },
        UnicodeRange{ 0x26BE, 0x26BF },
        UnicodeRange{ 0x26C4, 0x26CD },
        UnicodeRange{ 0x26CF, 0x26E1 },
        UnicodeRange{ 0x26E3, 0x26E3 },
        UnicodeRange{ 0x26E8, 0x26FF },
        UnicodeRange{ 0x273D, 0x273D },
        UnicodeRange{ 0x2776, 0x277F },
        UnicodeRange{ 0x2B55, 0x2B59 },
        UnicodeRange{ 0x3248, 0x324F },
        UnicodeRange{ 0xFFFD, 0xFFFD },
    };
    return in_ranges(cp, kRanges);
}

inline bool is_default_emoji_presentation(uint32_t cp)
{
    static constexpr std::array kRanges = {
        UnicodeRange{ 0x231A, 0x231B },
        UnicodeRange{ 0x23E9, 0x23EC },
        UnicodeRange{ 0x23F0, 0x23F0 },
        UnicodeRange{ 0x23F3, 0x23F3 },
        UnicodeRange{ 0x25FD, 0x25FE },
        UnicodeRange{ 0x2614, 0x2615 },
        UnicodeRange{ 0x2648, 0x2653 },
        UnicodeRange{ 0x267F, 0x267F },
        UnicodeRange{ 0x2693, 0x2693 },
        UnicodeRange{ 0x26A1, 0x26A1 },
        UnicodeRange{ 0x26AA, 0x26AB },
        UnicodeRange{ 0x26BD, 0x26BE },
        UnicodeRange{ 0x26C4, 0x26C5 },
        UnicodeRange{ 0x26CE, 0x26CE },
        UnicodeRange{ 0x26D4, 0x26D4 },
        UnicodeRange{ 0x26EA, 0x26EA },
        UnicodeRange{ 0x26F2, 0x26F3 },
        UnicodeRange{ 0x26F5, 0x26F5 },
        UnicodeRange{ 0x26FA, 0x26FA },
        UnicodeRange{ 0x26FD, 0x26FD },
        UnicodeRange{ 0x2705, 0x2705 },
        UnicodeRange{ 0x270A, 0x270B },
        UnicodeRange{ 0x2728, 0x2728 },
        UnicodeRange{ 0x274C, 0x274C },
        UnicodeRange{ 0x274E, 0x274E },
        UnicodeRange{ 0x2753, 0x2755 },
        UnicodeRange{ 0x2757, 0x2757 },
        UnicodeRange{ 0x2795, 0x2797 },
        UnicodeRange{ 0x27B0, 0x27B0 },
        UnicodeRange{ 0x27BF, 0x27BF },
        UnicodeRange{ 0x2B1B, 0x2B1C },
        UnicodeRange{ 0x2B50, 0x2B50 },
        UnicodeRange{ 0x2B55, 0x2B55 },
        UnicodeRange{ 0x1F004, 0x1F004 },
        UnicodeRange{ 0x1F0CF, 0x1F0CF },
        UnicodeRange{ 0x1F170, 0x1F171 },
        UnicodeRange{ 0x1F17E, 0x1F17F },
        UnicodeRange{ 0x1F18E, 0x1F18E },
        UnicodeRange{ 0x1F191, 0x1F19A },
        UnicodeRange{ 0x1F1E6, 0x1F1FF },
        UnicodeRange{ 0x1F201, 0x1F202 },
        UnicodeRange{ 0x1F21A, 0x1F21A },
        UnicodeRange{ 0x1F22F, 0x1F22F },
        UnicodeRange{ 0x1F232, 0x1F23A },
        UnicodeRange{ 0x1F250, 0x1F251 },
        UnicodeRange{ 0x1F300, 0x1FAFF },
    };
    return in_ranges(cp, kRanges);
}

inline bool is_emoji_text_presentation_candidate(uint32_t cp)
{
    static constexpr std::array kRanges = {
        UnicodeRange{ 0x00A9, 0x00A9 },
        UnicodeRange{ 0x00AE, 0x00AE },
        UnicodeRange{ 0x203C, 0x203C },
        UnicodeRange{ 0x2049, 0x2049 },
        UnicodeRange{ 0x2122, 0x2122 },
        UnicodeRange{ 0x2139, 0x2139 },
        UnicodeRange{ 0x2194, 0x2199 },
        UnicodeRange{ 0x21A9, 0x21AA },
        UnicodeRange{ 0x231A, 0x231B },
        UnicodeRange{ 0x2328, 0x2328 },
        UnicodeRange{ 0x23CF, 0x23CF },
        UnicodeRange{ 0x23E9, 0x23F3 },
        UnicodeRange{ 0x23F8, 0x23FA },
        UnicodeRange{ 0x24C2, 0x24C2 },
        UnicodeRange{ 0x25AA, 0x25AB },
        UnicodeRange{ 0x25B6, 0x25B6 },
        UnicodeRange{ 0x25C0, 0x25C0 },
        UnicodeRange{ 0x25FB, 0x25FE },
        UnicodeRange{ 0x2600, 0x27BF },
        UnicodeRange{ 0x2934, 0x2935 },
        UnicodeRange{ 0x2B05, 0x2B07 },
        UnicodeRange{ 0x2B1B, 0x2B1C },
        UnicodeRange{ 0x2B50, 0x2B55 },
        UnicodeRange{ 0x3030, 0x3030 },
        UnicodeRange{ 0x303D, 0x303D },
        UnicodeRange{ 0x3297, 0x3297 },
        UnicodeRange{ 0x3299, 0x3299 },
    };
    return in_ranges(cp, kRanges);
}

inline bool is_width_ignorable(uint32_t cp)
{
    static constexpr std::array kRanges = {
        UnicodeRange{ 0x0000, 0x001F },
        UnicodeRange{ 0x007F, 0x009F },
        UnicodeRange{ 0x0300, 0x036F },
        UnicodeRange{ 0x0483, 0x0489 },
        UnicodeRange{ 0x0591, 0x05BD },
        UnicodeRange{ 0x05BF, 0x05BF },
        UnicodeRange{ 0x05C1, 0x05C2 },
        UnicodeRange{ 0x05C4, 0x05C5 },
        UnicodeRange{ 0x05C7, 0x05C7 },
        UnicodeRange{ 0x0610, 0x061A },
        UnicodeRange{ 0x064B, 0x065F },
        UnicodeRange{ 0x0670, 0x0670 },
        UnicodeRange{ 0x06D6, 0x06ED },
        UnicodeRange{ 0x0711, 0x0711 },
        UnicodeRange{ 0x0730, 0x074A },
        UnicodeRange{ 0x07A6, 0x07B0 },
        UnicodeRange{ 0x07EB, 0x07F3 },
        UnicodeRange{ 0x0816, 0x082D },
        UnicodeRange{ 0x0859, 0x085B },
        UnicodeRange{ 0x08D3, 0x0903 },
        UnicodeRange{ 0x093A, 0x094F },
        UnicodeRange{ 0x0951, 0x0957 },
        UnicodeRange{ 0x0962, 0x0963 },
        UnicodeRange{ 0x1AB0, 0x1AFF },
        UnicodeRange{ 0x1DC0, 0x1DFF },
        UnicodeRange{ 0x200C, 0x200F },
        UnicodeRange{ 0x202A, 0x202E },
        UnicodeRange{ 0x2060, 0x206F },
        UnicodeRange{ 0x20D0, 0x20FF },
        UnicodeRange{ 0xFE00, 0xFE0F },
        UnicodeRange{ 0xFE20, 0xFE2F },
        UnicodeRange{ 0xE0100, 0xE01EF },
    };
    return in_ranges(cp, kRanges);
}

inline bool is_regional_indicator(uint32_t cp)
{
    return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

inline bool is_emoji_modifier(uint32_t cp)
{
    return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

inline bool is_ascii_keycap_base(uint32_t cp)
{
    return cp == '#' || cp == '*' || (cp >= '0' && cp <= '9');
}

inline int cluster_cell_width(std::string_view text, const UiOptions& options = {})
{
    if (text.empty())
        return 1;

    size_t offset = 0;
    uint32_t base = 0;
    bool have_base = false;
    bool has_vs16 = false;
    bool has_zwj = false;
    bool has_emoji_modifier = false;

    while (offset < text.size())
    {
        uint32_t cp = 0;
        if (!utf8_decode_next(text, offset, cp))
            break;

        if (cp == 0xFE0F)
            has_vs16 = true;
        else if (cp == 0x200D)
            has_zwj = true;
        else if (is_emoji_modifier(cp))
            has_emoji_modifier = true;

        if (!have_base && !is_width_ignorable(cp) && !is_emoji_modifier(cp))
        {
            base = cp;
            have_base = true;
        }
    }

    if (!have_base)
        return 1;

    if (is_east_asian_wide(base) || is_default_emoji_presentation(base) || is_regional_indicator(base))
        return 2;

    if (options.ambiwidth == AmbiWidth::Double && is_east_asian_ambiguous(base))
        return 2;

    if ((has_vs16 || has_zwj || has_emoji_modifier) && is_emoji_text_presentation_candidate(base)
        && !is_ascii_keycap_base(base))
        return 2;

    return 1;
}

} // namespace vklive_nvim
