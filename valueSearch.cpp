#include "valueSearch.h" 
#include "shareInfo.h"
#include "errorHandler.h"
//=================//
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <cstring> 
#include <stdint.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm> 

std::vector<uintptr_t> searchMemoryForInt(DWORD pid, int value, bool verbose) {
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
            ss << "Failed to open process " << pid << " for initial scan. Error code: " << GetLastError();
            LOG_ERROR(ss.str());
        }
        return results; 
    }
    REGISTER_HANDLE(process_handle); 

    MEMORY_BASIC_INFORMATION mbi;
    LPVOID address = 0;

     while (VirtualQueryEx(process_handle, address, &mbi, sizeof(mbi))) {
            bool is_readable = (mbi.State == MEM_COMMIT) &&
                           ((mbi.Protect & PAGE_READONLY) ||
                            (mbi.Protect & PAGE_READWRITE) ||
                            (mbi.Protect & PAGE_WRITECOPY) || 
                            (mbi.Protect & PAGE_EXECUTE_READ) ||
                            (mbi.Protect & PAGE_EXECUTE_READWRITE) ||
                            (mbi.Protect & PAGE_EXECUTE_WRITECOPY)) && 
                           !(mbi.Protect & PAGE_GUARD) &&
                           !(mbi.Protect & PAGE_NOACCESS);

            if (is_readable) {
                MemoryRegion region;
                region.start_address = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                region.end_address = region.start_address + mbi.RegionSize;
                memory_regions.push_back(region);
            }
            address = (LPVOID)((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
            if ((uintptr_t)address >= (uintptr_t)-1) { 
                 break;
             }
     }

     const size_t buffer_size = 65536; 
     std::vector<char> buffer(buffer_size); 
     size_t total_searched = 0;
     for (const auto& region : memory_regions) {
         uintptr_t current_address = region.start_address;
         size_t remaining_in_region = region.end_address - current_address;
         while (remaining_in_region >= sizeof(int)) {
             size_t bytes_to_read = std::min(buffer_size, remaining_in_region);
             SIZE_T bytes_read = 0;
             bool read_success = ReadProcessMemory(process_handle, (LPCVOID)current_address, buffer.data(), bytes_to_read, &bytes_read);
             if (read_success && bytes_read > 0) {
                 for (size_t i = 0; i <= bytes_read - sizeof(int); ++i) {
                      int potential_value;
                      std::memcpy(&potential_value, buffer.data() + i, sizeof(int));
                      if (potential_value == value) {
                          results.push_back(current_address + i);
                      }
                 }
                 current_address += bytes_read;
                 remaining_in_region -= bytes_read;
                 total_searched += bytes_read;
             } else {
                 if (verbose && !read_success) { /* log error */ }
                 break;
             }
         }
     }


    CloseHandle(process_handle);
    UNREGISTER_HANDLE(process_handle); 

    if (verbose) {
        std::stringstream ss;
        ss << "Initial scan complete. Found " << results.size() << " matches for value " << value;
        LOG_INFO(ss.str());
    }
    return results;
}

std::vector<uintptr_t> refineCandidates(DWORD pid, const std::vector<uintptr_t>& candidates, int newValue, bool verbose) {
    std::vector<uintptr_t> refinedList;

    if (candidates.empty()) {
        if (verbose) {
            std::cout << "[Refine] No candidates to refine." << std::endl;
        }
        return refinedList;
    }

    HANDLE process_handle = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (process_handle == NULL) {
        if (verbose) {
            std::cerr << "[Refine] Failed to open process " << pid << ". Error: " << GetLastError() << std::endl;
        }
        return refinedList;
    }
    REGISTER_HANDLE(process_handle);

    int currentValue = 0;
    SIZE_T bytesRead = 0;
    int keptCount = 0;

    for (uintptr_t addr : candidates) {
        bytesRead = 0;
        BOOL success = ReadProcessMemory(
            process_handle,
            (LPCVOID)addr,
            &currentValue,         
            sizeof(currentValue),
            &bytesRead
        );

        if (success && bytesRead == sizeof(currentValue)) {
            if (verbose) {
                std::cout << "[Refine] Addr: 0x" << std::hex << addr
                          << " | Read: " << std::dec << currentValue
                          << " | Target: " << newValue << std::endl;
            }

            if (currentValue == newValue) {
                refinedList.push_back(addr);
                keptCount++;
            }
        } else {
            if (verbose) {
                std::cerr << "[Refine] Failed to read 0x" << std::hex << addr
                          << " | Error: " << GetLastError() << std::endl;
            }
        }
    }

    CloseHandle(process_handle);
    UNREGISTER_HANDLE(process_handle);

    if (verbose) {
        std::cout << "[Refine] Finished. Kept " << keptCount << " of " << candidates.size()
                  << " addresses matching new value: " << newValue << std::endl;
    }

    return refinedList;
}