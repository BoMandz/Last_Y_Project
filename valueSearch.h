#pragma once
#include <vector>
#include <stdint.h>
#include <windows.h> 

std::vector<uintptr_t> searchMemoryForInt(DWORD pid, int value, bool verbose = true); 

std::vector<uintptr_t> refineCandidates(DWORD pid, const std::vector<uintptr_t>& candidates, int newValue, bool verbose = true); 