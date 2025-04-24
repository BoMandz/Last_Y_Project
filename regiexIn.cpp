#include "regiexIn.h"
#include "shareInfo.h"
#include "errorHandler.h"
#include "valueSearch.h"
//=====================//
#include <windows.h>
#include <regex>
#include <string>
#include <vector>
#include <limits>
#include <system_error>
#include <mutex>

regiex_In regiexIn;

void regiex_In::ReturnFromRex() {
    const std::regex number_pattern(R"(\d+)");

    std::string ocrText = shareInfo.getTheString();
    DWORD pid = shareInfo.getThePIDOfProsses();

    if (pid == 0) {
        int lastValue = shareInfo.getLastSearchedValue();
        if (lastValue != INT_MIN) {
            LOG_INFO("ReturnFromRex: Target PID became 0, resetting search state.");
            shareInfo.updateVoidPoitersFinaly({});
            shareInfo.updateLastSearchedValue(INT_MIN);
        }
        return;
    }

    if (shareInfo.writeValueRequestPending.load() || shareInfo.writeValueInputReady.load()) {
         LOG_INFO("ReturnFromRex: Skipping scan/refinement - Write operation is pending.");
         return;
    }

    auto words_begin = std::sregex_iterator(ocrText.begin(), ocrText.end(), number_pattern);
    auto words_end = std::sregex_iterator();
    std::string extractedNumberString;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        extractedNumberString += i->str();
    }

    if (!extractedNumberString.empty()) {
        try {
            int currentNumber = std::stoi(extractedNumberString);
            shareInfo.updateTheINT(currentNumber);

            int lastValue = shareInfo.getLastSearchedValue();
            std::vector<uintptr_t> currentCandidates;
            {
                std::lock_guard<std::mutex> lock(shareInfo.dataMutex);
                currentCandidates = shareInfo.voidPoitersFinaly;
            }

            std::vector<uintptr_t> resultingCandidates;

            if (currentNumber == lastValue) {
                resultingCandidates = currentCandidates;
            }
            else if (lastValue == INT_MIN || currentCandidates.empty()) {
                 if (currentCandidates.empty() && lastValue != INT_MIN) {
                     LOG_INFO("Candidate list empty, performing new initial scan for value: " + std::to_string(currentNumber));
                 } else {
                     LOG_INFO("Performing initial scan for value: " + std::to_string(currentNumber));
                 }
                 resultingCandidates = searchMemoryForInt(pid, currentNumber, true);
                 shareInfo.updateLastSearchedValue(currentNumber);
            }
            else {
                 LOG_INFO("Value changed (" + std::to_string(lastValue) + " -> " + std::to_string(currentNumber) + "). Refining " + std::to_string(currentCandidates.size()) + " candidates.");
                 resultingCandidates = refineCandidates(pid, currentCandidates, currentNumber, true);
                 shareInfo.updateLastSearchedValue(currentNumber);
            }

            shareInfo.updateVoidPoitersFinaly(resultingCandidates);

            size_t finalAddressCount = resultingCandidates.size();
            if (finalAddressCount > 0 && finalAddressCount <= 3) {
                 LOG_INFO("Found " + std::to_string(finalAddressCount) + " candidate addresses. Requesting user input for memory write.");
                 shareInfo.writeValueRequestPending.store(true);
                 shareInfo.writeValueInputReady.store(false);

                 HWND hMainWindow = nullptr;
                 {
                     std::lock_guard<std::mutex> lock(shareInfo.dataMutex);
                     hMainWindow = shareInfo.g_hWnd;
                 }

                 if (hMainWindow && IsWindow(hMainWindow)) {
                    if (!PostMessage(hMainWindow, WM_APP_REQUEST_WRITE_VALUE, 0, 0)) {
                         DWORD error = GetLastError();
                         LOG_ERROR("Failed to post WM_APP_REQUEST_WRITE_VALUE message. Error: " + std::to_string(error));
                         shareInfo.writeValueRequestPending.store(false);
                    } else {
                         LOG_INFO("Posted WM_APP_REQUEST_WRITE_VALUE to main window.");
                    }
                 } else {
                     LOG_ERROR("Cannot request write value: Main window handle is invalid or NULL.");
                     shareInfo.writeValueRequestPending.store(false);
                 }
            }
            else {
                if (finalAddressCount == 0 && currentNumber != lastValue) {
                     LOG_INFO("No addresses remaining after scan/refinement for value " + std::to_string(currentNumber));
                 } else if (finalAddressCount > 3 && currentNumber != lastValue) {
                     LOG_INFO("Found " + std::to_string(finalAddressCount) + " addresses for value " + std::to_string(currentNumber) + ". Refine further. No write requested.");
                 }
                shareInfo.writeValueRequestPending.store(false);
                shareInfo.writeValueInputReady.store(false);
            }

        } catch (const std::invalid_argument& e) {
             LOG_WARNING("Invalid argument during number conversion (stoi): '" + extractedNumberString + "'. Error: " + e.what());
             shareInfo.writeValueRequestPending.store(false);
             shareInfo.writeValueInputReady.store(false);
        } catch (const std::out_of_range& e) {
             LOG_WARNING("Out of range during number conversion (stoi): '" + extractedNumberString + "'. Error: " + e.what());
             shareInfo.writeValueRequestPending.store(false);
             shareInfo.writeValueInputReady.store(false);
        }
    } else {
         shareInfo.writeValueRequestPending.store(false);
         shareInfo.writeValueInputReady.store(false);

         int lastValue = shareInfo.getLastSearchedValue();
         if (lastValue != INT_MIN) {
            LOG_INFO("Resetting candidates and last searched value due to missing OCR number.");
            shareInfo.updateVoidPoitersFinaly({});
            shareInfo.updateLastSearchedValue(INT_MIN);
         }
    }
}
