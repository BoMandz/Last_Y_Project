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

void PerformMemoryWrite();

// Function to check if a key is pressed
static bool isKeyPressed(int key) {
    return GetAsyncKeyState(key) & 0x8000;
}

// Input window procedure - defined before it's used
LRESULT CALLBACK InputWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create text box
            g_hTextBox = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER, // Added ES_NUMBER for integer input
                10, 10, INPUT_WINDOW_WIDTH - 20, 30,
                hWnd, (HMENU)ID_TEXTBOX, g_hInstance, nullptr);
            REGISTER_HANDLE(g_hTextBox); // Register handle

            // Create submit button
            g_hSubmitButton = CreateWindowW(L"BUTTON", L"Submit",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                INPUT_WINDOW_WIDTH / 2 - 40, 50, 80, 30,
                hWnd, (HMENU)ID_SUBMIT_BUTTON, g_hInstance, nullptr);
            REGISTER_HANDLE(g_hSubmitButton); // Register handle

            // Set focus to the text box
            SetFocus(g_hTextBox);

            // Change window title based on why it was opened
            if (shareInfo.writeValueRequestPending.load()) {
                SetWindowTextW(hWnd, L"Enter New Value");
            } else {
                SetWindowTextW(hWnd, L"Enter Process Name"); // Or generic "Input Window"
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SUBMIT_BUTTON) {
                wchar_t buffer[1024];
                GetWindowTextW(g_hTextBox, buffer, 1024);
                std::wstring wideStr(buffer);

                // Check if we are in "write value" mode
                if (shareInfo.writeValueRequestPending.load()) {
                    try {
                        int newValue = std::stoi(wideStr); // Convert input to int
                        LOG_INFO("User submitted new value: " + std::to_string(newValue));
                        shareInfo.setValueToWrite(newValue);      // Store the value
                        shareInfo.writeValueInputReady.store(true); // Signal input is ready
                        shareInfo.writeValueRequestPending.store(false); // Request handled

                        // --- Option A: Trigger write immediately from UI thread ---
                        // PostMessage(shareInfo.g_hWnd, WM_APP_PERFORM_WRITE, 0, 0);
                        // DestroyWindow(hWnd); // Close after submit

                        // --- Option B: Let another mechanism poll writeValueInputReady ---
                        // Just close the window, the writer will pick it up
                        DestroyWindow(hWnd);

                    } catch (const std::invalid_argument& e) {
                        MessageBoxW(hWnd, L"Invalid input. Please enter an integer.", L"Input Error", MB_OK | MB_ICONWARNING);
                        SetFocus(g_hTextBox); // Put focus back
                    } catch (const std::out_of_range& e) {
                        MessageBoxW(hWnd, L"Input out of range for an integer.", L"Input Error", MB_OK | MB_ICONWARNING);
                        SetFocus(g_hTextBox); // Put focus back
                    }
                } else {
                    // Original behavior: Update process name/general input
                    std::string input(wideStr.begin(), wideStr.end()); // Simple conversion (consider UTF-8 safety)
                    LOG_INFO("User submitted input: " + input);
                    shareInfo.updateUserInput(input);
                    // Clear the text box or close window?
                    SetWindowTextW(g_hTextBox, L""); // Clear
                    // DestroyWindow(hWnd); // Optionally close after general submit too
                }
            }
            break;
        }

        case WM_CLOSE:
            // If the user closes the window while a write was requested, cancel it.
            if (shareInfo.writeValueRequestPending.load()) {
                LOG_INFO("User closed input window, cancelling write request.");
                shareInfo.writeValueRequestPending.store(false);
                shareInfo.writeValueInputReady.store(false); // Ensure not ready
            }
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
             UNREGISTER_HANDLE(g_hSubmitButton); // Unregister handles
             UNREGISTER_HANDLE(g_hTextBox);
             UNREGISTER_HANDLE(g_hInputWnd); // Unregister window handle itself
            g_hInputWnd = NULL; // Mark as destroyed
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

                // Double buffering
                HDC memDC = CreateCompatibleDC(hdc);
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
                SelectObject(memDC, memBitmap);

                // Draw to memory DC
                HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(memDC, &clientRect, hBlackBrush);
                DeleteObject(hBlackBrush);

                if (brightRect.left >= 0 && brightRect.top >= 0 && brightRect.right >= 0 && brightRect.bottom >= 0) {
                    HBRUSH hBrightBrush = CreateSolidBrush(RGB(200, 200, 200));
                    FillRect(memDC, &brightRect, hBrightBrush);
                    DeleteObject(hBrightBrush);
                }

                if (isDragging) {
                    HBRUSH hDragBrush = CreateSolidBrush(RGB(100, 100, 100));
                    FillRect(memDC, &dragRect, hDragBrush);
                    DeleteObject(hDragBrush);
                }

                // Copy to screen
                BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

                // Cleanup
                DeleteObject(memBitmap);
                DeleteDC(memDC);

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
    // Double-check if input is actually ready
    if (!shareInfo.writeValueInputReady.load()) {
        LOG_WARNING("PerformMemoryWrite called, but writeValueInputReady is false.");
        return;
    }

    DWORD pid = shareInfo.getThePIDOfProsses();
    std::vector<uintptr_t> addresses = shareInfo.getVoidPoitersFinaly();
    int newValue = shareInfo.getValueToWrite();

    if (pid == 0) {
        LOG_ERROR("Cannot write value: Target Process ID is 0.");
        shareInfo.writeValueInputReady.store(false); // Reset flag even on error
        return;
    }
    if (addresses.empty()) {
        LOG_ERROR("Cannot write value: Address list is empty.");
        shareInfo.writeValueInputReady.store(false); // Reset flag
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (hProcess == NULL) {
        LOG_ERROR("Failed to open target process with write permissions. Error code: " + std::to_string(GetLastError()));
        shareInfo.writeValueInputReady.store(false); // Reset flag
        return;
    }
     REGISTER_HANDLE(hProcess); // Register for cleanup

    LOG_INFO("Attempting to write value " + std::to_string(newValue) + " to " + std::to_string(addresses.size()) + " address(es)...");
    int writeSuccessCount = 0;
    int writeFailCount = 0;

    for (uintptr_t addr : addresses) {
        SIZE_T bytesWritten = 0;
        BOOL success = WriteProcessMemory(hProcess, (LPVOID)addr, &newValue, sizeof(newValue), &bytesWritten);

        if (success && bytesWritten == sizeof(newValue)) {
            std::stringstream ss;
            ss << "Successfully wrote to address 0x" << std::hex << addr;
            LOG_INFO(ss.str());
            writeSuccessCount++;
        } else {
            std::stringstream ss;
            ss << "Failed to write to address 0x" << std::hex << addr << ". Error code: " << GetLastError();
            LOG_ERROR(ss.str());
            writeFailCount++;
        }
    }
    LOG_INFO("Write operation complete. Success: " + std::to_string(writeSuccessCount) + ", Failed: " + std::to_string(writeFailCount));

    CloseHandle(hProcess);
     UNREGISTER_HANDLE(hProcess); // Unregister after normal close

    // Reset the flag now that the write is done (or attempted)
    shareInfo.writeValueInputReady.store(false);
}

// Function to toggle the overlay visibility
static void ToggleOverlay() {
    if (isOverlayVisible) {
        LOG_INFO("Overlay hidden.");
        ShowWindow(g_hWnd, SW_HIDE);
        isOverlayVisible.store(false);
    } else {
        LOG_INFO("Overlay shown.");
        ShowWindow(g_hWnd, SW_SHOW);
        UpdateWindow(g_hWnd);
        isOverlayVisible.store(true);
    }

    // Update shareInfo
    shareInfo.update(isOverlayVisible.load(), isRunning.load(), brightRect, isDragging, g_hWnd);
}

// WinMain function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInstance = hInstance;
    REGISTER_HANDLE(g_hInstance);
    REGISTER_HANDLE(hInstance);
    REGISTER_HANDLE(g_hWnd);
    REGISTER_HANDLE(g_hInputWnd);
    REGISTER_HANDLE(g_hTextBox);
    REGISTER_HANDLE(g_hSubmitButton);

    // lamda func is needed cuz normal ones dont work
    std::thread screenReaderThread([]() { screenReaderLoop(true); });
    std::thread processSearcherThread([]() { SearchForProcessLoop(true); });

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = NULL; 
    wc.lpszClassName = L"OverlayClass";

    if (!RegisterClass(&wc)) {
        LOG_ERROR("Failed to register window class.");
        return 1;
    }

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"OverlayClass", L"Overlay",
        WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) {
        LOG_ERROR("Failed to create overlay window.");
        return 1;
    }

    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 100, LWA_ALPHA);

    // Main loop
    MSG msg = {};
    
    while (isRunning.load()) {
        if (shareInfo.writeValueInputReady.load()) {
            PerformMemoryWrite();
        }

       if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                isRunning.store(false);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // No messages, check hotkeys and idle
            if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x41)) { // Ctrl+Alt+A
                ToggleOverlay();
                Sleep(300); // Debounce
            } else if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x58)) { // Ctrl+Alt+X
                // --> Add flag clearing here <--
                shareInfo.writeValueRequestPending.store(false);
                shareInfo.writeValueInputReady.store(false);
                CreateInputWindow();
                Sleep(300); // Debounce
            } else if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x53)) { // Ctrl+Alt+S
                LOG_INFO("Exit requested via hotkey (Ctrl+Alt+S)."); // Use INFO or FATAL consistently
                isRunning.store(false);
                break;
            } else {
                // --> Wait briefly if nothing else happened <--
                MsgWaitForMultipleObjectsEx(0, NULL, 10, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
        }
    }

    UNREGISTER_HANDLE(g_hInstance);
    UNREGISTER_HANDLE(hInstance);
    UNREGISTER_HANDLE(g_hWnd);
    UNREGISTER_HANDLE(g_hInputWnd);
    UNREGISTER_HANDLE(g_hTextBox);
    UNREGISTER_HANDLE(g_hSubmitButton);
    
    processSearcherThread.join();
    screenReaderThread.join();

    return 0;
}