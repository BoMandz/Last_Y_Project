// valueSearch.cpp
#include "valueSearch.h" // Include the header
#include "shareInfo.h"
#include "errorHandler.h"
//=================//
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <cstring> // For memcpy or direct read
#include <stdint.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm> // For std::min if needed

// --- searchMemoryForInt implementation remains largely the same ---
// Make sure the pid parameter is DWORD and remove the default arg value here
std::vector<uintptr_t> searchMemoryForInt(DWORD pid, int value, bool verbose) {
    std::vector<uintptr_t> results;
    // ... (rest of the existing searchMemoryForInt implementation)
    // Ensure OpenProcess uses pid (DWORD)
    // Ensure logging uses pid correctly
    // Remember to REGISTER_HANDLE/UNREGISTER_HANDLE for process_handle
    // Return results
     // ... (Paste the full implementation from previous answers, ensuring pid is DWORD) ...
     // Example snippet start:
     struct MemoryRegion {
        uintptr_t start_address;
        uintptr_t end_address;
    };
    std::vector<MemoryRegion> memory_regions;

    // Use PROCESS_VM_READ only, unless write is needed elsewhere (unlikely for search)
    HANDLE process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    // REGISTER_HANDLE(process_handle); // Register handle - DO IT *AFTER* checking for NULL

    if (process_handle == NULL) {
        if (verbose) {
            std::stringstream ss;
            ss << "Failed to open process " << pid << " for initial scan. Error code: " << GetLastError();
            LOG_ERROR(ss.str());
        }
        return results; // Return empty list
    }
    REGISTER_HANDLE(process_handle); // Register *after* successful open

    MEMORY_BASIC_INFORMATION mbi;
    LPVOID address = 0;

    // --- VirtualQueryEx loop ---
    // ... (same loop as before to find readable regions) ...
     while (VirtualQueryEx(process_handle, address, &mbi, sizeof(mbi))) {
            // Check if readable (consider adding PAGE_WRITECOPY as well)
            bool is_readable = (mbi.State == MEM_COMMIT) &&
                           ((mbi.Protect & PAGE_READONLY) ||
                            (mbi.Protect & PAGE_READWRITE) ||
                            (mbi.Protect & PAGE_WRITECOPY) || // Added WriteCopy
                            (mbi.Protect & PAGE_EXECUTE_READ) ||
                            (mbi.Protect & PAGE_EXECUTE_READWRITE) ||
                            (mbi.Protect & PAGE_EXECUTE_WRITECOPY)) && // Added Execute WriteCopy
                           !(mbi.Protect & PAGE_GUARD) &&
                           !(mbi.Protect & PAGE_NOACCESS);

            if (is_readable) {
                MemoryRegion region;
                region.start_address = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                region.end_address = region.start_address + mbi.RegionSize;
                memory_regions.push_back(region);
            }
            address = (LPVOID)((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
            if ((uintptr_t)address >= (uintptr_t)-1) { // Check for wrap-around or max address
                 break;
             }
     }

    // --- Buffer and search loop ---
    // ... (same buffer logic and reading loop as before) ...
     const size_t buffer_size = 65536; // Larger buffer can be faster
     std::vector<char> buffer(buffer_size); // Use vector for automatic memory management
     size_t total_searched = 0;
     // ... loop through memory_regions ...
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
                  // Handle read failure - log and break inner loop for this region
                 if (verbose && !read_success) { /* log error */ }
                 break;
             }
         }
     }


    CloseHandle(process_handle);
    UNREGISTER_HANDLE(process_handle); // Unregister handle

    if (verbose) {
        std::stringstream ss;
        ss << "Initial scan complete. Found " << results.size() << " matches for value " << value;
        LOG_INFO(ss.str());
    }
    return results;
     // Example snippet end.
}


// *** NEW: Refine function implementation ***
// Remove the default argument value here
std::vector<uintptr_t> refineCandidates(DWORD pid, const std::vector<uintptr_t>& candidates, int newValue, bool verbose) {
    std::vector<uintptr_t> refinedList;
    if (candidates.empty()) {
        // ... (empty check logic) ...
        return refinedList;
    }

    HANDLE process_handle = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    // ... (handle checking and registration logic) ...
     if (process_handle == NULL) {
        // ... (error handling) ...
        return refinedList;
     }
     REGISTER_HANDLE(process_handle);

    if (verbose) {
        // ... (logging) ...
    }

    int currentValue = 0; // Variable to hold the value read from memory
    SIZE_T bytesRead = 0;
    int keptCount = 0;
    LPVOID tValue;

    for (uintptr_t addr : candidates) {
        bytesRead = 0; // Reset for each read attempt
        BOOL success = ReadProcessMemory(
            process_handle,
            (LPCVOID)addr,
            tValue,       // Buffer to store the read value <<< CORRECTION: Uses currentValue
            sizeof(currentValue), // Number of bytes to read
            &bytesRead
        );

        if (success && bytesRead == sizeof(currentValue)) {
            // Successfully read the value, now compare it
            // *** CORRECTION: Compare currentValue with newValue ***
            if (currentValue == newValue) {
                refinedList.push_back(addr); // Keep this address
                keptCount++;
            }
        } else {
            // ... (error handling for failed read) ...
             if (verbose) {
                 // ... logging ...
             }
        }
    }

    CloseHandle(process_handle);
    UNREGISTER_HANDLE(process_handle); // Unregister handle

    if (verbose) {
        // ... (logging) ...
    }

    return refinedList;
}