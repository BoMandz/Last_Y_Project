#include "shareInfo.h"
#include "screenReader.h"
#include "processSearcher.h"
#include "consoleHandler.h"
#include "errorHandler.h"
//=====================//
#include <windows.h>
#include <winuser.h> 
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#ifndef WINVER
#define WINVER 0x0601 // Windows 7
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Windows 7
#endif

//#pragma comment(lib, "Msimg32.lib")
#undef min
#undef max
#define WM_APP_SET_FOCUS_EDIT (WM_APP + 1)

// Global variables
HINSTANCE g_hInstance;
HWND g_hWnd;
std::atomic<bool> isOverlayVisible(false);
std::atomic<bool> isRunning(true);
RECT brightRect = { 1, 2, 3, 4 };
RECT dragRect = { -1, -1, -1, -1 };
bool isDragging = false;
POINT startPoint = { -1, -1 };
POINT currentPoint = { -1, -1 };

// Input window globals
HWND g_hInputWnd = NULL;
WNDCLASSW g_wcInput = {};
#define INPUT_WINDOW_WIDTH 400
#define INPUT_WINDOW_HEIGHT 200
#define ID_TEXTBOX 101
#define ID_SUBMIT_BUTTON 102
HWND g_hTextBox = NULL;
HWND g_hSubmitButton = NULL;

// Input window for value swich globals
HWND g_hIntValueWnd = NULL;        
WNDCLASSW g_wcIntValue = {};       
#define INT_INPUT_WINDOW_WIDTH 350 
#define INT_INPUT_WINDOW_HEIGHT 150
#define ID_INT_TEXTBOX 201        
#define ID_INT_SUBMIT_BUTTON 202  
HWND g_hIntTextBox = NULL;         
HWND g_hIntSubmitButton = NULL;    

// Forward declarations for the new functions/procedures
LRESULT CALLBACK IntValueWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateIntValueWindow();

void PerformMemoryWrite();

// Function to check if a key is pressed
static bool isKeyPressed(int key) {
    return GetAsyncKeyState(key) & 0x8000;
}

LRESULT CALLBACK InputWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

        case WM_APP_SET_FOCUS_EDIT: {
            if (g_hTextBox) {
                LOG_INFO("Setting focus to general text box."); 
                SetFocus(g_hTextBox);
            } else {
                LOG_WARNING("WM_APP_SET_FOCUS_EDIT received for general input, but g_hTextBox is NULL.");
            }
            break;
        }

        case WM_CREATE: {
            
            g_hTextBox = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 10, INPUT_WINDOW_WIDTH - 20, 30,
                hWnd, (HMENU)ID_TEXTBOX, g_hInstance, nullptr);
            REGISTER_HANDLE(g_hTextBox);

            
            g_hSubmitButton = CreateWindowW(L"BUTTON", L"Submit",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                INPUT_WINDOW_WIDTH / 2 - 40, 50, 80, 30,
                hWnd, (HMENU)ID_SUBMIT_BUTTON, g_hInstance, nullptr);
            REGISTER_HANDLE(g_hSubmitButton);

            SetWindowTextW(hWnd, L"Enter Process Name / General Input");

            PostMessage(hWnd, WM_APP_SET_FOCUS_EDIT, 0, 0);
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SUBMIT_BUTTON) {
                wchar_t buffer[1024];
                GetWindowTextW(g_hTextBox, buffer, 1024);
                std::wstring wideStr(buffer);

                std::string input(wideStr.begin(), wideStr.end()); 
                LOG_INFO("General input window submitted: " + input);
                shareInfo.updateUserInput(input);

                DestroyWindow(hWnd);            

            }
            break;
        }

        case WM_CLOSE:
            // General window closed by user
            LOG_INFO("General input window closed by user.");
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
             LOG_INFO("General input window destroyed.");
             UNREGISTER_HANDLE(g_hSubmitButton);
             UNREGISTER_HANDLE(g_hTextBox);
             UNREGISTER_HANDLE(g_hInputWnd);
             g_hInputWnd = NULL;
            break;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}
// Function to create the input window
void CreateInputWindow() {
    // If the window already exists, just show it
    if (g_hInputWnd != NULL) {
        LOG_INFO("Input window already exists. Showing it.");
        ShowWindow(g_hInputWnd, SW_SHOW);
        SetForegroundWindow(g_hInputWnd);
        return;
    }

    // Register the window class if not already registered
    if (g_wcInput.lpszClassName == NULL) {
        LOG_INFO("Registering input window class.");
        g_wcInput.lpfnWndProc = InputWndProc;
        g_wcInput.hInstance = g_hInstance;
        g_wcInput.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        g_wcInput.lpszClassName = L"InputWindowClass";
        if (!RegisterClassW(&g_wcInput)) {
            LOG_ERROR("Failed to register input window class.");
            return;
        }
    }

    // Calculate position to center the window on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - INPUT_WINDOW_WIDTH) / 2;
    int y = (screenHeight - INPUT_WINDOW_HEIGHT) / 2;

    // Create the window
    g_hInputWnd = CreateWindowExW(
        0, L"InputWindowClass", L"Input Window",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
        x, y, INPUT_WINDOW_WIDTH, INPUT_WINDOW_HEIGHT,
        NULL, NULL, g_hInstance, NULL);

    if (g_hInputWnd) {
        LOG_INFO("Input window created successfully.");
        ShowWindow(g_hInputWnd, SW_SHOW);
        UpdateWindow(g_hInputWnd);
        PostMessage(g_hInputWnd, WM_APP_SET_FOCUS_EDIT, 0, 0);
    } else {
        LOG_ERROR("Failed to create input window.");
    }
}

// Main window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            LOG_INFO("Overlay window created.");
            SetWindowPos(g_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOZORDER);
            break;

        case WM_APP_REQUEST_WRITE_VALUE: 
            LOG_INFO("Received request to open input window for writing value.");
            if (shareInfo.writeValueRequestPending.load()) {
                CreateInputWindow(); // Open the input dialog
            } else {
                LOG_WARNING("WM_APP_REQUEST_WRITE_VALUE received, but writeValueRequestPending is false. Ignoring.");
            }
            break;

        case WM_ERASEBKGND:
            return 1;

            case WM_PAINT: {
                if (isOverlayVisible.load()) {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hWnd, &ps);
            
                    // Get full client area
                    RECT clientRect;
                    GetClientRect(hWnd, &clientRect);
                    
                    // Debugging: Log the paint area
                    LOG_INFO("Painting area: " + 
                             std::to_string(clientRect.right) + "x" + 
                             std::to_string(clientRect.bottom));
            
                    // Double buffering to prevent flickering
                    HDC memDC = CreateCompatibleDC(hdc);
                    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
                    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
                    // Clear with semi-transparent black background
                    HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
                    FillRect(memDC, &clientRect, hBlackBrush);
                    DeleteObject(hBlackBrush);
            
                    // Draw bright rectangle if active
                    if (brightRect.left >= 0 && brightRect.top >= 0 && 
                        brightRect.right >= 0 && brightRect.bottom >= 0) {
                        HBRUSH hBrightBrush = CreateSolidBrush(RGB(200, 200, 200));
                        FillRect(memDC, &brightRect, hBrightBrush);
                        DeleteObject(hBrightBrush);
            
                        // Draw border around bright rectangle
                        HPEN hBorderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                        HPEN hOldPen = (HPEN)SelectObject(memDC, hBorderPen);
                        MoveToEx(memDC, brightRect.left, brightRect.top, NULL);
                        LineTo(memDC, brightRect.right, brightRect.top);
                        LineTo(memDC, brightRect.right, brightRect.bottom);
                        LineTo(memDC, brightRect.left, brightRect.bottom);
                        LineTo(memDC, brightRect.left, brightRect.top);
                        SelectObject(memDC, hOldPen);
                        DeleteObject(hBorderPen);
                    }
            
                    // Draw drag rectangle if active
                    if (isDragging) {
                        HBRUSH hDragBrush = CreateSolidBrush(RGB(100, 100, 255));  // Blue tint for visibility
                        FillRect(memDC, &dragRect, hDragBrush);
                        DeleteObject(hDragBrush);
            
                        // Draw border around drag rectangle
                        HPEN hDragPen = CreatePen(PS_DASH, 1, RGB(255, 255, 0));  // Yellow dashed
                        HPEN hOldPen = (HPEN)SelectObject(memDC, hDragPen);
                        MoveToEx(memDC, dragRect.left, dragRect.top, NULL);
                        LineTo(memDC, dragRect.right, dragRect.top);
                        LineTo(memDC, dragRect.right, dragRect.bottom);
                        LineTo(memDC, dragRect.left, dragRect.bottom);
                        LineTo(memDC, dragRect.left, dragRect.top);
                        SelectObject(memDC, hOldPen);
                        DeleteObject(hDragPen);
                    }
            
                    // Copy the memory DC to the screen
                    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            
                    // Cleanup
                    SelectObject(memDC, oldBitmap);
                    DeleteObject(memBitmap);
                    DeleteDC(memDC);
            
                    EndPaint(hWnd, &ps);
                } else {
                    // Handle case when overlay is not visible (if needed)
                    PAINTSTRUCT ps;
                    BeginPaint(hWnd, &ps);
                    EndPaint(hWnd, &ps);
                }
                break;
            }

        case WM_LBUTTONDOWN: {
            POINT clickPoint = { LOWORD(lParam), HIWORD(lParam) };
            startPoint = clickPoint;
            currentPoint = clickPoint;
            isDragging = true;
            SetCapture(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
            break;
        }

        case WM_MOUSEMOVE: {
            if (isDragging) {
                currentPoint = { LOWORD(lParam), HIWORD(lParam) };
        
                // Get screen dimensions
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                int maxX = clientRect.right - 1;
                int maxY = clientRect.bottom - 1;
        
                // Cast min/max results to int before clamping
                dragRect.left = std::clamp(
                    static_cast<int>(std::min(startPoint.x, currentPoint.x)), 0, maxX
                );
                dragRect.top = std::clamp(
                    static_cast<int>(std::min(startPoint.y, currentPoint.y)), 0, maxY
                );
                dragRect.right = std::clamp(
                    static_cast<int>(std::max(startPoint.x, currentPoint.x)), 0, maxX
                );
                dragRect.bottom = std::clamp(
                    static_cast<int>(std::max(startPoint.y, currentPoint.y)), 0, maxY
                );
        
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (isDragging) {
                currentPoint = { LOWORD(lParam), HIWORD(lParam) };
        
                // Get screen dimensions
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                int maxX = clientRect.right - 1;
                int maxY = clientRect.bottom - 1;
        
                // Cast min/max results to int before clamping
                brightRect.left = std::clamp(
                    static_cast<int>(std::min(startPoint.x, currentPoint.x)), 0, maxX
                );
                brightRect.top = std::clamp(
                    static_cast<int>(std::min(startPoint.y, currentPoint.y)), 0, maxY
                );
                brightRect.right = std::clamp(
                    static_cast<int>(std::max(startPoint.x, currentPoint.x)), 0, maxX
                );
                brightRect.bottom = std::clamp(
                    static_cast<int>(std::max(startPoint.y, currentPoint.y)), 0, maxY
                );
        
                isDragging = false;
                ReleaseCapture();
                InvalidateRect(hWnd, nullptr, TRUE);
        
                shareInfo.update(isOverlayVisible.load(), isRunning.load(), brightRect, isDragging, g_hWnd);
            }
            break;
        }

        case WM_DESTROY:
            LOG_INFO("Overlay window destroyed.");
            isRunning.store(false);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

void PerformMemoryWrite() {
    if (!shareInfo.writeValueInputReady.load()) {
        LOG_WARNING("PerformMemoryWrite called, but writeValueInputReady is false.");
        return;
    }

    DWORD pid = shareInfo.getThePIDOfProsses();
    std::vector<uintptr_t> addresses = shareInfo.getVoidPoitersFinaly();
    int newValue = shareInfo.getValueToWrite(); // Gets the value set by IntValueWndProc

    // Reset the ready flag immediately.
    shareInfo.writeValueInputReady.store(false);

    LOG_INFO("PerformMemoryWrite triggered with value: " + std::to_string(newValue)); // Added log

    if (pid == 0) {
        // ... (rest of error handling remains the same) ...
        LOG_ERROR("Cannot write value: Target Process ID is 0.");
        MessageBoxW(NULL, L"Cannot write value: Target Process ID is 0.", L"Write Error", MB_OK | MB_ICONERROR);
        return;
    }
    if (addresses.empty()) {
       // ... (rest of error handling remains the same) ...
        LOG_ERROR("Cannot write value: Address list is empty.");
         MessageBoxW(NULL, L"Cannot write value: No memory addresses found to write to.", L"Write Error", MB_OK | MB_ICONERROR);
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
     if (hProcess == NULL) {
         // ... (rest of error handling remains the same) ...
        LOG_ERROR("Failed to open target process (PID: " + std::to_string(pid) + ") with write permissions. Error code: " + std::to_string(GetLastError()));
         MessageBoxW(NULL, (L"Failed to open target process (PID: " + std::to_wstring(pid) + L") with write permissions.\nError code: " + std::to_wstring(GetLastError())).c_str(), L"Write Error", MB_OK | MB_ICONERROR);
        return;
     }
     REGISTER_HANDLE(hProcess);

    // ... (actual writing loop remains the same) ...
    LOG_INFO("Attempting to write value " + std::to_string(newValue) + " to " + std::to_string(addresses.size()) + " address(es) in PID " + std::to_string(pid) + "...");
    int writeSuccessCount = 0;
    int writeFailCount = 0;

    for (uintptr_t addr : addresses) {
        SIZE_T bytesWritten = 0;
        BOOL success = WriteProcessMemory(hProcess, (LPVOID)addr, &newValue, sizeof(newValue), &bytesWritten);
        if (success && bytesWritten == sizeof(newValue)) {
            // ... success logging ...
            writeSuccessCount++;
        } else {
            // ... failure logging ...
            writeFailCount++;
        }
    }

    LOG_INFO("Write operation complete. Success: " + std::to_string(writeSuccessCount) + ", Failed: " + std::to_string(writeFailCount));
    std::wstring summary = L"Memory Write Result:\nValue: " + std::to_wstring(newValue) + L"\nSuccess: " + std::to_wstring(writeSuccessCount) + L"\nFailed: " + std::to_wstring(writeFailCount); // Added value to summary
    MessageBoxW(NULL, summary.c_str(), L"Write Operation Complete", MB_OK | (writeFailCount > 0 ? MB_ICONWARNING : MB_ICONINFORMATION));


    CloseHandle(hProcess);
    UNREGISTER_HANDLE(hProcess);
}

LRESULT CALLBACK IntValueWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            
            g_hIntTextBox = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER, // Always ES_NUMBER
                10, 10, INT_INPUT_WINDOW_WIDTH - 20, 30,
                hWnd, (HMENU)ID_INT_TEXTBOX, g_hInstance, nullptr);
            REGISTER_HANDLE(g_hIntTextBox);

            // Create submit button
            g_hIntSubmitButton = CreateWindowW(L"BUTTON", L"Write Value", // Specific button text
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                INT_INPUT_WINDOW_WIDTH / 2 - 50, 50, 100, 30, // Centered
                hWnd, (HMENU)ID_INT_SUBMIT_BUTTON, g_hInstance, nullptr);
            REGISTER_HANDLE(g_hIntSubmitButton);

            SetWindowTextW(hWnd, L"Enter Integer Value to Write"); // Specific title

            // Post message to set focus reliably
            PostMessage(hWnd, WM_APP_SET_FOCUS_EDIT, 0, 0);
            break;
        }

        // Handle the custom focus message (shared with the other input window is fine)
        case WM_APP_SET_FOCUS_EDIT: {
             if (g_hIntTextBox) { // Check the correct text box handle
                 LOG_INFO("Setting focus to integer text box.");
                 SetFocus(g_hIntTextBox);
             } else {
                 LOG_WARNING("WM_APP_SET_FOCUS_EDIT received for IntValueWnd, but g_hIntTextBox is NULL.");
             }
             break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SUBMIT_BUTTON) {
                wchar_t buffer[1024];
                GetWindowTextW(g_hTextBox, buffer, 1024);
                std::wstring wideStr(buffer);

                // Check if we are in "write value" mode << --- PROBLEM AREA ---
                if (shareInfo.writeValueRequestPending.load()) { // <<< THIS IS THE ISSUE
                    try {
                        // It *tries* to convert to an integer because the flag might be true
                        int newValue = std::stoi(wideStr);
                        LOG_INFO("User submitted new value: " + std::to_string(newValue));
                        shareInfo.setValueToWrite(newValue);
                        shareInfo.writeValueInputReady.store(true);
                        shareInfo.writeValueRequestPending.store(false);
                        DestroyWindow(hWnd);

                    } catch (const std::invalid_argument& e) {
                        // If you type non-integers, it fails here and shows the integer error!
                        MessageBoxW(hWnd, L"Invalid input. Please enter an integer.", L"Input Error", MB_OK | MB_ICONWARNING);
                        SetFocus(g_hTextBox); // Put focus back
                    } catch (const std::out_of_range& e) {
                        MessageBoxW(hWnd, L"Input out of range for an integer.", L"Input Error", MB_OK | MB_ICONWARNING);
                        SetFocus(g_hTextBox); // Put focus back
                    }
                } else {
                    // This 'else' block is intended for Ctrl+Alt+X, but might not be reached
                    // if writeValueRequestPending is incorrectly true.
                    std::string input(wideStr.begin(), wideStr.end());
                    LOG_INFO("User submitted input: " + input);
                    shareInfo.updateUserInput(input);
                    SetWindowTextW(g_hTextBox, L""); // Clear
                    // DestroyWindow(hWnd); // Optionally close
                }
            }
            break;
        }

        case WM_CLOSE:
             // User closed the window, simply destroy it. The write won't happen.
             LOG_INFO("Integer input window closed by user.");
             shareInfo.writeValueInputReady.store(false); // Ensure flag is false if closed prematurely
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            LOG_INFO("Integer input window destroyed.");
            UNREGISTER_HANDLE(g_hIntSubmitButton);
            UNREGISTER_HANDLE(g_hIntTextBox);
            UNREGISTER_HANDLE(g_hIntValueWnd); // Unregister window handle itself
            g_hIntValueWnd = NULL; // Mark as destroyed
            break;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateIntValueWindow() {
    // If the window already exists, just show it and bring it to front
    if (g_hIntValueWnd != NULL) {
        LOG_INFO("Integer input window already exists. Showing and activating it.");
        ShowWindow(g_hIntValueWnd, SW_RESTORE);
        SetForegroundWindow(g_hIntValueWnd);
        // Re-post focus message just in case
        PostMessage(g_hIntValueWnd, WM_APP_SET_FOCUS_EDIT, 0, 0);
        return;
    }

    // Register the window class if not already registered
    if (g_wcIntValue.lpszClassName == NULL) {
        LOG_INFO("Registering integer input window class.");
        g_wcIntValue.lpfnWndProc = IntValueWndProc; // Use the new procedure
        g_wcIntValue.hInstance = g_hInstance;
        g_wcIntValue.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        g_wcIntValue.lpszClassName = L"IntValueWindowClass"; // New class name
        if (!RegisterClassW(&g_wcIntValue)) {
            LOG_ERROR("Failed to register integer input window class.");
            return;
        }
    }

    // Calculate position to center the window on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - INT_INPUT_WINDOW_WIDTH) / 2;
    int y = (screenHeight - INT_INPUT_WINDOW_HEIGHT) / 2;

    // Create the window
    g_hIntValueWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"IntValueWindowClass", // Use the new class name
        L"Enter Integer Value", // Initial Title (can be set again in WM_CREATE)
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, INT_INPUT_WINDOW_WIDTH, INT_INPUT_WINDOW_HEIGHT,
        NULL, NULL, g_hInstance, NULL);
    REGISTER_HANDLE(g_hIntValueWnd); // Register handle right after creation

    if (g_hIntValueWnd) {
        LOG_INFO("Integer input window created successfully.");
        ShowWindow(g_hIntValueWnd, SW_SHOW);
        UpdateWindow(g_hIntValueWnd);
        SetForegroundWindow(g_hIntValueWnd); // Bring the window to the front
        // Focus is handled via PostMessage from WM_CREATE
    } else {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to create integer input window. Error code: " + std::to_string(error));
         UNREGISTER_HANDLE(g_hIntValueWnd); // Unregister if creation failed
    }
}

// Function to toggle the overlay visibility
static void ToggleOverlay() {
    if (isOverlayVisible) {
        LOG_INFO("Overlay hidden.");
        ShowWindow(g_hWnd, SW_HIDE);
        isOverlayVisible.store(false);
    } else {
        LOG_INFO("Overlay shown.");
        
        // Get screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        // Ensure fullscreen coverage
        SetWindowPos(g_hWnd, HWND_TOPMOST, 
                    0, 0,                      // Position at top-left
                    screenWidth, screenHeight,  // Full screen size
                    SWP_SHOWWINDOW | SWP_NOACTIVATE);
        
        // Force redraw
        InvalidateRect(g_hWnd, NULL, TRUE);
        UpdateWindow(g_hWnd);
        
        isOverlayVisible.store(true);
    }
    shareInfo.update(isOverlayVisible.load(), isRunning.load(), brightRect, isDragging, g_hWnd);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    REGISTER_HANDLE(g_hInstance);

    // Start threads
    std::thread screenReaderThread([]() { screenReaderLoop(true); });
    std::thread processSearcherThread([]() { SearchForProcessLoop(true); });

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayClass";
    
    if (!RegisterClassW(&wc)) {
        LOG_ERROR("Failed to register window class");
        return 1;
    }

    // Create window with proper styles
    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"OverlayClass", 
        L"Overlay",
        WS_POPUP,
        0, 0, 
        GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, 
        nullptr, 
        hInstance, 
        nullptr);

    if (!g_hWnd) {
        LOG_ERROR("Failed to create window");
        return 1;
    }

    // Set transparency properties
    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 128, LWA_ALPHA);

    // Show the window immediately at startup
    isOverlayVisible.store(true);
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    // Force initial paint
    InvalidateRect(g_hWnd, NULL, TRUE);
    UpdateWindow(g_hWnd);

    // Main message loop
    MSG msg = {};
    while (isRunning.load()) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                isRunning.store(false);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Hotkey handling
            if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x41)) { // Ctrl+Alt+A
                ToggleOverlay();
                Sleep(300); // Debounce
            }
            else if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x58)) { // Ctrl+Alt+X
                // --> Add flag clearing here <--
                shareInfo.writeValueRequestPending.store(false);
                shareInfo.writeValueInputReady.store(false);
                CreateInputWindow();
                Sleep(300); // Debounce
            } else if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x53)) { // Ctrl+Alt+S
                LOG_FATAL("Exit requested via hotkey (Ctrl+Alt+S)."); // Use INFO or FATAL consistently
                isRunning.store(false);
                break;
            }
            // Brief sleep to prevent CPU overuse
            Sleep(10);
        }
    }

    // Cleanup
    processSearcherThread.join();
    screenReaderThread.join();
    
    return 0;
}