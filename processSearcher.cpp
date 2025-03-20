#include "shareInfo.h"
#include "consoleHandler.h"
#include "errorHandler.h"
//==================//
#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <string>
#include <thread>

DWORD ProcessSearcher(const std::string& name , bool verbose = false){
    std::wstring w_name(name.begin(),name.end()); 

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSnapshot == INVALID_HANDLE_VALUE){
        if (verbose){
            LOG_ERROR("Failed to get handle for snapshot: " + std::to_string(GetLastError()));
        }
        return 0;
    }
    REGISTER_HANDLE(hSnapshot);
    PROCESSENTRY32 proc;
    proc.dwSize = sizeof(PROCESSENTRY32);

    if(!Process32First(hSnapshot, &proc)){
        if (verbose){
            LOG_ERROR("Failed to get process info: " + std::to_string(GetLastError()));
        }
        CloseHandle(hSnapshot);
        UNREGISTER_HANDLE(hSnapshot);
        return 0;
    }

    do{
        if (w_name.compare(proc.szExeFile) == 0){
            CloseHandle(hSnapshot);
            return proc.th32ProcessID;
            UNREGISTER_HANDLE(hSnapshot);
        }
        
    }while (Process32Next(hSnapshot, &proc));

    CloseHandle(hSnapshot);
    UNREGISTER_HANDLE(hSnapshot);
    return 0;
}



void SearchForProcessLoop(bool verbose = false){
    while (shareInfo.isRunning.load()){
        DWORD processS = ProcessSearcher(shareInfo.getUserInput());
        shareInfo.updateThePIDOfProsses(processS);
        if (verbose){
            LOG_INFO(std::to_string(processS));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
}