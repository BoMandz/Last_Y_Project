// valueSearch.h
#pragma once
#include <vector>
#include <stdint.h>
#include <windows.h> // Needed for HANDLE, DWORD etc.

// Performs the initial, full memory scan
std::vector<uintptr_t> searchMemoryForInt(DWORD pid, int value, bool verbose = true); // Use DWORD for pid

// *** NEW: Refines an existing list of candidates ***
// Takes a list of addresses and keeps only those currently holding 'newValue'
std::vector<uintptr_t> refineCandidates(DWORD pid, const std::vector<uintptr_t>& candidates, int newValue, bool verbose = true); // Use DWORD

// Remove performSingleValueSearch declaration - it's no longer needed
// void performSingleValueSearch(bool verbose = true);