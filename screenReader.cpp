#include "shareInfo.h"
#include "regiexIn.h"
#include "errorHandler.h"
//==================//
#include <iostream>
#include <thread>
#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <filesystem>
#include <shellscalingapi.h>
#include <windows.h>
#include <shcore.h>

namespace fs = std::filesystem;

UINT getSystemDPI() {
    HMODULE user32Module = LoadLibrary(TEXT("user32.dll"));
    if (user32Module) {
        try {
            auto GetDpiForSystem = reinterpret_cast<UINT(WINAPI*)()>(
                GetProcAddress(user32Module, "GetDpiForSystem"));
            if (GetDpiForSystem) {
                UINT dpi = GetDpiForSystem();
                FreeLibrary(user32Module);
                return dpi;
            }
        }
        catch (...) {
            FreeLibrary(user32Module);
            return 96;
        }
        FreeLibrary(user32Module);
    }
    return 96;
}

std::string captureAndReadText() {
    RECT rect = shareInfo.getSelected();
    if (rect.left < 0 || rect.top < 0 || rect.right <= rect.left || rect.bottom <= rect.top) {
        LOG_INFO("No valid area selected for screen capture.");
        return ""; // No valid area selected
    }
    
    UINT dpi = getSystemDPI();
    float scaleFactor = dpi / 96.0f;

    // Adjust for DPI scaling
    int width = static_cast<int>((rect.right - rect.left) * scaleFactor);
    int height = static_cast<int>((rect.bottom - rect.top) * scaleFactor);
    
    if (width <= 0 || height <= 0) {
        LOG_FATAL("Invalid dimensions for screen capture.");
        return "";
    }

    // Create resources for screen capture
    HDC hScreen = nullptr;
    HDC hDC = nullptr;
    HBITMAP hBitmap = nullptr;
    cv::Mat mat;

    try {
        hScreen = GetDC(NULL);
        if (!hScreen) {
            LOG_FATAL("Failed to get screen DC.");
            throw std::runtime_error("Failed to get screen DC");
        }
        
        hDC = CreateCompatibleDC(hScreen);
        if (!hDC) {
            LOG_FATAL("Failed to create compatible DC.");
            throw std::runtime_error("Failed to create compatible DC");
        }
        
        hBitmap = CreateCompatibleBitmap(hScreen, width, height);
        if (!hBitmap) {
            LOG_FATAL("Failed to create bitmap.");
            throw std::runtime_error("Failed to create bitmap");
        }
        
        HGDIOBJ oldObj = SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, width, height, hScreen, rect.left, rect.top, SRCCOPY);
        
        // Get bitmap properties
        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        
        // Create OpenCV Mat with proper dimensions
        mat = cv::Mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4);
        GetBitmapBits(hBitmap, bmp.bmHeight * bmp.bmWidth * 4, mat.data);
        
        // Cleanup GDI resources
        SelectObject(hDC, oldObj);
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
    }
    catch (const std::exception& e) {
        // Clean up in case of exception
        if (hBitmap) DeleteObject(hBitmap);
        if (hDC) DeleteDC(hDC);
        if (hScreen) ReleaseDC(NULL, hScreen);
        LOG_FATAL(std::string("Capture error: ") + e.what());
        return std::string("Capture error: ") + e.what();
    }

    // Image processing with OpenCV
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);

    cv::Mat processed;
    cv::threshold(gray, processed, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Initialize Tesseract
    fs::path tessdataPrefix = "C:/msys64/mingw64/share/tessdata";
    if (!fs::exists(tessdataPrefix)) {
        LOG_FATAL("Tesseract data path not found: " + tessdataPrefix.string());
        return ""; // Tesseract data path not found
    }

    std::string text;
    tesseract::TessBaseAPI tess;
    
    try {
        if (tess.Init(tessdataPrefix.string().c_str(), "eng")) {
            LOG_FATAL("Tesseract initialization failed.");
            return ""; // Tesseract initialization failed
        }
        
        tess.SetImage(processed.data, processed.cols, processed.rows, 1, processed.step);
        text = tess.GetUTF8Text();
        tess.End();
    }
    catch (const std::exception& e) {
        LOG_FATAL(std::string("OCR error: ") + e.what());
        return std::string("OCR error: ") + e.what();
    }

    return text;
}

void screenReaderLoop() {
    while (shareInfo.isRunning.load()) {
        if (!shareInfo.isDragging.load()) {
            std::string text = captureAndReadText();
            if (!text.empty()) {
                LOG_INFO("Captured text: " + text);
                shareInfo.updateTheString(text);
                regiexIn.ReturnFromRex();
            } else {
                LOG_INFO("Failed to capture or process text.");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}