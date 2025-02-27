#include "shareInfo.h"
#include "screenReader.h"
#include "consoleHandler.h"
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
RECT brightRect = { -1, -1, -1, -1 };
RECT dragRect = { -1, -1, -1, -1 };
bool isDragging = false;
POINT startPoint = { -1, -1 };
POINT currentPoint = { -1, -1 };

// Function to check if a key is pressed
static bool isKeyPressed(int key) {
    return GetAsyncKeyState(key) & 0x8000;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
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
        ShowWindow(g_hWnd, SW_HIDE);
        isOverlayVisible.store(false);
    } else {
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

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = NULL; 
    wc.lpszClassName = L"OverlayClass";

    if (!RegisterClass(&wc)) {
        std::cerr << "Failed to register window class." << std::endl;
        return 1;
    }

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"OverlayClass", L"Overlay",
        WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) {
        std::cerr << "Failed to create overlay window." << std::endl;
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

        // Ctrl + Alt + S to exit
        if (isKeyPressed(VK_CONTROL) && isKeyPressed(VK_MENU) && isKeyPressed(0x53)) {
            isRunning = false;
            break;
        }
    }

    screenReaderThread.join();

    conHandler.The_Output_New_Line("Ended");

    return 0;
}