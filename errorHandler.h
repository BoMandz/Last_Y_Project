#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <string>
#include <atomic>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <windows.h>
#include <functional>
#include "consoleHandler.h"

// Forward declarations of external globals that might need to be accessed
extern std::atomic<bool> isRunning;
extern HWND g_hWnd;

enum class ErrorLevel {
    _INFO,    // Informational message
    _WARNING, // Warning, non-critical issue
    _ERROR,   // Error, operation failed but program can continue
    _FATAL    // Fatal error, program cannot continue
};

struct ErrorRecord {
    std::string message;
    ErrorLevel level;
    std::string timestamp;
    std::string source;
};

class ErrorHandler {
private:
    std::vector<ErrorRecord> errorLog;
    mutable std::mutex errorMutex;
    std::string logFilePath;
    bool consoleOutputEnabled;
    bool fileOutputEnabled;
    bool throwExceptions;
    
    // List of handles to close on fatal error
    std::vector<HANDLE> registeredHandles;
    // List of additional cleanup functions to call on fatal error
    std::vector<std::function<void()>> cleanupFunctions;

    // Get current timestamp as string
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::tm timeInfo;
        localtime_s(&timeInfo, &now_time_t);
        
        char buffer[25];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
        return std::string(buffer);
    }

    // Convert ErrorLevel to string
    std::string levelToString(ErrorLevel level) const {
        switch (level) {
            case ErrorLevel::_INFO:    return "INFO";
            case ErrorLevel::_WARNING: return "WARNING";
            case ErrorLevel::_ERROR:   return "ERROR";
            case ErrorLevel::_FATAL:   return "FATAL";
            default:                  return "UNKNOWN";
        }
    }

    // Write to log file
    void writeToFile(const ErrorRecord& record) {
        if (!fileOutputEnabled || logFilePath.empty()) {
            return;
        }
        bool logFileCleared = false;

        try {
            std::ofstream logFile(logFilePath, std::ios::app);

            if (!logFileCleared) {
                std::ofstream clearFile(logFilePath, std::ios::trunc);
                clearFile.close();
                logFileCleared = true;  
            }

            if (logFile.is_open()) {
                logFile << "[" << record.timestamp << "] [" << levelToString(record.level) 
                        << "] [" << record.source << "] " << record.message << std::endl;
                logFile.close();
            }
        } catch (...) {
            // If we can't write to the log file, at least try to output to console
            if (consoleOutputEnabled) {
                conHandler.printLine("Failed to write to log file: " + logFilePath);
            }
        }
    }

    // Write to console
    void writeToConsole(const ErrorRecord& record) {
        if (!consoleOutputEnabled) {
            return;
        }

        std::string levelStr = levelToString(record.level);
        std::string formattedMessage = "[" + record.timestamp + "] [" + levelStr + "] [" + record.source + "] " + record.message;

        conHandler.printLine(formattedMessage);
    }

    // Handle fatal error - close handles, windows, and perform cleanup
    void handleFatalError(const std::string& message) {
        // Show message box with error details
        MessageBoxA(NULL, message.c_str(), "Fatal Error", MB_OK | MB_ICONERROR);
        
        // Log that we're starting cleanup
        if (consoleOutputEnabled) {
            conHandler.printLine("FATAL ERROR: Performing cleanup before exit...");
        }
        
        // Execute all registered cleanup functions
        for (auto& cleanup : cleanupFunctions) {
            try {
                cleanup();
                // Close all registered handles
                for (HANDLE handle : registeredHandles) {
                    if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
                        CloseHandle(handle);
                    }
                }
                
                // If we have a reference to the main window, destroy it
                if (g_hWnd != NULL && IsWindow(g_hWnd)) {
                    DestroyWindow(g_hWnd);
                }
                
                // Signal that the application should stop running
                if (isRunning.operator bool()) {
                    isRunning.store(false);
                }
            } catch (...) {
                if (consoleOutputEnabled) {
                    conHandler.printLine("Exception during cleanup function execution");
                }
            }
        }
        
        
        
        // Final log message
        if (consoleOutputEnabled) {
            conHandler.printLine("Cleanup complete. Application will exit.");
        }
        
        // Terminate the process with error code
        ExitProcess(1);
    }

public:
    ErrorHandler() : 
        logFilePath("error_log.txt"), 
        consoleOutputEnabled(true),
        fileOutputEnabled(true),
        throwExceptions(false) {}

    // Log an error
    void logError(const std::string& message, ErrorLevel level, const std::string& source) {
        std::lock_guard<std::mutex> lock(errorMutex);
        
        ErrorRecord record{
            message,
            level,
            getCurrentTimestamp(),
            source
        };
        
        errorLog.push_back(record);
        
        if (fileOutputEnabled) {
            writeToFile(record);
        }
        
        if (consoleOutputEnabled) {
            writeToConsole(record);
        }
        
        // For fatal errors, perform cleanup and exit
        if (level == ErrorLevel::_FATAL) {
            handleFatalError(message);
        }
        
        // Optionally throw an exception
        if (throwExceptions && level == ErrorLevel::_ERROR) {
            throw std::runtime_error(message);
        }
    }

    // Convenience methods for different error levels
    void info(const std::string& message, const std::string& source = "System") {
        logError(message, ErrorLevel::_INFO, source);
    }
    
    void warning(const std::string& message, const std::string& source = "System") {
        logError(message, ErrorLevel::_WARNING, source);
    }
    
    void error(const std::string& message, const std::string& source = "System") {
        logError(message, ErrorLevel::_ERROR, source);
    }
    
    void fatal(const std::string& message, const std::string& source = "System") {
        logError(message, ErrorLevel::_FATAL, source);
    }

    // Register a handle to be closed on fatal error
    void registerHandle(HANDLE handle) {
        if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
            std::lock_guard<std::mutex> lock(errorMutex);
            registeredHandles.push_back(handle);
        }
    }
    
    // Unregister a handle (e.g., if it was closed normally)
    void unregisterHandle(HANDLE handle) {
        std::lock_guard<std::mutex> lock(errorMutex);
        for (auto it = registeredHandles.begin(); it != registeredHandles.end(); ) {
            if (*it == handle) {
                if (*it != NULL && *it != INVALID_HANDLE_VALUE) {
                    CloseHandle(*it); // Close the handle
                }
                it = registeredHandles.erase(it); // Remove the handle from the vector
            } else {
                ++it; // Move to the next handle
            }
        }
    }
    
    // Register a cleanup function to be called on fatal error
    void registerCleanupFunction(std::function<void()> cleanup) {
        std::lock_guard<std::mutex> lock(errorMutex);
        cleanupFunctions.push_back(cleanup);
    }

    // Get all errors
    std::vector<ErrorRecord> getAllErrors() const {
        std::lock_guard<std::mutex> lock(errorMutex);
        return errorLog;
    }

    // Clear error log
    void clearErrors() {
        std::lock_guard<std::mutex> lock(errorMutex);
        errorLog.clear();
    }

    // Configuration methods
    void setLogFilePath(const std::string& path) {
        std::lock_guard<std::mutex> lock(errorMutex);
        logFilePath = path;
    }

    void enableConsoleOutput(bool enable) {
        std::lock_guard<std::mutex> lock(errorMutex);
        consoleOutputEnabled = enable;
    }

    void enableFileOutput(bool enable) {
        std::lock_guard<std::mutex> lock(errorMutex);
        fileOutputEnabled = enable;
    }

    void enableExceptionThrowing(bool enable) {
        std::lock_guard<std::mutex> lock(errorMutex);
        throwExceptions = enable;
    }
};

// Global error handler instance
extern ErrorHandler errHandler;

// Macro for easier error reporting with source file and line information
#define LOG_INFO(msg) errHandler.info(msg, __FILE__ ":" + std::to_string(__LINE__))
#define LOG_WARNING(msg) errHandler.warning(msg, __FILE__ ":" + std::to_string(__LINE__))
#define LOG_ERROR(msg) errHandler.error(msg, __FILE__ ":" + std::to_string(__LINE__))
#define LOG_FATAL(msg) errHandler.fatal(msg, __FILE__ ":" + std::to_string(__LINE__))

// Macro for registering handles
#define REGISTER_HANDLE(handle) errHandler.registerHandle(handle)
#define UNREGISTER_HANDLE(handle) errHandler.unregisterHandle(handle)

#endif // ERROR_HANDLER_H