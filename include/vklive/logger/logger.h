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

#ifdef WIN32
// A reference to the debug API on windows, to help the logger output in VC.  This is better
// than out to the console sometimes, and as long as you are building on Windows, you are referencing the necessary
// kernel32.dll....
extern "C" {
__declspec(dllimport) void __stdcall OutputDebugStringA(_In_opt_ const char* pszChar);
}
#endif

#undef ERROR

struct Logger
{
    bool headers = false;
    uint32_t level = 0;
    uint32_t globalIndent = 0;
    std::vector<uint32_t> indentStack;
    bool lastEmpty = false;
};

extern Logger vklogger;

class Log
{
public:
    Log()
    {
    }
    Log(uint32_t l, uint32_t indent = 0)
    {
        level = l;
        // out << "(T:" << std::this_thread::get_id() << ") ";

        if (level <= vklogger.level)
        {
            for (uint32_t i = 0; i < indent + vklogger.globalIndent; i++)
            {
                out << " ";
            }
        }
    }
    ~Log()
    {
        if (opened)
        {
            if (level <= vklogger.level)
            {
                out << std::endl;
#ifdef WIN32
                OutputDebugStringA(out.str().c_str());
#else
                std::cout << out.str();
#endif
                vklogger.lastEmpty = false;
            }
        }
        opened = false;
    }
    template <class T>
    Log& operator<<(const T& msg)
    {
        if (disabled || level > vklogger.level)
            return *this;
        out << msg;
        opened = true;
        return *this;
    }

    static bool disabled;

private:
    bool opened = false;
    uint32_t level = 0;
    std::ostringstream out;
};

enum LOGLEVEL
{
    ERROR = 0,
    WARNING = 5,
    INFO = 8,
    DBG = 10,
};

class LogIndenter
{
public:
    LogIndenter(uint32_t level, uint32_t i)
        : indent(i)
        , level(level)
    {
        vklogger.globalIndent += indent;
    }
    ~LogIndenter()
    {
        vklogger.globalIndent -= indent;
        if (!vklogger.lastEmpty)
        {
#ifdef WIN32
            OutputDebugStringA("\n");
#else
            std::cout << "\n";
#endif
            vklogger.lastEmpty = true;
        }
    }

private:
    uint32_t indent;
    uint32_t level;
};

#ifndef LOG
#define CONCAT_LINE_(x,y) x##y
#define CONCAT_LINE(x,y) CONCAT_LINE_(x, y)
#ifdef _DEBUG
#define LOG_SCOPE(a, b) \
    Log(a) << b;        \
    LogIndenter CONCAT_LINE(LogIndenter,__LINE__)(a, 4);
#define LOG_PUSH_INDENT(a)                 \
    {                                      \
        vklogger.globalIndent += a;        \
        vklogger.indentStack.push_back(a); \
    }
#define LOG_POP_INDENT()                                          \
    {                                                             \
        if (!vklogger.indentStack.empty())                        \
        {                                                         \
            vklogger.globalIndent -= vklogger.indentStack.back(); \
            vklogger.indentStack.pop_back();                      \
        }                                                         \
    }
#define LOG(a, b) Log(a) << b
#define LOG_INDENT(a, indent, b) Log(a, indent) << b
#else
#define LOG_PUSH_INDENT(a)
#define LOG_POP_INDENT()
#define LOG_SCOPE(a, b)
#define LOG(a, b)
#define LOG_INDENT(a, indent, b)
#endif
#endif
