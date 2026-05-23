/*
 * https://stackoverflow.com/questions/5028302/small-logger-class
 * File:   Log.h
 * Author: Alberto Lepe <dev@alepe.com>
 *
 * Created on December 1, 2015, 6:00 PM
 * Modified by cmaughan
 */

#pragma once
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <cassert>

#ifdef WIN32
// A reference to the debug API on windows, to help the logger output in VC.  This is better
// than out to the console sometimes, and as long as you are building on Windows, you are referencing the necessary
// kernel32.dll....
extern "C" {
__declspec(dllimport) void __stdcall OutputDebugStringA(_In_opt_ const char* pszChar);
}
#endif

#undef ERROR

namespace Zest
{

enum class LT
{
    NONE,
    DBG,
    INFO,
    WARNING,
    ERR,
    ALWAYS
};

struct Logger
{
    bool headers = false;
    LT level = LT::WARNING;
    int32_t globalIndent = 0;
    std::vector<uint32_t> indentStack;
    bool lastEmpty = false;
};

extern Logger logger;

class Log
{
public:
    Log()
    {
    }
    Log(LT type, uint32_t indent = 0)
    {
        msglevel = type;
        if (logger.headers && msglevel >= logger.level)
        {
            operator<<("[" + getLabel(type) + "] ");
        }
        if (msglevel >= logger.level)
        {
            for (uint32_t i = 0; i < indent + logger.globalIndent; i++)
            {
                out << " ";
            }
            out << "(T:" << std::this_thread::get_id() << ") ";
        }
    }
    ~Log()
    {
        if (opened)
        {
            out << std::endl;
#ifdef WIN32
            OutputDebugStringA(out.str().c_str());
#else
            std::cout << out.str();
#endif
            logger.lastEmpty = false;
        }
        opened = false;
    }
    template <class T>
    Log& operator<<(const T& msg)
    {
        if (disabled || msglevel < logger.level)
            return *this;
        out << msg;
        opened = true;
        return *this;
    }

    static bool disabled;
private:
    bool opened = false;
    LT msglevel = LT::DBG;
    inline std::string getLabel(LT type)
    {
        std::string label;
        switch (type)
        {
        case LT::DBG:
                label = "DEBUG";
                break;
        case LT::INFO:
                label = "INFO ";
                break;
        case LT::WARNING:
                label = "WARN ";
                break;
        case LT::ERR:
                label = "ERR";
                break;
        case LT::NONE:
                label = "NONE";
                break;
        case LT::ALWAYS:
                label = "ALWAYS";
                break;
        }
        return label;
    }
    std::ostringstream out;
};

class LogIndenter
{
public:
    LogIndenter(uint32_t i)
        : indent(i)
    {
        logger.globalIndent += indent;
    }
    ~LogIndenter()
    {
        logger.globalIndent -= indent;
        assert(logger.globalIndent >= 0);
        logger.globalIndent = std::max(logger.globalIndent, int32_t(0));
        if (!logger.lastEmpty)
        {
#ifdef WIN32
            OutputDebugStringA("\n");
#else
            std::cout << "\n";
#endif
            logger.lastEmpty = true;
        }
    }

private:
    int32_t indent;
};

#ifndef LOG
#define CONCAT_LINE_(x,y) x##y
#define CONCAT_LINE(x,y) CONCAT_LINE_(x, y)
#ifndef NDEBUG
#define LOG_SCOPE(a, b) \
    Zest::Log(Zest::LT::a) << b;        \
    Zest::LogIndenter CONCAT_LINE(LogIndenter,__LINE__)(4);
#define LOG_PUSH_INDENT(a)                 \
    {                                      \
        logger.globalIndent += a;        \
        logger.indentStack.push_back(a); \
    }
#define LOG_POP_INDENT()                                          \
    {                                                             \
        if (!logger.indentStack.empty())                        \
        {                                                         \
            logger.globalIndent -= logger.indentStack.back(); \
            logger.indentStack.pop_back();                      \
        }                                                         \
    }
#define LOG(a, b) Zest::Log(Zest::LT::a) << b
#define LOG_INDENT(a, indent, b) Zest::Log(Zest::LT::a, indent) << b
#else // Release
#define LOG_PUSH_INDENT(a)
#define LOG_POP_INDENT()
#define LOG_SCOPE(a, b)
#define LOG(a, b)
#define LOG_INDENT(a, indent, b)
#endif
#endif

} // namespace Zest
