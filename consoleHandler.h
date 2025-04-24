#ifndef CONSOLEHANDLER_H
#define CONSOLEHANDLER_H

#include <string>
#include <mutex>
#include <windows.h>
#include <stdexcept>
#include <sstream> 

class consoleHandler 
{
    mutable std::mutex dataMutex;
    HANDLE hConsoleInput;
    HANDLE hConsoleOutput;
    bool consoleAllocated; 

    std::string formatErrorMessage(const std::string& msg, DWORD errorCode) {
        std::ostringstream oss;
        oss << msg << " Error code: " << errorCode;
        return oss.str();
    }

    void writeToConsoleInternal(const std::wstring& value); 
    std::wstring readFromConsoleInternal(); 

public:
    consoleHandler();
    ~consoleHandler();

    void printLine(const std::string& value);
    void print(const std::string& value);
    std::string input();

    bool isAvailable() const { return hConsoleInput != INVALID_HANDLE_VALUE && hConsoleOutput != INVALID_HANDLE_VALUE; }
};

extern consoleHandler conHandler; // Keep extern if global is intended

#endif 
