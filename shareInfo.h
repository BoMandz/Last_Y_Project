#ifndef SHARE_INFO_H
#define SHARE_INFO_H

#include <atomic>
#include <iostream>
#include <windows.h>
#include <string>
#include <mutex>
#include <vector>

#define WM_APP_REQUEST_WRITE_VALUE (WM_APP + 1)
#define WM_APP_PERFORM_WRITE (WM_APP + 2)

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
    std::vector<uintptr_t> memoryFoundPointers;
    std::vector<uintptr_t>  voidPoitersFinaly;
    std::atomic<bool> writeValueRequestPending = false;
    std::atomic<bool> writeValueInputReady = false;     
    std::atomic<int> valueToWrite = 0; 

    State_Overlay();
    void update(bool visible, bool running, RECT rect, bool dragging, HWND g_h) {
        isOverlayVisible.store(visible);
        isRunning.store(running);
        selected = rect;
        isDragging.store(dragging);
        g_hWnd = g_h;
    }

    void updateVoidPoitersFinaly(std::vector<uintptr_t> var){
        std::lock_guard<std::mutex> lock(dataMutex);
        voidPoitersFinaly = var;
    }

    void updateMemoryFoundPointers(std::vector<uintptr_t> var){
        std::lock_guard<std::mutex> lock(dataMutex);
        memoryFoundPointers = var;
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

    std::vector<uintptr_t> getVoidPoitersFinaly(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return voidPoitersFinaly;
    }


    std::vector<uintptr_t> getMemoryFoundPointers(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return memoryFoundPointers;
    }

    void setValueToWrite(int val) { valueToWrite.store(val); }
    int getValueToWrite() { return valueToWrite.load(); }
    
};

extern State_Overlay shareInfo;

#endif
