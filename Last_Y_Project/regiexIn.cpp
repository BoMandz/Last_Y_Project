#include "regiexIn.h"
#include "shareInfo.h" 
//==================//
#include <regex>
#include <iostream>

// Define the external variable
regiex_In regiexIn;

// Implement the ReturnFromRex method
void regiex_In::ReturnFromRex() {
    std::regex number_pattern(R"(\d+)"); // Regex pattern to match numbers

    // Access the text from shareInfo
    std::string text = shareInfo.textReturned;

    // Use regex iterator to find all matches
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), number_pattern);
    auto words_end = std::sregex_iterator();

    std::string output;

    // Concatenate all matched numbers
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        output += i->str();
    }

    // If numbers are found, update the integer in shareInfo
    if (!output.empty()) {
        try {
            int number = std::stoi(output);
            shareInfo.updateTheINT(number);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid argument: " << e.what() << std::endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "Out of range: " << e.what() << std::endl;
        }
    }
}