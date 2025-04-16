// consoleHandler.cpp (Conceptual changes)
#include "consoleHandler.h"
#include <vector> // For ReadConsoleW buffer
#include <locale> // For conversions
#include <codecvt> // For conversions

consoleHandler::consoleHandler() : hConsoleInput(INVALID_HANDLE_VALUE), hConsoleOutput(INVALID_HANDLE_VALUE), consoleAllocated(false) {
    // Check if the program already has a console
    if (!GetConsoleWindow()) {
        // No console attached, allocate a new one
        if (!AllocConsole()) {
            // Non-fatal: Maybe log to DebugOutputString? Don't throw.
             OutputDebugStringA("Failed to allocate console.\n");
             return; // Leave handles invalid
        }
        consoleAllocated = true;
    }

    hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hConsoleInput == INVALID_HANDLE_VALUE || hConsoleOutput == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("Failed to get standard console handles.\n");
        if (consoleAllocated) {
            FreeConsole();
            consoleAllocated = false;
        }
        // Leave handles invalid
        hConsoleInput = INVALID_HANDLE_VALUE;
        hConsoleOutput = INVALID_HANDLE_VALUE;
    } else {
        // Set console output to UTF-8? Optional but often helpful.
        // SetConsoleOutputCP(CP_UTF8);
        // SetConsoleCP(CP_UTF8);
    }
}

consoleHandler::~consoleHandler() {
    // Free the console only if we allocated it
    if (consoleAllocated && GetConsoleWindow()) {
        FreeConsole();
    }
    // Handles are closed automatically by OS if obtained via GetStdHandle
}

// Helper for string conversion (simplistic)
std::wstring string_to_wstring(const std::string& str) {
    // Note: This basic conversion works for ASCII/UTF-8 subset, but proper
    // conversion might need MultiByteToWideChar with CP_UTF8 if input isn't guaranteed ASCII
    return std::wstring(str.begin(), str.end());
}

std::string wstring_to_string(const std::wstring& wstr) {
     // Note: Proper conversion might need WideCharToMultiByte
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    try {
        return converter.to_bytes(wstr);
    } catch (...) {
        // Handle conversion error, maybe return a placeholder or throw
        return "[conversion error]";
    }
}


void consoleHandler::printLine(const std::string& value) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (!isAvailable()) return;
    writeToConsoleInternal(string_to_wstring(value + "\n"));
}

void consoleHandler::print(const std::string& value) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (!isAvailable()) return;
    writeToConsoleInternal(string_to_wstring(value));
}

std::string consoleHandler::input() {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (!isAvailable()) return "";
    return wstring_to_string(readFromConsoleInternal());
}


void consoleHandler::writeToConsoleInternal(const std::wstring& value) {
    DWORD charsWritten;
    if (!WriteConsoleW(hConsoleOutput, value.c_str(), static_cast<DWORD>(value.size()), &charsWritten, nullptr)) {
        DWORD error = GetLastError();
        // Log to debug output as console might be broken
        std::string dbgMsg = "Failed to write to console. Error code: " + std::to_string(error) + "\n";
        OutputDebugStringA(dbgMsg.c_str());
        // Don't throw here to avoid recursion if ErrorHandler uses this
    }
}

std::wstring consoleHandler::readFromConsoleInternal() {
    const DWORD bufferSize = 1024;
    std::vector<wchar_t> buffer(bufferSize); // Use vector
    DWORD charsRead;

    if (!ReadConsoleW(hConsoleInput, buffer.data(), bufferSize -1, &charsRead, nullptr)) { // Read wide chars
        DWORD error = GetLastError();
         std::string dbgMsg = "Failed to read from console. Error code: " + std::to_string(error) + "\n";
         OutputDebugStringA(dbgMsg.c_str());
         return L""; // Return empty on failure
    }

    // Null-terminate just in case (though ReadConsoleW might not?)
    buffer[charsRead] = L'\0';

    // Remove trailing newline characters (\r\n or \n)
    while (charsRead > 0 && (buffer[charsRead - 1] == L'\n' || buffer[charsRead - 1] == L'\r')) {
        charsRead--;
    }

    return std::wstring(buffer.data(), charsRead);
}

consoleHandler conHandler; // Definition