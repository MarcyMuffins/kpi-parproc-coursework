#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <ranges>
#include <unordered_set>
#include "inverted_index.h"

//TODO: Add word position tracking?

#define DEBUG true

std::string inverted_index::clean_string(std::string input) {
    std::string cleaned;

    // видалення тегу <br />
    size_t pos;
    std::string sub = "<br />";
    while ((pos = input.find(sub)) != std::string::npos) {
        input.erase(pos, sub.length());
        input.insert(pos, " ");
    }
    for (int i = 0; i < input.size(); i++) {
        /*if (input[i] == '<') {
            for (int j = i; j < input.size(); j++) {
                if (input[j] == '>') {
                    i = j + 1;
                    break;
                }
            }
            cleaned += ' ';
            break;
        }*/
        char ch = input[i];
        if ((ch & 0b11100000) == 0b11000000) { //2 символьний широкий символ
            cleaned += ch;
            i++;
            cleaned += input[i];
        }
        else if ((ch & 0b11110000) == 0b11100000) { //3 символьний широкий символ
            cleaned += ch;
            i++;
            cleaned += input[i];
            i++;
            cleaned += input[i];
        }
        else if (ch == ',') {
            cleaned += ' ';
        }
        else if (ch == '/') {
            cleaned += ' ';
        }
        else if (std::isalnum(ch) || std::isspace(ch)) {
            cleaned += std::tolower(ch); // Забираємо капіталізацію
        }
    }
    return cleaned;
}


inverted_index::inverted_index(std::vector<std::string>& filenames) {
    write_lock _(m_rw_lock);
    int count = 0;
    int progress = 0;
    int size = filenames.size();
    for (auto i : filenames) {
        std::ifstream file(i);
        if (!file.is_open()) {
            std::cerr << "Unable to open the file!" << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf(); // Читаємо файл у рядок
        file.close();

        std::string fileContent = buffer.str();
        //std::cout << "File contents:\n" << fileContent << "\n" << std::endl;

        // Очищення вмісту
        std::string cleanedContent = clean_string(fileContent);
        //std::cout << "Cleaned text:\n" << cleanedContent << "\n" << std::endl;

        std::istringstream stream(cleanedContent);
        std::string word;
        std::vector<std::string> temp;
        while (stream >> word) {
            if (std::count(dict[word].begin(), dict[word].end(), i) == 0) {
                dict[word].emplace_back(i);
            }
        }
        if (DEBUG) {
            count++;
            if (count >= size / 10) {
                count = 0;
                progress += 10;
                std::cout << progress << "%" << std::endl;
            }
        }
    }
}


void inverted_index::add_file(std::string& filename) {
    write_lock _(m_rw_lock);
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Unable to open the file!" << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Читаємо файл у рядок
    file.close();

    std::string fileContent = buffer.str();
    //std::cout << "File contents:\n" << fileContent << "\n" << std::endl;

    // Очищення вмісту
    std::string cleanedContent = clean_string(fileContent);
    //std::cout << "Cleaned text:\n" << cleanedContent << "\n" << std::endl;

    std::istringstream stream(cleanedContent);
    std::string word;
    std::vector<std::string> temp;
    while (stream >> word) {
        if (std::count(dict[word].begin(), dict[word].end(), filename) == 0) {
            dict[word].emplace_back(filename);
        }
    }
}

void inverted_index::debug_list_files() {
    for (const auto& [key, value] : dict) {
        std::cout << '[' << key << "] = \n";
        for (const auto& val : value) {
            std::cout << " " << val << "\n";
        }
        std::cout << "\n";
    }
}

std::vector<std::string> inverted_index::search(std::vector<std::string>& word_query) {
    read_lock _(m_rw_lock);
    std::vector<std::string> found_files;
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

        std::string query = word_query[0];
        found_files = dict[query];
        return found_files;
    } else {
        std::string query = word_query[0];
        std::vector<std::string> next_found_files;

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

        std::vector<std::string> overlapped_found_files;
        //going through every subsequent word in the query
        for (int i = 1; i < word_query.size(); i++) {
            next_found_files = dict[word_query[i]];
            //comparing every file in the previous query overlap with the next query
            for (int j = 0; j < found_files.size(); j++) {
                for (int k = 0; k < next_found_files.size(); k++) {
                    if (found_files[j] == next_found_files[k] 
                        && std::count(overlapped_found_files.begin(), overlapped_found_files.end(), found_files[j]) == 0) {
                        overlapped_found_files.emplace_back(found_files[j]);
                    }
                }
            }
        }
        return overlapped_found_files;
    }
}