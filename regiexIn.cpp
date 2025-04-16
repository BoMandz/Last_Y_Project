#include "regiexIn.h"
#include "shareInfo.h" 
#include "errorHandler.h"
#include "valueSearch.h"
//==================//
#include <windows.h>
#include <regex>
#include <iostream>
#include <thread>
#include <algorithm>
#include <vector>

// Define the external variable
regiex_In regiexIn;

// Implement the ReturnFromRex method
void regiex_In::ReturnFromRex() {
    std::regex number_pattern(R"(\d+)"); // Regex pattern to match numbers

    std::string text = shareInfo.textReturned; // Assuming textReturned is updated elsewhere

    auto words_begin = std::sregex_iterator(text.begin(), text.end(), number_pattern);
    auto words_end = std::sregex_iterator();

    std::string output;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        output += i->str();
    }

    if (!output.empty()) {
        try {
            int number = std::stoi(output);
            shareInfo.updateTheINT(number); // Update the target number for the scanner

            // --- Memory Scan and Intersection Logic ---
            std::vector<uintptr_t> previousAddresses = shareInfo.getMemoryFoundPointers();

            // We might already be waiting for user input from a *previous* cycle.
            // Don't start a new scan or request if a write is pending completion.
            if (shareInfo.writeValueRequestPending.load() || shareInfo.writeValueInputReady.load()) {
                 LOG_INFO("Skipping scan/request: Write operation from previous cycle is still pending.");
                 return; // Exit this call to ReturnFromRex
            }

            std::thread valueSearchThread([]() { runTheValueSearcher(true); });
            valueSearchThread.join(); // Wait for scan

            std::vector<uintptr_t> currentAddresses = shareInfo.getMemoryFoundPointers();
            std::vector<uintptr_t> refinedAddresses;

            if (previousAddresses.empty()) {
                // First scan results
                refinedAddresses = std::move(currentAddresses);
            } else {
                // Subsequent scan, find intersection
                std::sort(previousAddresses.begin(), previousAddresses.end());
                std::sort(currentAddresses.begin(), currentAddresses.end());
                std::set_intersection(
                    previousAddresses.begin(), previousAddresses.end(),
                    currentAddresses.begin(), currentAddresses.end(),
                    std::back_inserter(refinedAddresses)
                );
            }
            shareInfo.updateVoidPoitersFinaly(refinedAddresses); // Store the candidates regardless

            // --- Check size and REQUEST input if 1-3 addresses found ---
            size_t finalAddressCount = refinedAddresses.size();
            if (finalAddressCount > 0 && finalAddressCount <= 3) {
                LOG_INFO("Found " + std::to_string(finalAddressCount) + " candidate addresses. Requesting user input via GUI.");

                // Set the flag indicating we need input for these addresses
                shareInfo.writeValueRequestPending.store(true);
                shareInfo.writeValueInputReady.store(false); // Ensure input is not ready yet

                // Get the main window handle FROM shareInfo
                HWND hMainWindow = shareInfo.g_hWnd; // Assumes g_hWnd in shareInfo is valid

                if (hMainWindow && IsWindow(hMainWindow)) {
                    // Post a message to the main window's message queue.
                    // This tells the main thread to open the input window.
                    PostMessage(hMainWindow, WM_APP_REQUEST_WRITE_VALUE, 0, 0);
                } else {
                     LOG_ERROR("Cannot request write value: Main window handle in shareInfo is invalid.");
                    // Reset the flag if we can't post the message
                    shareInfo.writeValueRequestPending.store(false);
                }
                // --- DO NOT attempt the write here. Wait for the GUI thread ---

            } else {
                // Not 1-3 addresses, just log and proceed (list is already updated)
                if (finalAddressCount == 0) {
                    LOG_INFO("No addresses found in intersection or first scan was empty.");
                } else {
                    LOG_INFO("Found " + std::to_string(finalAddressCount) + " addresses. Storing for next refinement cycle. No write requested.");
                }
                 // Ensure flags are clear if we aren't requesting a write
                 shareInfo.writeValueRequestPending.store(false);
                 shareInfo.writeValueInputReady.store(false);
            }

        } catch (const std::invalid_argument& e) {
            LOG_WARNING("Invalid argument during number conversion: " + std::string(e.what()));
            shareInfo.writeValueRequestPending.store(false); // Clear flag on error
            shareInfo.writeValueInputReady.store(false);
        } catch (const std::out_of_range& e) {
            LOG_WARNING("Out of range during number conversion: " + std::string(e.what()));
             shareInfo.writeValueRequestPending.store(false); // Clear flag on error
             shareInfo.writeValueInputReady.store(false);
        }
    } else {
         // No number found in OCR text
         shareInfo.writeValueRequestPending.store(false); // Ensure flags are clear
         shareInfo.writeValueInputReady.store(false);
    }
}