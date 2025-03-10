#include "shareInfo.h"
#include "consoleHandler.h"
#include "libs/errorHandler.h"
//==================//
#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <string>
#include <thread>

DWORD ProcessSearcher(const std::string& name){
    std::wstring w_name(name.begin(),name.end()); 

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSnapshot == INVALID_HANDLE_VALUE){
        LOG_ERROR("Failed to get handle for snapshot: " + std::to_string(GetLastError()));
        return 0;
    }
    PROCESSENTRY32 proc;
    proc.dwSize = sizeof(PROCESSENTRY32);

    if(!Process32First(hSnapshot, &proc)){
        LOG_ERROR("Failed to get process info: " + std::to_string(GetLastError()));
        CloseHandle(hSnapshot);
        return 0;
    }

    do{
        if (w_name.compare(proc.szExeFile) == 0){
            CloseHandle(hSnapshot);
            return proc.th32ProcessID;
        }
        
    }while (Process32Next(hSnapshot, &proc));

    CloseHandle(hSnapshot);
    return 0;
}



void SearchForProcessLoop(){
    while (shareInfo.isRunning.load()){
        shareInfo.updateThePIDOfProsses(ProcessSearcher(shareInfo.getUserInput()));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
}