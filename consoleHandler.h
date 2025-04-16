// consoleHandler.h (Partial improvements)
#ifndef CONSOLEHANDLER_H
#define CONSOLEHANDLER_H

#include <string>
#include <mutex>
#include <windows.h>
#include <stdexcept>
#include <sstream> // Keep for potential fallback string conversion

class consoleHandler // Use class instead of struct for better encapsulation
{
    mutable std::mutex dataMutex;
    HANDLE hConsoleInput;
    HANDLE hConsoleOutput;
    bool consoleAllocated; // Track if we allocated it

    // Simplified error formatting internal to the class
    std::string formatErrorMessage(const std::string& msg, DWORD errorCode) {
        std::ostringstream oss;
        oss << msg << " Error code: " << errorCode;
        // Optionally add FormatMessage logic here for richer error descriptions
        return oss.str();
    }

    void writeToConsoleInternal(const std::wstring& value); // Use wstring
    std::wstring readFromConsoleInternal(); // Use wstring

public:
    consoleHandler();
    ~consoleHandler();

    // Public interface uses std::string for convenience, converts internally
    void printLine(const std::string& value);
    void print(const std::string& value);
    std::string input();

    // Allow disabling if needed (e.g., if console init fails gracefully)
    bool isAvailable() const { return hConsoleInput != INVALID_HANDLE_VALUE && hConsoleOutput != INVALID_HANDLE_VALUE; }
};

extern consoleHandler conHandler; // Keep extern if global is intended

#endif // CONSOLEHANDLER_H
