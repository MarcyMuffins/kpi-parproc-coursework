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

#include "thread_pool.h"
#include "inverted_index.h"

namespace fs = std::filesystem;


std::random_device rd;  // a seed source for the random number engine
std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
std::uniform_int_distribution<> distrib1(1, 4);
std::uniform_int_distribution<> distrib2(0, 9);
std::uniform_int_distribution<> distrib3(5000, 10000);

std::string options[10] = { "vampyr", "the", "woman", "movie" , "and" , "of" , "bad" , "good" , "man" , "a" };

std::pair<std::vector<std::string>, std::vector<std::string>> task(inverted_index& index) {
    //size_t time = distrib3(gen);
    //std::this_thread::sleep_for(std::chrono::milliseconds(time));

    int amount = distrib1(gen);
    std::vector<std::string> words;
    for (int i = 0; i < amount; i++) {
        words.push_back(options[distrib2(gen)]);
    }
    std::vector<std::string> res = index.search(words);
    return { words, res };
}

int main()
{
    std::cout << "Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;
    std::cout << "Enter full path for the folder you want scanned:" << std::endl;
    std::string p;
    std::cin >> p;
    fs::path curr(p);
    std::vector<std::string> files;
    std::cout << std::endl;

    std::cout << "Scanning folder and subfolders for files." << std::endl;
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
    std::cout << "Finished scanning, found " << files.size() << " files." << std::endl;

    std::cout << "Building the file index." << std::endl;
    inverted_index index(files);
    std::cout << "Done." << std::endl;
    std::cout << "Do you want to print out the index? (It will be really long) y/n" << std::endl;
    std::cin >> p;
    if (p == "y") {
        std::cout << ":3" << std::endl;
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

    std::this_thread::sleep_for(std::chrono::seconds(10));
    pool.terminate_now();

    while (true) {
        size_t id;
        std::cout << std::endl << "Enter task ID to check status: ";
        std::cin >> id;
        if (id == -1) {
            break;
        }
        std::pair<std::vector<std::string>, std::vector<std::string>> status = pool.get_status(id);
        if (status.first.size() != 0) {
            std::cout << "Task " << id << " with the query ( ";
            for (const auto& i : status.first) {
                std::cout << i << " ";
            }
            std::cout << ") found in:\n";
            for (const auto& i : status.second) {
                std::cout << " " << i << std::endl;
            }
        }
    }
    return 0;


}