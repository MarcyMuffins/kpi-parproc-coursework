#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <bitset>

#include "inverted_index.h"

namespace fs = std::filesystem;

int main() {
    std::cout << "Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;
    std::cout << "Enter full path for the folder you want scanned:" << std::endl;
    std::string p;
    std::cin >> p;
    fs::path curr(p);
    std::vector<std::string> files;
    std::cout << std::endl;

    for (const auto& entry : fs::recursive_directory_iterator(curr, fs::directory_options::follow_directory_symlink)) {
        /*if (fs::is_directory(entry)) {
            std::cout << "[+]" << entry.path().filename() << std::endl;
        }
        else */
        if (fs::is_regular_file(entry)) {
            //std::cout << entry.path() << " " << entry.file_size() << std::endl;
            files.push_back(entry.path().generic_string());
        }
    }

    inverted_index index(files);
    std::cout << "Do you want to print out the index? y/n" << std::endl;
    std::cin >> p;
    if (p == "y") {
        std::cout << ":3" << std::endl;
        index.debug_list_files();
    }

    std::vector<std::string> query;
    std::cout << std::endl << "Enter word(s) you want to search for. Type 'n' to end the list" << std::endl;
    while (1) {
        std::cin >> p;
        if (p == "n") {
            break;
        }
        query.emplace_back(p);
    }
    std::cout << "Searching for ";
    for (const auto& q : query) {
        std::cout << '"' << q << '"' << " ";
    }
    std::cout << std::endl << std::endl;
    std::vector<std::string> search_result = index.search(query);
    std::cout << "Found in: " << std::endl;
    for (const auto& r : search_result) {
        std::cout << " "  << r << std::endl;
    }


    return 0;
}