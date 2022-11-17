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

enum class LT
{
    NONE,
    DBG,
    INFO,
    WARNING,
    ERROR
};

struct Logger
{
    bool headers = false;
    LT level = LT::WARNING;
};

extern Logger vklogger;

class Log
{
public:
    Log()
    {
    }
    Log(LT type, uint32_t indent = 0)
    {
        msglevel = type;
        if (vklogger.headers && msglevel >= vklogger.level)
        {
            operator<<("[" + getLabel(type) + "] ");
        }
        out << "(T:" << std::this_thread::get_id() << ") ";

        for (uint32_t i = 0; i < indent; i++)
        {
            out << " ";
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
        }
        opened = false;
    }
    template <class T>
    Log& operator<<(const T& msg)
    {
        if (disabled || msglevel < vklogger.level)
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
        case LT::ERROR:
                label = "ERROR";
                break;
        case LT::NONE:
                label = "NONE";
                break;
        }
        return label;
    }
    std::ostringstream out;
};

#ifndef LOG
#ifdef _DEBUG
#define LOG(a, b) Log(LT::a) << b
#define LOG_INDENT(a, indent, b) Log(LT::a, indent) << b
#else
#define LOG(a, b)
#define LOG_INDENT(a, indent, b)
#endif
#endif

