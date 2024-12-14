#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <ranges>
#include <unordered_set>
#include <cwctype>
#include <io.h>
#include <fcntl.h>
#include "inverted_index.h"

//TODO: Add word position tracking?

#define DEBUG true

std::wstring inverted_index::clean_string(std::wstring input) {
    std::wstring cleaned;

    // видалення тегу <br />
    size_t pos;
    std::wstring sub = L"<br />";
    while ((pos = input.find(sub)) != std::wstring::npos) {
        input.erase(pos, sub.length());
        input.insert(pos, L" ");
    }
    for (int i = 0; i < input.length(); i++) {
        wchar_t ch = input.at(i);
        if (std::iswalnum(ch) || std::iswspace(ch)) {
            cleaned += std::towlower(ch); // Забираємо капіталізацію
        } else {
            cleaned += L' ';
        }
    }
    return cleaned;
}


inverted_index::inverted_index(std::vector<std::wstring>& filenames) {
    write_lock _(m_rw_lock);
    int count = 0;
    int progress = 0;
    int size = filenames.size();
    for (auto i : filenames) {
        std::wifstream file(i);
        if (!file.is_open()) {
            std::wcerr << L"Unable to open the file!" << std::endl;
            return;
        }
        file.imbue(std::locale("en_US.UTF-8"));

        std::wstringstream buffer;
        buffer << file.rdbuf(); // Читаємо файл у рядок
        file.close();

        std::wstring file_content = buffer.str();
        //std::cout << "File contents:\n" << fileContent << "\n" << std::endl;

        // Очищення вмісту
        std::wstring cleaned_content = clean_string(file_content);
        //std::cout << "Cleaned text:\n" << cleanedContent << "\n" << std::endl;

        std::wistringstream stream(cleaned_content);
        std::wstring word;
        std::vector<std::wstring> temp;
        while (stream >> word) {
            //std::wcout << word << std::endl;
            if (std::count(dict[word].begin(), dict[word].end(), i) == 0) {
                dict[word].push_back(i);
            }
        }
        if (DEBUG) {
            count++;
            if (count >= size / 10) {
                count = 0;
                progress += 10;
                std::wcout << progress << L"%" << std::endl;
            }
        }
    }
}


void inverted_index::add_file(std::wstring& filename) {
    write_lock _(m_rw_lock);
    std::wifstream file(filename);
    if (!file.is_open()) {
        std::wcerr << L"Unable to open the file!" << std::endl;
        return;
    }

    std::wstringstream buffer;
    buffer << file.rdbuf(); // Читаємо файл у рядок
    file.close();

    std::wstring fileContent = buffer.str();
    //std::cout << "File contents:\n" << fileContent << "\n" << std::endl;

    // Очищення вмісту
    std::wstring cleanedContent = clean_string(fileContent);
    //std::cout << "Cleaned text:\n" << cleanedContent << "\n" << std::endl;

    std::wistringstream stream(cleanedContent);
    std::wstring word;
    std::vector<std::wstring> temp;
    while (stream >> word) {
        if (std::count(dict[word].begin(), dict[word].end(), filename) == 0) {
            dict[word].push_back(filename);
        }
    }
}

void inverted_index::debug_list_files() {
    for (const auto& [key, value] : dict) {
        std::wcout << L'[' << key << L"] = \n";
        for (const auto& val : value) {
            std::wcout << L" " << val << L"\n";
        }
        std::wcout << L"\n";
    }
}

std::vector<std::wstring> inverted_index::search(std::vector<std::wstring>& word_query) {
    read_lock _(m_rw_lock);
    std::vector<std::wstring> found_files;
    if (word_query.size() == 0) {
        return found_files;
    } else if (word_query.size() == 1) {

        //debug
        /*
        for (const auto& i : word_query) {
            std::cout << '"' << i << "\" found in:" << "\n";
            for (const auto& j : dict[i]) {
                std::cout << ' ' << j << '\n';
            }
            std::cout << std::endl;
        }
        */
        //debug

        std::wstring query = word_query[0];
        found_files = dict[query];
        return found_files;
    } else {
        std::wstring query = word_query[0];
        std::vector<std::wstring> next_found_files;

        found_files = dict[query];

        //debug
        /*
        for (const auto& i : word_query) {
            std::cout << i << " found in:" << "\n";
            for (const auto& j : dict[i]) {
                std::cout << ' ' << j << '\n';
            }
            std::cout << std::endl;
        }
        */
        //debug

        //How the next bit SHOULD work
        /*
        Find list of files from the first query

        For every word in the query after it 
            Fetch next list of files for the next word

        
        
        */
            
        std::vector<std::wstring> overlapped_found_files;
        //going through every subsequent word in the query
        for (int i = 1; i < word_query.size(); i++) {
            next_found_files = dict[word_query[i]];
            //comparing every file in the previous query overlap with the next query
            for (int j = 0; j < found_files.size(); j++) {
                for (int k = 0; k < next_found_files.size(); k++) {
                    if (found_files[j] == next_found_files[k] 
                        && std::count(overlapped_found_files.begin(), overlapped_found_files.end(), found_files[j]) == 0) {
                        overlapped_found_files.push_back(found_files[j]);
                    }
                }
            }
            found_files = overlapped_found_files;
            overlapped_found_files.clear();
        }
        return found_files;
    }
}