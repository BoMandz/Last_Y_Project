#ifndef CONSOLEHANDLER_H
#define CONSOLEHANDLER_H

#include <string>
#include <mutex>
#include <windows.h>
#include <stdexcept>

struct consoleHandler
{
    mutable std::mutex dataMutex;
    HANDLE hConsoleInput;
    HANDLE hConsoleOutput;

    consoleHandler() : hConsoleInput(GetStdHandle(STD_INPUT_HANDLE)), hConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE)) 
    {
        if (hConsoleInput == INVALID_HANDLE_VALUE || hConsoleOutput == INVALID_HANDLE_VALUE) {
            //cant get handle/s
            throw std::runtime_error("Failed to get console handle");
        }
    }


    void The_Output_New_Line(const std::string& value) {
        std::lock_guard<std::mutex> lock(dataMutex);
        WriteConsoleOutput(value);
        WriteConsoleOutput("\n");
    }

    void The_Output(const std::string& value) {
        std::lock_guard<std::mutex> lock(dataMutex);
        WriteConsoleOutput(value);
    }

    std::string The_Input() {
        std::lock_guard<std::mutex> lock(dataMutex);
        return ReadConsoleInput();
    }

private:
    void WriteConsoleOutput(const std::string& value) {
        DWORD charsWritten;
        if (!WriteConsole(hConsoleOutput, value.c_str(), static_cast<DWORD>(value.size()), &charsWritten, nullptr)) {
            // Handle error: Unable to write to console
            throw std::runtime_error("Failed to write to console");
        }
    }

    std::string ReadConsoleInput() {
        const DWORD bufferSize = 1024;
        char buffer[bufferSize];
        DWORD charsRead;

        // Read input from the console
        if (!ReadConsole(hConsoleInput, buffer, bufferSize, &charsRead, nullptr)) {
            // Handle error: Unable to read from console
            throw std::runtime_error("Failed to read from console");
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