#include "shareInfo.h"
#include "regiexIn.h"
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
    // For Windows 8.1 or later
    HMODULE user32Module = LoadLibrary(TEXT("user32.dll"));
    if (user32Module) {
        auto GetDpiForSystem = reinterpret_cast<UINT(WINAPI*)()>(
            GetProcAddress(user32Module, "GetDpiForSystem"));
        if (GetDpiForSystem) {
            return GetDpiForSystem();
        }
    }
    return 96;
}

std::string captureAndReadText() {
    RECT rect = shareInfo.getSelected();
    if (rect.left < 0 || rect.top < 0 || rect.right <= rect.left || rect.bottom <= rect.top) {
        return "No valid area selected!";
    }
    
    UINT dpi = getSystemDPI();

    float scaleFactor = dpi / 96.0f;

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    width = static_cast<int>(width * scaleFactor);
    height = static_cast<int>(height * scaleFactor);
    
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid region dimensions!" << std::endl;
        ReleaseDC(NULL, hScreen);
        DeleteDC(hDC);
        return "";
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, width, height, hScreen, rect.left, rect.top, SRCCOPY);

    // Get bitmap properties
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    // Create OpenCV Mat with proper dimensions
    cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4);
    GetBitmapBits(hBitmap, bmp.bmHeight * bmp.bmWidth * 4, mat.data);

    // Cleanup GDI resources
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    // Rest of processing remains the same
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);

    cv::Mat processed;
    cv::threshold(gray, processed, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Initialize Tesseract
    fs::path tessdataPrefix = "C:/msys64/mingw64/share/tessdata";
    if (!fs::exists(tessdataPrefix)) {
        std::cerr << "Tesseract data path not found: " << tessdataPrefix << std::endl;
        return "TESSDATA PATH ERROR!";
    }

    tesseract::TessBaseAPI tess;
    if (tess.Init(tessdataPrefix.string().c_str(), "eng")) {
        return "Tesseract init failed!";
    }
    tess.SetImage(processed.data, processed.cols, processed.rows, 1, processed.step);
    
    std::string text = tess.GetUTF8Text();
    return text;
}

void screenReaderLoop() {
    while (shareInfo.isRunning.load()) {  
        std::string text = captureAndReadText();
        shareInfo.updateTheString(text);
        regiexIn.ReturnFromRex();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
}