#ifndef CONSOLEHANDLER_H
#define CONSOLEHANDLER_H

#include <string>
#include <mutex>
#include <windows.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <comutil.h>



struct consoleHandler
{
    mutable std::mutex dataMutex;
    HANDLE hConsoleInput;
    HANDLE hConsoleOutput;

    //Gets everything returns it as a string
    template <typename T>
    std::string valueToString(const T& value) {
        if constexpr (std::is_integral_v<T>) {
            return std::to_string(value);
        } 
        else if constexpr (std::is_floating_point_v<T>) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(6) << value;
            return stream.str();
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return value;
        }
        else if constexpr (std::is_same_v<T, BOOL>) {
            return value ? "TRUE" : "FALSE";
        }
        else if constexpr (std::is_same_v<T, LPCTSTR>) {
            return value ? std::string(value) : "NULL";
        }
        else {
            // Fallback for complex types
            std::ostringstream stream;
            stream << value;
            return stream.str();
        }
    }
    
    consoleHandler() {
        // Check if the program already has a console
        if (!GetConsoleWindow()) {
            // No console attached, allocate a new one
            if (!AllocConsole()) {
                throw std::runtime_error("Failed to allocate console");
            }
        }

        // Get the handles for the console
        hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
        hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

        // Check if the handles are valid
        if (hConsoleInput == INVALID_HANDLE_VALUE || hConsoleOutput == INVALID_HANDLE_VALUE) {
            FreeConsole(); // Clean up if handles are invalid
            throw std::runtime_error("Failed to get console handle");
        }
    }

    ~consoleHandler() {
        // Clean up: Free the console when the object is destroyed
        if (GetConsoleWindow()) {
            FreeConsole();
        }
    }

    void printLine(const std::string& value) {
        std::lock_guard<std::mutex> lock(dataMutex);
        WriteConsoleOutput(value);
        WriteConsoleOutput("\n");
    }

    void print(const std::string& value) {
        std::lock_guard<std::mutex> lock(dataMutex);
        WriteConsoleOutput(value);
    }

    std::string input() {
        std::lock_guard<std::mutex> lock(dataMutex);
        return ReadConsoleInput();
    }

private:
    void WriteConsoleOutput(const std::string& value) {
        DWORD charsWritten;
        if (!WriteConsoleA(hConsoleOutput, value.c_str(), static_cast<DWORD>(value.size()), &charsWritten, nullptr)) {
            DWORD error = GetLastError();
            throw std::runtime_error("Failed to write to console. Error code: " + std::to_string(error));
        }
    }

    std::string ReadConsoleInput() {
        const DWORD bufferSize = 1024;
        char buffer[bufferSize];
        DWORD charsRead;

        // Read input from the console
        if (!ReadConsoleA(hConsoleInput, buffer, bufferSize, &charsRead, nullptr)) {
            DWORD error = GetLastError();
            throw std::runtime_error("Failed to read from console. Error code: " + std::to_string(error));
        }

        // Remove the newline character at the end (if present)
        if (charsRead >= 2 && buffer[charsRead - 2] == '\r' && buffer[charsRead - 1] == '\n') {
            charsRead -= 2;
        } else if (charsRead >= 1 && (buffer[charsRead - 1] == '\r' || buffer[charsRead - 1] == '\n')) {
            charsRead -= 1;
        }

        return std::string(buffer, charsRead);
    }

};

extern consoleHandler conHandler;

#endif