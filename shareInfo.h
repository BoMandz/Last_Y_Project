// shareInfo.h
#ifndef SHARE_INFO_H
#define SHARE_INFO_H

#include <atomic>
#include <iostream>
#include <windows.h>
#include <string>
#include <mutex>
#include <vector>
#include <limits> // Required for INT_MIN

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
    std::string textReturned; // Text from OCR
    DWORD theRetunedINT;     // The *current* integer value extracted from OCR
    int thePIDOfProsses;
    std::string userInput;
    // void* targetFound; // Likely redundant now
    std::vector<uintptr_t> memoryFoundPointers; // Raw results from the *last full scan* (might remove later if not needed)
    std::vector<uintptr_t> voidPoitersFinaly;   // The *current refined candidates*
    std::atomic<bool> writeValueRequestPending = false;
    std::atomic<bool> writeValueInputReady = false;
    std::atomic<int> valueToWrite = 0;
    // *** NEW: Store the last value we actually searched/refined for ***
    std::atomic<int> lastSearchedValue = INT_MIN; // Initialize to an unlikely value

    State_Overlay();
    void update(bool visible, bool running, RECT rect, bool dragging, HWND g_h) {
         // No need for lock here as atomics are used individually
        isOverlayVisible.store(visible);
        isRunning.store(running);
         // Lock needed for non-atomic RECT selected
         std::lock_guard<std::mutex> lock(dataMutex);
        selected = rect;
        isDragging.store(dragging);
        g_hWnd = g_h; // Assigning handle likely okay without lock if done from main thread only
    }

    // --- Refined address list ---
    void updateVoidPoitersFinaly(const std::vector<uintptr_t>& var){ // Pass by const ref
        std::lock_guard<std::mutex> lock(dataMutex);
        voidPoitersFinaly = var;
    }
    std::vector<uintptr_t> getVoidPoitersFinaly(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return voidPoitersFinaly; // Return by value (copy)
    }

    // --- Raw scan results (potentially remove later) ---
    void updateMemoryFoundPointers(const std::vector<uintptr_t>& var){
        std::lock_guard<std::mutex> lock(dataMutex);
        memoryFoundPointers = var;
    }
    std::vector<uintptr_t> getMemoryFoundPointers(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return memoryFoundPointers;
    }

    // --- Target process/value ---
    void updateThePIDOfProsses(DWORD var){ // Use DWORD consistently
        std::lock_guard<std::mutex> lock(dataMutex);
        thePIDOfProsses = var;
    }
     DWORD getThePIDOfProsses(){ // Use DWORD consistently
        std::lock_guard<std::mutex> lock(dataMutex);
        return thePIDOfProsses;
    }

    // --- OCR Text / Extracted Int ---
    void updateTheString(const std::string& var) {
        std::lock_guard<std::mutex> lock(dataMutex);
        textReturned = var;
    }
    std::string getTheString() { // Getter for text
        std::lock_guard<std::mutex> lock(dataMutex);
        return textReturned;
    }
    void updateTheINT(int a) { // Current value from OCR
        // Maybe update atomic directly if no lock needed? Assume lock is safer for now.
        std::lock_guard<std::mutex> lock(dataMutex);
        theRetunedINT = a;
    }
    int getTheReturnedINT() const { // Current value from OCR
        std::lock_guard<std::mutex> lock(dataMutex);
        return theRetunedINT;
    }

    // --- Last Searched Value ---
    void updateLastSearchedValue(int val) {
        lastSearchedValue.store(val);
    }
    int getLastSearchedValue() const {
        return lastSearchedValue.load();
    }

    // --- User Input ---
    void updateUserInput(const std::string& var){
        std::lock_guard<std::mutex> lock(dataMutex);
        userInput = var;
    }
    std::string getUserInput(){
        std::lock_guard<std::mutex> lock(dataMutex);
        return userInput;
    }

    // --- Selected Area ---
    RECT getSelected() const {
        std::lock_guard<std::mutex> lock(dataMutex);
        return selected;
    }

    // --- Value to Write ---
    void setValueToWrite(int val) { valueToWrite.store(val); }
    int getValueToWrite() { return valueToWrite.load(); }

};

extern State_Overlay shareInfo;

#endif