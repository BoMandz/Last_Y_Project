// regiexIn.cpp
#include "regiexIn.h"       // Header for this class/struct
#include "shareInfo.h"      // Access to shared application state
#include "errorHandler.h"   // For logging (LOG_INFO, LOG_WARNING, LOG_ERROR)
#include "valueSearch.h"    // For searchMemoryForInt and refineCandidates

//==================// Standard Library & Windows API Headers
#include <windows.h>        // Basic Windows functionalities
#include <regex>            // For std::regex and related functions
#include <string>           // For std::string, std::stoi
#include <vector>           // For std::vector
#include <limits>           // For INT_MIN initialization
#include <system_error>     // For std::invalid_argument, std::out_of_range (used by stoi)
#include <mutex>            // For std::lock_guard

// Define the external variable if regiexIn is intended as a global instance
regiex_In regiexIn;

// Implement the ReturnFromRex method
// This function is called periodically (likely by screenReaderLoop)
// to process the latest OCR text and update the memory address candidates.
void regiex_In::ReturnFromRex() {
    // Define the regular expression to find sequences of digits
    const std::regex number_pattern(R"(\d+)"); // Matches one or more digits

    // --- Step 1: Get current state from shareInfo ---
    // Retrieve necessary information atomically or under mutex protection
    std::string ocrText = shareInfo.getTheString(); // Get the latest text from OCR
    DWORD pid = shareInfo.getThePIDOfProsses();     // Get the target Process ID

    // --- Step 2: Validate preconditions ---
    // If we don't have a valid process ID, we can't scan/refine memory
    if (pid == 0) {
        // Optional: Log this state, but it might be frequent if the target process isn't running
        // LOG_INFO("ReturnFromRex: Skipping - Target PID is 0.");

        // If the PID becomes invalid, clear previous search results to avoid stale data
        int lastValue = shareInfo.getLastSearchedValue();
        if (lastValue != INT_MIN) { // Only reset if we were actively searching
            LOG_INFO("ReturnFromRex: Target PID became 0, resetting search state.");
            shareInfo.updateVoidPoitersFinaly({});   // Clear candidate addresses
            shareInfo.updateLastSearchedValue(INT_MIN); // Reset last searched value indicator
        }
        return; // Exit the function
    }

    // If a memory write operation (initiated previously) is still pending user input
    // or being processed, skip this cycle to avoid conflicts.
    if (shareInfo.writeValueRequestPending.load() || shareInfo.writeValueInputReady.load()) {
         LOG_INFO("ReturnFromRex: Skipping scan/refinement - Write operation is pending.");
         return; // Exit the function
    }

    // --- Step 3: Extract number from OCR text ---
    // Use regex iterators to find all number sequences in the text
    auto words_begin = std::sregex_iterator(ocrText.begin(), ocrText.end(), number_pattern);
    auto words_end = std::sregex_iterator();
    std::string extractedNumberString; // String to store the concatenated digits found
    // Combine all found digit sequences (assuming only one relevant number is expected)
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        extractedNumberString += i->str();
    }

    // --- Step 4: Process the extracted number (if any) ---
    if (!extractedNumberString.empty()) {
        // A number was found in the OCR text
        try {
            // Convert the extracted digit string to an integer
            int currentNumber = std::stoi(extractedNumberString);
            // Update the shared state with the latest number found (e.g., for display)
            shareInfo.updateTheINT(currentNumber);

            // --- Step 4a: Decide Scan Type (Initial or Refine) ---
            int lastValue = shareInfo.getLastSearchedValue(); // Get the value from the *last* successful scan/refine
            std::vector<uintptr_t> currentCandidates;         // Prepare to get the candidate list from the *last* step
            { // Scope for mutex lock
                std::lock_guard<std::mutex> lock(shareInfo.dataMutex);
                currentCandidates = shareInfo.voidPoitersFinaly; // Get a copy of the current candidates
            }

            std::vector<uintptr_t> resultingCandidates; // This will hold the results of *this* cycle's scan/refine

            // --- Logic Branching ---

            // Branch 1: Value hasn't changed since the last scan/refinement.
            // No need to scan/refine again. Keep the existing candidates.
            if (currentNumber == lastValue) {
                // LOG_INFO("Value (" + std::to_string(currentNumber) + ") unchanged. Skipping scan/refine."); // Can be noisy
                resultingCandidates = currentCandidates; // Pass through the existing candidates
            }
            // Branch 2: This is the very first scan (lastValue is uninitialized) OR
            //           the candidate list is empty (e.g., after a reset or failed refine).
            // Perform a full, initial memory scan.
            else if (lastValue == INT_MIN || currentCandidates.empty()) {
                 if (currentCandidates.empty() && lastValue != INT_MIN) {
                     LOG_INFO("Candidate list empty, performing new initial scan for value: " + std::to_string(currentNumber));
                 } else {
                     LOG_INFO("Performing initial scan for value: " + std::to_string(currentNumber));
                 }
                 // Call the function that scans the entire process memory
                 resultingCandidates = searchMemoryForInt(pid, currentNumber, true); // Pass verbose flag
                 shareInfo.updateLastSearchedValue(currentNumber); // Update state: the list now corresponds to 'currentNumber'
            }
            // Branch 3: Value has changed, AND we have a non-empty list of candidates from the previous step.
            // Perform a refinement scan.
            else {
                 LOG_INFO("Value changed (" + std::to_string(lastValue) + " -> " + std::to_string(currentNumber) + "). Refining " + std::to_string(currentCandidates.size()) + " candidates.");
                 // Call the function that checks *only* the addresses in 'currentCandidates'
                 resultingCandidates = refineCandidates(pid, currentCandidates, currentNumber, true); // Pass verbose flag
                 shareInfo.updateLastSearchedValue(currentNumber); // Update state: the list now corresponds to 'currentNumber'
            }

            // --- Step 4b: Update the global candidate list ---
            // Store the results (either from scan, refine, or pass-through) for the next cycle
            shareInfo.updateVoidPoitersFinaly(resultingCandidates);

            // --- Step 4c: Check if criteria met for write request ---
            size_t finalAddressCount = resultingCandidates.size();
            // If we narrowed down to 1-3 candidates, prompt the user to write a value
            if (finalAddressCount > 0 && finalAddressCount <= 3) {
                 LOG_INFO("Found " + std::to_string(finalAddressCount) + " candidate addresses. Requesting user input for memory write.");
                 // Set flags indicating a write is requested and input is needed
                 shareInfo.writeValueRequestPending.store(true);
                 shareInfo.writeValueInputReady.store(false); // Input is not ready yet

                 // Get the main window handle to post a message to its thread
                 HWND hMainWindow = nullptr;
                 { // Scope for mutex lock
                     std::lock_guard<std::mutex> lock(shareInfo.dataMutex);
                     hMainWindow = shareInfo.g_hWnd; // Assumes g_hWnd is valid and set elsewhere
                 }

                 // Safely post the message to the main GUI thread
                 if (hMainWindow && IsWindow(hMainWindow)) {
                    // WM_APP_REQUEST_WRITE_VALUE tells the main thread to open the input dialog
                    if (!PostMessage(hMainWindow, WM_APP_REQUEST_WRITE_VALUE, 0, 0)) {
                         DWORD error = GetLastError();
                         LOG_ERROR("Failed to post WM_APP_REQUEST_WRITE_VALUE message. Error: " + std::to_string(error));
                         shareInfo.writeValueRequestPending.store(false); // Reset flag if posting failed
                    } else {
                         LOG_INFO("Posted WM_APP_REQUEST_WRITE_VALUE to main window.");
                    }
                 } else {
                     LOG_ERROR("Cannot request write value: Main window handle is invalid or NULL.");
                     shareInfo.writeValueRequestPending.store(false); // Reset flag if window invalid
                 }
                 // The actual write happens later, triggered by the GUI thread after user input
            }
            // If criteria not met (0 or >3 addresses), ensure write flags are clear
            else {
                if (finalAddressCount == 0 && currentNumber != lastValue) { // Log only if a scan/refine happened and yielded nothing
                     LOG_INFO("No addresses remaining after scan/refinement for value " + std::to_string(currentNumber));
                 } else if (finalAddressCount > 3 && currentNumber != lastValue) { // Log if scan/refine happened but too many results
                     LOG_INFO("Found " + std::to_string(finalAddressCount) + " addresses for value " + std::to_string(currentNumber) + ". Refine further. No write requested.");
                 }
                // Ensure flags are reset if we are not requesting a write this cycle
                shareInfo.writeValueRequestPending.store(false);
                shareInfo.writeValueInputReady.store(false);
            }

        // Catch potential errors during string-to-integer conversion
        } catch (const std::invalid_argument& e) {
             LOG_WARNING("Invalid argument during number conversion (stoi): '" + extractedNumberString + "'. Error: " + e.what());
             // Reset state on conversion error? Maybe clear candidates.
             shareInfo.writeValueRequestPending.store(false);
             shareInfo.writeValueInputReady.store(false);
             // shareInfo.updateVoidPoitersFinaly({});
             // shareInfo.updateLastSearchedValue(INT_MIN);
        } catch (const std::out_of_range& e) {
             LOG_WARNING("Out of range during number conversion (stoi): '" + extractedNumberString + "'. Error: " + e.what());
             // Reset state on conversion error?
             shareInfo.writeValueRequestPending.store(false);
             shareInfo.writeValueInputReady.store(false);
             // shareInfo.updateVoidPoitersFinaly({});
             // shareInfo.updateLastSearchedValue(INT_MIN);
        }
    } else {
         // --- Step 5: Handle Case: No Number Found in OCR Text ---
         // Optional: LOG_INFO("No numbers found in OCR text."); // Can be noisy if OCR often fails

         // Reset write flags if no number found
         shareInfo.writeValueRequestPending.store(false);
         shareInfo.writeValueInputReady.store(false);

         // If OCR consistently fails to find a number, reset the search state
         // to prevent trying to refine an old, irrelevant list indefinitely.
         int lastValue = shareInfo.getLastSearchedValue();
         if (lastValue != INT_MIN) { // Only reset if we were actively searching before
            LOG_INFO("Resetting candidates and last searched value due to missing OCR number.");
            shareInfo.updateVoidPoitersFinaly({});    // Clear candidate list
            shareInfo.updateLastSearchedValue(INT_MIN); // Reset last value indicator
         }
    }
} // End of regiex_In::ReturnFromRex