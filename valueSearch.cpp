#include "shareInfo.h"
#include "errorHandler.h"
//=================//
#include <iostream>
#include <vector>
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include <chrono>

std::vector<void*> findValueInProcessMemory(DWORD pid, int targetValue) {
    std::vector<void*> foundLocations;
    
    // Get a handle to the process
    HANDLE processHandle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
        pid
    );
    REGISTER_HANDLE(processHandle);
    
    if (processHandle == NULL) {
        LOG_INFO("Failed to open process with PID");

        while(true){
            if (processHandle != NULL){
                break;
            }
            LOG_INFO("Failed to open process with PID");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        return foundLocations;
    }
    
    try {
        // Get system info for memory page size and address range
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        // Start at the minimum application address
        MEMORY_BASIC_INFORMATION memInfo;
        LPCVOID address = sysInfo.lpMinimumApplicationAddress;
        
        // Buffer for reading memory
        int buffer[4096];
        
        // Iterate through memory regions
        while (address < sysInfo.lpMaximumApplicationAddress) {
            // Query the memory region
            if (VirtualQueryEx(processHandle, address, &memInfo, sizeof(memInfo))) {
                // Check if the memory region is committed, readable, and not guarded or noaccess
                if ((memInfo.State == MEM_COMMIT) && 
                    (memInfo.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) && 
                    !(memInfo.Protect & PAGE_GUARD) && 
                    !(memInfo.Protect & PAGE_NOACCESS)) {
                    
                    // Scan the memory region in chunks
                    SIZE_T bytesRead;
                    SIZE_T offset = 0;
                    
                    while (offset < memInfo.RegionSize) {
                        SIZE_T bytesToRead = sizeof(buffer);
                        if (offset + bytesToRead > memInfo.RegionSize) {
                            bytesToRead = memInfo.RegionSize - offset;
                        }
                        
                        // Only process complete integers
                        bytesToRead = (bytesToRead / sizeof(int)) * sizeof(int);
                        if (bytesToRead == 0) break;
                        
                        // Read memory chunk
                        if (ReadProcessMemory(processHandle, 
                                             (LPCVOID)((uintptr_t)memInfo.BaseAddress + offset), 
                                             buffer, 
                                             bytesToRead, 
                                             &bytesRead)) {
                            
                            // Check each int in the buffer
                            int numInts = bytesRead / sizeof(int);
                            for (int i = 0; i < numInts; i++) {
                                if (buffer[i] == targetValue) {
                                    // Record the address where we found the target value
                                    void* foundAddress = (void*)((uintptr_t)memInfo.BaseAddress + offset + (i * sizeof(int)));
                                    foundLocations.push_back(foundAddress);
                                }
                            }
                        }
                        
                        // Move to the next chunk
                        offset += bytesRead;
                        
                        // If we couldn't read any bytes, move to the next page
                        if (bytesRead == 0) break;
                    }
                }
                
                // Move to the next memory region
                address = (LPVOID)((uintptr_t)memInfo.BaseAddress + memInfo.RegionSize);
            } else {
                // If VirtualQueryEx fails, move forward by the system page size
                address = (LPVOID)((uintptr_t)address + sysInfo.dwPageSize);
            }
        }
    } catch (const std::exception& e) {
        LOG_FATAL(std::string("Exception occurred: ") + e.what());
    }
    
    // Clean up
    UNREGISTER_HANDLE(processHandle);
    CloseHandle(processHandle);
    
    return foundLocations;
}

std::vector<void*> findOverlappingLocations(const std::vector<void*>& locations1, const std::vector<void*>& locations2) {
    std::vector<void*> overlappingLocations;
    
    // If either vector is empty, no overlaps are possible
    if (locations1.empty() || locations2.empty()) {
        return overlappingLocations;
    }
    
    // For better performance when one vector is much larger than the other,
    // create a hash set from the smaller vector
    if (locations1.size() <= locations2.size()) {
        std::unordered_set<void*> locationSet(locations1.begin(), locations1.end());
        
        // Check each location in the second vector against the set
        for (const auto& location : locations2) {
            if (locationSet.find(location) != locationSet.end()) {
                overlappingLocations.push_back(location);
            }
        }
    } else {
        std::unordered_set<void*> locationSet(locations2.begin(), locations2.end());
        
        // Check each location in the first vector against the set
        for (const auto& location : locations1) {
            if (locationSet.find(location) != locationSet.end()) {
                overlappingLocations.push_back(location);
            }
        }
    }
    
    return overlappingLocations;
}

void* lastOverlap() {
    DWORD pid;
    
    pid = shareInfo.getThePIDOfProsses();

    std::vector<void*> previousLocations;
    std::vector<void*> currentLocations;
    
    while (true) {
        // Find the current locations of the target value in the process memory
        currentLocations = findValueInProcessMemory(pid, shareInfo.getTheReturnedINT());
        
        if (previousLocations.empty()) {
            // If this is the first scan, just store the current locations
            previousLocations = currentLocations;
        } else {
            // Find overlapping locations between the previous and current scans
            std::vector<void*> overlappingLocations = findOverlappingLocations(previousLocations, currentLocations);
            
            if (overlappingLocations.size() == 1) {
                // If only one overlapping address remains, return it
                return overlappingLocations[0];
            } else if (overlappingLocations.empty()) {
                // If no overlapping addresses, reset the previous locations
                previousLocations = currentLocations;
            } else {
                // Otherwise, update the previous locations to the overlapping ones
                previousLocations = overlappingLocations;
            }
        }
        
        // Wait for a short period before scanning again
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
    
    return nullptr; // This line should never be reached
}

void runTheValueSearcher(){
    void* val = lastOverlap();
    std::stringstream ss;
    
    while(shareInfo.isRunning.load()){
        if (val == nullptr){
            val = lastOverlap();
            ss << "0x" << std::hex << std::setw(sizeof(void*) * 2) << std::setfill('0') << reinterpret_cast<uintptr_t>(val);
            std::string ptrStr = ss.str();
            LOG_INFO("Pointer to mem: " + ptrStr);
        }else{

        }
    }
}