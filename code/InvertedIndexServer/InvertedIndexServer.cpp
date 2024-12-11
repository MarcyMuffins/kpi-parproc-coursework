#include <iostream>
#include <chrono>
#include <random>
#include <thread> 
#include <filesystem>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <bitset>
#include <utility>
#include <io.h>
#include <fcntl.h>

#include "thread_pool.h"
#include "inverted_index.h"

namespace fs = std::filesystem;


std::random_device rd;  // a seed source for the random number engine
std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
std::uniform_int_distribution<> distrib1(1, 4);
std::uniform_int_distribution<> distrib2(0, 9);
std::uniform_int_distribution<> distrib3(5000, 10000);

std::wstring options[10] = { L"vampyr", L"the", L"woman", L"movie" , L"and" , L"of" , L"bad" , L"good" , L"man" , L"a" };

std::pair<std::vector<std::wstring>, std::vector<std::wstring>> task(inverted_index& index) {
    //size_t time = distrib3(gen);
    //std::this_thread::sleep_for(std::chrono::milliseconds(time));
    // movie vampyr of man
    int amount = distrib1(gen);
    std::vector<std::wstring> words = { L"vampyr", L"woman", L"of", L"man"};
    /*
    for (int i = 0; i < amount; i++) {
        words.push_back(options[distrib2(gen)]);
    }*/
    std::vector<std::wstring> res = index.search(words);
    return { words, res };
}

int main()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    std::wcout << L"Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;
    std::wcout << L"Enter full path for the folder you want scanned:" << std::endl;
    std::wstring p;
    std::wcin >> p;
    fs::path curr(p);
    std::vector<std::wstring> files;
    std::wcout << std::endl;

    std::wcout << L"Scanning folder and subfolders for files." << std::endl;
    for (const auto& entry : fs::recursive_directory_iterator(curr, fs::directory_options::follow_directory_symlink)) {
        /*if (fs::is_directory(entry)) {
            std::cout << "[+]" << entry.path().filename() << std::endl;
        }
        else */
        if (fs::is_regular_file(entry)) {
            //std::cout << entry.path() << " " << entry.file_size() << std::endl;
            files.push_back(entry.path().generic_wstring());
        }
    }
    std::wcout << L"Finished scanning, found " << files.size() << L" files." << std::endl;

    std::wcout << L"Building the file index." << std::endl;
    inverted_index index(files);
    std::wcout << L"Done." << std::endl;


    std::wcout << L"Do you want to print out the index? (It will be really long) y/n" << std::endl;
    std::wcin >> p;
    if (p == L"y") {
        std::wcout << L":3" << std::endl;
        index.debug_list_files();
    }



    int task_count = 10;
    thread_pool pool;
    pool.initialize(4, true);
    //std::cout << "Starting " << task_count << " tasks." << std::endl;
    for (int i = 0; i < task_count; i++) {
        size_t id = pool.add_task(task, std::ref(index));
        //std::cout << "Added task " << id << " to the thread pool." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    pool.terminate_now();

    while (true) {
        size_t id;
        std::wcout << std::endl << L"Enter task ID to check status: ";
        std::wcin >> id;
        if (id == -1) {
            break;
        }
        std::pair<std::vector<std::wstring>, std::vector<std::wstring>> status = pool.get_status(id);
        if (status.first.size() != 0) {
            std::wcout << L"Task " << id << L" with the query ( ";
            for (const auto& i : status.first) {
                std::wcout << i << " ";
            }
            std::wcout << L") found in:\n";
            for (const auto& i : status.second) {
                std::wcout << L" " << i << std::endl;
            }
        }
    }
    return 0;


}