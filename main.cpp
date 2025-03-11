#include "shareInfo.h"
#include "screenReader.h"
#include "processSearcher.h"
#include "consoleHandler.h"
#include "errorHandler.h"
#include "valueSearch.h"
//=====================//
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>  
#include <opencv2/opencv.hpp>  
#include <tesseract/baseapi.h> 
#include <leptonica/allheaders.h> 

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
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 10, INPUT_WINDOW_WIDTH - 20, 30,
                hWnd, (HMENU)ID_TEXTBOX, g_hInstance, nullptr);

            // Create submit button
            g_hSubmitButton = CreateWindowW(L"BUTTON", L"Submit",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                INPUT_WINDOW_WIDTH / 2 - 40, 50, 80, 30,
                hWnd, (HMENU)ID_SUBMIT_BUTTON, g_hInstance, nullptr);

            // Set focus to the text box
            SetFocus(g_hTextBox);
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SUBMIT_BUTTON) {
                // Get text from the text box
                wchar_t buffer[1024];
                GetWindowTextW(g_hTextBox, buffer, 1024);
                
                // Convert wide string to regular string
                std::wstring wideStr(buffer);
                std::string input(wideStr.begin(), wideStr.end());
                
                // Log the user input
                LOG_INFO("User submitted input: " + input);
                
                shareInfo.updateUserInput(input);

                // Clear the text box
                SetWindowTextW(g_hTextBox, L"");
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
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

    std::thread screenReaderThread(screenReaderLoop);
    std::thread processSearcherThread(SearchForProcessLoop);
    std::thread valueSearcher(runTheValueSearcher);

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
    
    while (isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Ctrl + Alt + A to toggle overlay
        if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x41)) {
            ToggleOverlay();
            Sleep(300); 
        }

        // Ctrl + Alt + X to open input window
        if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x58)) {
            CreateInputWindow();
            Sleep(300); 
        }

        // Ctrl + Alt + S to exit
        if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x53)) {
            LOG_FATAL("Stoped the file whit Ctrl + Alt + S");
            break;
        }
    }

    valueSearcher.join();
    processSearcherThread.join();
    screenReaderThread.join();

    return 0;
}