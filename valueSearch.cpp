#include "shareInfo.h"
#include "errorHandler.h"
//=================//
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <cstring>
#include <stdint.h>
#include <thread>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>


std::vector<uintptr_t> searchMemoryForInt(int pid, int value, bool verbose = true) {
    std::vector<uintptr_t> results;
    
    struct MemoryRegion {
        uintptr_t start_address;
        uintptr_t end_address;
    };
    std::vector<MemoryRegion> memory_regions;
    
    HANDLE process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process_handle == NULL) {
        if (verbose) {
            std::stringstream ss;
            ss << "Failed to open process. Error code: " << GetLastError();
            LOG_FATAL(ss.str());
        }
        return results;
    }
    
    MEMORY_BASIC_INFORMATION mbi;
    LPVOID address = 0;
    
    while (VirtualQueryEx(process_handle, address, &mbi, sizeof(mbi))) {
        bool is_readable = (mbi.State == MEM_COMMIT) && 
                            ((mbi.Protect & PAGE_READONLY) || 
                            (mbi.Protect & PAGE_READWRITE) || 
                            (mbi.Protect & PAGE_EXECUTE_READ) || 
                            (mbi.Protect & PAGE_EXECUTE_READWRITE)) && 
                            !(mbi.Protect & PAGE_GUARD) && 
                            !(mbi.Protect & PAGE_NOACCESS);
            
        if (is_readable) {
            MemoryRegion region;
            region.start_address = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            region.end_address = region.start_address + mbi.RegionSize;
            memory_regions.push_back(region);
            
            if (verbose) {
                std::stringstream ss;

                ss << "Readable region: 0x" << std::hex << region.start_address 
                          << " - 0x" << region.end_address 
                          << " Protection: 0x" << mbi.Protect << std::dec;
                LOG_INFO(ss.str());
            }
        }
        
        address = (LPVOID)((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
        
        if ((uintptr_t)address >= 0x7FFFFFFFFFFFFFFF) {
            break;
        }
    }
    
    if (verbose) {
        std::cout <<"Found " + std::to_string(memory_regions.size()) + " readable memory regions" <<"\n";
    }
    
    const size_t buffer_size = 4096;
    int* buffer = new int[buffer_size / sizeof(int)];
    size_t total_searched = 0;
    
    for (const auto& region : memory_regions) {
        uintptr_t address = region.start_address;
        size_t region_size = region.end_address - region.start_address;
        
        if (verbose) {
            std::stringstream ss;
            ss << "Searching region 0x" 
            << std::hex << address 
            << " - 0x" 
            << region.end_address 
            << std::dec 
            << " (" 
            << (region_size / 1024) 
            << " KB)";
            LOG_INFO(ss.str());
        }
        
        size_t progress = 0;
        while (address + sizeof(int) <= region.end_address) {
            size_t bytes_to_read = buffer_size;
            
            if (address + bytes_to_read > region.end_address) {
                bytes_to_read = region.end_address - address;
            }
            
            bytes_to_read = (bytes_to_read / sizeof(int)) * sizeof(int);
            
            if (bytes_to_read < sizeof(int)) {
                break;
            }
            
            SIZE_T bytes_read;
            bool read_success = ReadProcessMemory(process_handle, (LPCVOID)address, buffer, bytes_to_read, &bytes_read) && bytes_read == bytes_to_read;
            
            if (read_success) {
                size_t ints_to_check = bytes_to_read / sizeof(int);
                
                for (size_t i = 0; i < ints_to_check; i++) {
                    if (buffer[i] == value) {
                        results.push_back(address + (i * sizeof(int)));
                    }
                }
                
                address += bytes_to_read;
                total_searched += bytes_to_read;
                
                if (verbose && (total_searched / (5 * 1024 * 1024)) > progress) {
                    progress = total_searched / (5 * 1024 * 1024);
                    std::stringstream ss;
                    ss << "Searched " << (total_searched / (1024 * 1024)) << " MB...";
                    LOG_INFO(ss.str());
                }
            } else {
                if (verbose) {
                    std::stringstream ss;
                    ss << "ReadProcessMemory failed at address 0x" << std::hex << address 
                    << " Error code: " << GetLastError() << std::dec;
                    LOG_FATAL(ss.str());
                }
                address = (address + 4096) & ~0xFFF;
            }
        }
    }
    
    delete[] buffer;
    
    CloseHandle(process_handle);
    
    if (verbose) {
        std::stringstream ss;
        ss << "Search complete. Total memory searched: " << (total_searched / (1024 * 1024)) << " MB";
        LOG_INFO(ss.str());
        ss.str("");
        ss << "Found " << results.size() << " matches";
        LOG_INFO(ss.str());
    }
    
    return results;
}


void runTheValueSearcher(bool verbose = true){
    while (shareInfo.isRunning)
    {
        DWORD pid = 0;
        int value = NULL;
        std::vector<uintptr_t> listOfPointers;
        while (shareInfo.getThePIDOfProsses() == 0){
            pid = shareInfo.getThePIDOfProsses();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
        }
        while (shareInfo.getTheReturnedINT() == NULL){
            value = shareInfo.getTheReturnedINT();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
        }
        if (pid != 0 && value != NULL){
            listOfPointers = searchMemoryForInt(pid,value,verbose);
            shareInfo.updateMemoryFoundPointers(listOfPointers);
        }
    }
}