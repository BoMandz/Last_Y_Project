#ifndef SHARE_INFO_H
#define SHARE_INFO_H

#include <atomic>
#include <windows.h>
#include <string>
#include <mutex>
struct State_Overlay {
    // Add mutex for thread safety
    mutable std::mutex dataMutex;
    
    HWND g_hWnd;
    std::atomic<bool> isOverlayVisible;
    std::atomic<bool> isRunning;
    RECT selected;
    std::atomic<bool> isDragging;
    std::string textReturned;
    DWORD theRetunedINT;
    int thePIDOfProsses;
    std::string userInput;
    void* targetFound;

    State_Overlay();
    void update(bool visible, bool running, RECT rect, bool dragging, HWND g_h) {
        isOverlayVisible.store(visible);
        isRunning.store(running);
        selected = rect;
        isDragging.store(dragging);
        g_hWnd = g_h;
    }

    void updateTargetFound(void* var){
        std::lock_guard<std::mutex> lock(dataMutex);
        targetFound = var;
    }

    void* getTargetFound(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return targetFound;
    }

    void updateUserInput(const std::string& var){
        std::lock_guard<std::mutex> lock(dataMutex);
        userInput = var;
    }

    std::string getUserInput(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return userInput;
    }

    void updateThePIDOfProsses(const DWORD& var){
        std::lock_guard<std::mutex> lock(dataMutex);
        thePIDOfProsses = var;
    }

    void updateTheString(const std::string& var) {
        std::lock_guard<std::mutex> lock(dataMutex);
        textReturned = var;
    }

    DWORD getThePIDOfProsses(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return thePIDOfProsses;
    }

    int getTheReturnedINT() const {
        std::lock_guard<std::mutex> lock(dataMutex);
        return theRetunedINT;
    }

    RECT getSelected() const {
        std::lock_guard<std::mutex> lock(dataMutex);
        return selected;
    }

    void updateTheINT(int a) {
        std::lock_guard<std::mutex> lock(dataMutex);
        theRetunedINT = a;
    }
};

extern State_Overlay shareInfo;

#endif
