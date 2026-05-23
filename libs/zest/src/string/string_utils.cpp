#include <algorithm>
#include <cassert>
#include <cctype>
#include <codecvt>
#include <cstring>
#include <iomanip>
#include <locale>
#include <regex>
#include <sstream>
#include <string>

#include <zest/string/murmur_hash.h>
#include <zest/string/string_utils.h>

using namespace std;

// StringUtils.
// Note, simple, effective string utilities which concentrate on useful functinality and correctness and not on speed!
namespace Zest
{

// https://stackoverflow.com/a/17708801/18942
string string_url_encode(const string& value)
{
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
    {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << uppercase;
        escaped << '%' << setw(2) << int((unsigned char)c);
        escaped << nouppercase;
    }

    return escaped.str();
}

bool string_ends_with(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool string_starts_with(std::string_view str, std::string_view prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

std::string string_tolower(const std::string& str)
{
    std::string copy = str;
    std::transform(copy.begin(), copy.end(), copy.begin(), ::tolower);
    return copy;
}

std::string string_replace(std::string subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

void string_replace_in_place(std::string& subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

#pragma warning(disable : 4996)
//https://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
std::string string_from_wstring(const std::wstring& str)
{
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

    //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
    std::string converted_str = converter.to_bytes(str);
    return converted_str;
}
#pragma warning(default : 4996)

/*
//https://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
std::string makeStr(const std::wstring& str)
{
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

    //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
    std::string converted_str = converter.to_bytes(str);
    return converted_str;
}
*/

std::vector<std::string> string_split(const std::string& text, const char* delims)
{
    std::vector<std::string> tok;
    string_split(text, delims, tok);
    return tok;
}

// String split with multiple delims
// https://stackoverflow.com/a/7408245/18942
void string_split(const std::string& text, const char* delims, std::vector<std::string>& tokens)
{
    tokens.clear();
    std::size_t start = text.find_first_not_of(delims), end = 0;

    while ((end = text.find_first_of(delims, start)) != std::string::npos)
    {
        tokens.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(delims, end);
    }
    if (start != std::string::npos)
        tokens.push_back(text.substr(start));
}

void string_split_each(const std::string& text, const char* delims, std::function<bool(size_t, size_t)> fn)
{
    std::size_t start = text.find_first_not_of(delims), end = 0;

    while ((end = text.find_first_of(delims, start)) != std::string::npos)
    {
        if (!fn(start, end - start))
            return;
        start = text.find_first_not_of(delims, end);
    }
    if (start != std::string::npos)
        fn(start, text.length() - start);
}

std::vector<std::string> string_split_delim_string(const std::string& str, const std::string& delim)
{
    std::vector<std::string> ret;
    std::size_t current, previous = 0;
    current = str.find(delim);
    while (current != std::string::npos)
    {
        ret.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find(delim, previous);
    }
    ret.push_back(str.substr(previous, current - previous));
    return ret;
}

size_t string_first_not_of(const char* text, size_t start, size_t end, const char* delims)
{
    for (auto index = start; index < end; index++)
    {
        bool found = false;
        auto pDelim = delims;
        while (*pDelim != 0)
        {
            if (text[index] == *pDelim++)
            {
                found = true;
                break;
            }
        }
        if (!found)
            return index;
    }
    return std::string::npos;
}

size_t string_first_of(const char* text, size_t start, size_t end, const char* delims)
{
    for (auto index = start; index < end; index++)
    {
        auto pDelim = delims;
        while (*pDelim != 0)
        {
            if (text[index] == *pDelim++)
            {
                return index;
            }
        }
    }
    return std::string::npos;
}

void string_split_each(char* text, size_t startIndex, size_t endIndex, const char* delims, std::function<bool(size_t, size_t)> fn)
{
    // Skip delims (start now at first thing that is not a delim)
    std::size_t start = string_first_not_of(text, startIndex, endIndex, delims);
    std::size_t end;

    // Find first delim (end now at first delim)
    while ((end = string_first_of(text, start, endIndex, delims)) != std::string::npos)
    {
        // Callback with string between delims
        if (!fn(start, end))
            return;

        if (text[end] == 0 && end < endIndex)
            end++;

        // Find the first non-delim
        start = string_first_not_of(text, end, endIndex, delims);
    }
    // Return the last one
    if (start != std::string::npos)
        fn(start, endIndex);
}

// Given an index into a string, count the newlines and return (line, lineOffset).
// I wrote this with a fuzzy head due to accidental caffeine intake.  It worked first time, but I should write
// a unit test.  It is probably inelegant.
std::pair<uint32_t, uint32_t> string_convert_index_to_line_offset(const std::string& str, uint32_t index)
{
    uint32_t line = 0;
    std::pair<uint32_t, uint32_t> lineRange = std::make_pair(0, 0);

    uint32_t currentIndex = 0;
    auto itr = str.begin();
    while (itr != str.end())
    {
        lineRange.second = currentIndex;

        // Found it
        if (index >= lineRange.first && index <= lineRange.second)
        {
            return std::make_pair(line, index - lineRange.first);
        }

        // Find line end
        if (*itr == '\r' || *itr == '\n')
        {
            bool isR = (*itr == '\r');
            // Skip
            itr++;
            currentIndex++;

            // Not at end and a '\r\n'
            if (itr != str.end() && isR && *itr == '\n')
            {
                // Skip another
                itr++;
                currentIndex++;
            }

            // New line starts here
            line++;
            lineRange.first = (uint32_t)currentIndex;

            if (index >= currentIndex)
            {
                return std::make_pair(line, index - lineRange.first);
            }
            continue;
        }
        itr++;
        currentIndex++;
    }
    return std::make_pair(line, index - lineRange.first);
}

void string_split_lines(const std::string& text, std::vector<std::string>& split)
{
    string_split(text, "\r\n", split);
}

std::vector<std::string> string_split_lines(const std::string& text)
{
    std::vector<std::string> split;
    string_split(text, "\r\n", split);
    return split;
}

StringId::StringId(const char* pszString)
{
    id = murmur_hash(pszString, (int)strlen(pszString), 0);
    LookupInstance()[id] = pszString;
}

StringId::StringId(const std::string& str)
{
    id = murmur_hash(str.c_str(), (int)str.length(), 0);
    LookupInstance()[id] = str;
}

const StringId& StringId::operator=(const char* pszString)
{
    id = murmur_hash(pszString, (int)strlen(pszString), 0);
    LookupInstance()[id] = pszString;
    return *this;
}

const StringId& StringId::operator=(const std::string& str)
{
    id = murmur_hash(str.c_str(), (int)str.length(), 0);
    LookupInstance()[id] = str;
    return *this;
}

std::vector<std::vector<std::string>> string_get_string_grid(const std::string& str)
{
    // Gather into array
    std::vector<std::string> lines;
    string_split_lines(str, lines);
    std::vector<std::vector<std::string>> arrayLines;
    for (auto& line : lines)
    {
        std::vector<std::string> vals = string_split(line, "\t, ");
        if (!vals.empty())
        {
            arrayLines.push_back(vals);
        }
    }
    return arrayLines;
}

std::vector<std::vector<int>> string_get_integer_grid(const std::string& str, const std::string& delim)
{
    // Gather into array
    std::vector<std::string> lines;
    string_split_lines(str, lines);
    std::vector<std::vector<int>> arrayLines;
    for (auto& line : lines)
    {
        std::vector<int> vals;
        if (!delim.empty())
        {
            auto strVals = string_split(line, delim.c_str());
            std::transform(strVals.begin(), strVals.end(), back_inserter(vals), [](const std::string& str) { return stoi(str); });
        }
        else
        {
            std::transform(line.begin(), line.end(), back_inserter(vals), [](const char& str) { return str - '0'; });
        }
        if (!vals.empty())
        {
            arrayLines.push_back(vals);
        }
    }
    return arrayLines;
}

std::vector<int> string_get_integers(const std::string& str)
{
    std::vector<int> vals;
    auto strVals = string_split(str, "\t\n\r ,");
    std::transform(strVals.begin(), strVals.end(), back_inserter(vals), [](const std::string& str) { return stoi(str); });
    return vals;
}

std::string string_get_first_line(std::string& text)
{
    std::istringstream iss(text);
    std::string line;
    std::getline(iss, line);
    return line;
}

std::string string_remove_first_line(std::string& text)
{
    auto line = string_get_first_line(text);
    text.erase(0, line.length() + 1); // Remove the first line from the string
    return line;
}

int string_extract_integer(const std::string& text)
{
    try
    {
        std::regex numberRegex("\\d+");
        std::smatch match;
        std::regex_search(text, match, numberRegex);
        return std::stoi(match.str());
    }
    catch (std::exception& ex)
    {
        return 0;
    }
}

} // namespace Zest
