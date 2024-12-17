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
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <thread>

#include "thread_pool.h"
#include "inverted_index.h"

#pragma comment(lib, "ws2_32.lib")

#define buffer_size 4096

namespace fs = std::filesystem;

bool DEBUG = false;

std::mutex print_mtx;
std::mutex log_mtx;

// Formats current time into a proper string for logging
// Example: 2024_12_16_15_52_32
std::wstring get_formatted_time(){
    time_t now = std::time(nullptr);
    tm local_now;
    localtime_s(&local_now, &now); 
    std::wostringstream woss;
    woss << std::put_time(&local_now, L"%Y_%m_%d_%H_%M_%S");
    return woss.str();
}

// Logging disconnects and closing the socket. Separate function because it's used a LOT
inline void handle_disconnect(size_t id, SOCKET& client_socket, std::wofstream& log_file){
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        //log_file.open(log_path, std::ios::app);
        log_file << get_formatted_time();
        log_file << L": Client " << id << " disconnected." << std::endl;
        //log_file.close();

    }
    closesocket(client_socket);
}

// Same as above. Since the receive function now handles three error cases (empty buffer disconnect, error timeout and misc error)
// I put it into it's own function to save space. 
bool receive_buffer(size_t& id, SOCKET& client_socket, std::wofstream& log_file, char* buffer, int len){
    //std::wofstream log_file;
    int bytes_received = recv(client_socket, buffer, len, 0);
    if (bytes_received == 0) {
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" disconnected. " << std::endl;
        }
        handle_disconnect(id, client_socket, log_file);
        return false;
    } else if (bytes_received < 0) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << " timed out." << std::endl;
                //log_file.close();
            }
            if (DEBUG) {
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" timeout. " << std::endl;
            }
        } else {
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << " error." << std::endl;
                //log_file.close();
            }
            if (DEBUG) {
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" Error. " << std::endl;
            }
        }
        handle_disconnect(id, client_socket, log_file);
        return false;
    }
    return true;
}

// Main client processing function. Handles both admins and clients.
void process_client(size_t id, SOCKET client_socket, inverted_index& index, std::wofstream& log_file, fs::path& db_path){
    //std::wofstream log_file;
    char buffer[buffer_size];
    int timeout = 60 * 1000; // 60 second timeout
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    // Sending a signal to the client that it started being processed
    char start_buf[2] = { 'S', '\0' };
    if (send(client_socket, (char*)start_buf, sizeof(start_buf), 0) < 0) {
        std::wcout << L"CLT " << id << L" error sending message" << std::endl;
        handle_disconnect(id, client_socket, log_file);
        return;
    }
    // This scoped block of code will appear every time the program logs something.
    // Minor performance hit because every time you log something you have to open and close the file.
    // TODO: Log queue?
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        //log_file.open(log_path, std::ios::app);
        log_file << get_formatted_time();
        log_file << L": Client " << id << " started processing." << std::endl;
        //log_file.close();
    }

    char type_buf[2] = {0};
    if(receive_buffer(id, client_socket, log_file, type_buf, sizeof(type_buf)) == false){
        return;
    }
    // Admin branch
    if (type_buf[0] == 'A'){
        if (DEBUG) {
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" logged in as admin. " << std::endl;
        }
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " logged in as admin." << std::endl;
            //log_file.close();
        }

        // Receive the amount of files to be added
        unsigned char n_files_buf[5] = {0};
        if(receive_buffer(id, client_socket, log_file, (char*)n_files_buf, sizeof(n_files_buf)) == false){
            return;
        }
        unsigned int n_files = (n_files_buf[0] << 24) | (n_files_buf[1] << 16) | (n_files_buf[2] << 8) | (n_files_buf[3]);
        if(n_files == 0){
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << L" wants to send no files." << std::endl;
                //log_file.close();
            }
            if (DEBUG) {
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" wants to send no files." << std::endl;
            }
            handle_disconnect(id, client_socket, log_file);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " wants to send " << n_files << " file(s)." << std::endl;
            //log_file.close();
        }
        if (DEBUG) {
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" wants to send " << n_files << " file(s)." << std::endl;
        }
        std::vector<std::wstring> files_to_add;

        for(int i = 0; i < n_files; i++){
            // Get buffer for filename (truncate to current directory)
            if(receive_buffer(id, client_socket, log_file, buffer, buffer_size) == false){
                return;
            }
            int wide_string_size = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
            std::wstring filepath(wide_string_size, 0);
            MultiByteToWideChar(CP_UTF8, 0, buffer, -1, &filepath[0], wide_string_size);
            // MultiByteToWideChar() adds an extra terminator byte to the end, so we gotta remove it
            filepath.pop_back();
            fs::path new_filepath = db_path / filepath;
            files_to_add.push_back(new_filepath.generic_wstring());

            unsigned char filesize_buf[3] = {0};
            if(receive_buffer(id, client_socket, log_file, (char*)filesize_buf, sizeof(filesize_buf)) == false){
                return;
            }

            unsigned short n_file_content_blocks = ((filesize_buf[0] << 8) | (filesize_buf[1]));
            std::wstring file_contents;
            for (int i = 0; i < n_file_content_blocks; i++) {
                memset(buffer, '\0', sizeof(buffer));
                if(receive_buffer(id, client_socket, log_file, buffer, buffer_size) == false){
                    return;
                }
                int file_contents_size = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
                std::wstring temp(file_contents_size, 0);
                MultiByteToWideChar(CP_UTF8, 0, buffer, -1, &temp[0], file_contents_size);
                temp.pop_back();
                file_contents += temp;
            }
            if (!new_filepath.parent_path().empty()) {
                fs::create_directories(new_filepath.parent_path());
            }
            std::wofstream out_file;
            out_file.open(new_filepath, std::ios::out);
            if (out_file.is_open()) {
                out_file << file_contents;
                out_file.close();
                {
                    std::lock_guard<std::mutex> lock(log_mtx);
                    //log_file.open(log_path, std::ios::app);
                    log_file << get_formatted_time();
                    log_file << L": Client " << id << " file written successfully to " << new_filepath << std::endl;
                    //log_file.close();
                }
                if (DEBUG) {
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" file written successfully to: " << std::endl << new_filepath << std::endl;
                }
            } else {
                {
                    std::lock_guard<std::mutex> lock(log_mtx);
                    //log_file.open(log_path, std::ios::app);
                    log_file << get_formatted_time();
                    log_file << L": Client " << id << " failed to write file " << new_filepath << std::endl;
                    //log_file.close();
                }
                if (DEBUG) {
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" failed to write file: " << std::endl << new_filepath << std::endl;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " adding new files to the index. " << std::endl;
            //log_file.close();
        }
        if (DEBUG) {
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" adding new files to the index. " << std::endl;
        }
        index.add_files(files_to_add);
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " finished adding new files to the index. " << std::endl;
            //log_file.close();
        }
        if (DEBUG) {
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" finished adding new files to the index. " << std::endl;
        }
        handle_disconnect(id, client_socket, log_file);
        return;
    }

    // Client "branch"
    std::vector<std::wstring> query = {};
    std::vector<std::wstring> result = {};
    unsigned char n_blocks_buff[2];
    memset(buffer, '\0', sizeof(buffer));
    
    if(receive_buffer(id, client_socket, log_file, buffer, buffer_size) == false){
        return;
    }
    
    int wide_string_size = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
    std::wstring word_query(wide_string_size, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, &word_query[0], wide_string_size);
    word_query.pop_back();

    std::wistringstream stream(word_query);
    std::wstring word;
    while (stream >> word) {
        query.push_back(word);
    }

    if (DEBUG) {
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"CLT " << id << L" query:" << std::endl;
        for(const auto& i : query){
            std::wcout << i << " ";
        }
        std::wcout << std::endl << std::endl;
    }
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        //log_file.open(log_path, std::ios::app);
        log_file << get_formatted_time();
        log_file << L": Client " << id << " query:" << std::endl;
        for(const auto& i : query){
            log_file << i << " ";
        }
        log_file << std::endl;
        //log_file.close();
    }
    // Task in pool starts looking in the index
    // Client waits for response indicating it's done
    // Kind of a bad solution but better than constant pinging I guess
    // Sending that search started
    char status_buf[2] = { 'Q', '\0' };
    if (send(client_socket, (char*)status_buf, sizeof(status_buf), 0) < 0) {
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " error sending message." << std::endl;
            //log_file.close();
        }
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" error sending message" << std::endl;
        }
        handle_disconnect(id, client_socket, log_file);
        return;
    }
    result = index.search(query);
    // Sending that search finished
    status_buf[0] = 'F';
    if (send(client_socket, (char*)status_buf, sizeof(status_buf), 0) < 0) {
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " error sending message." << std::endl;
            //log_file.close();
        }
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" error sending message" << std::endl;
        }
        handle_disconnect(id, client_socket, log_file);
        return;
    }

    // Sending the amount of files found
    unsigned int n_files = result.size();
    unsigned char n_files_buf[5] = {0};
    n_files_buf[0] = (n_files >> 24) & 0xFF;
    n_files_buf[1] = (n_files >> 16) & 0xFF;
    n_files_buf[2] = (n_files >> 8) & 0xFF;
    n_files_buf[3] = n_files & 0xFF; 
    if (send(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << L" error sending message." << std::endl;
            //log_file.close();
        }
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" error sending message" << std::endl;
        }
        handle_disconnect(id, client_socket, log_file);
        return;
    }
    if(n_files == 0){
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " found no files." << std::endl;
            //log_file.close();
        }
        handle_disconnect(id, client_socket, log_file);
        return;
    } else {
        std::lock_guard<std::mutex> lock(log_mtx);
        //log_file.open(log_path, std::ios::app);
        log_file << get_formatted_time();
        // Main culprit of large log files
        // Temporarily removing cuz yeag
        log_file << L": Client " << id << " found query in " << n_files << " files." << std::endl;
        /*
        for(const auto& i : result){
            log_file << " " << i << std::endl;
        }*/
        //log_file.close();
    }
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"CLT " << id << L" found query in " << n_files << " files:" << std::endl;
        for(const auto& i : result){
            std::wcout << " " << i << std::endl;
        }
        std::wcout << std::endl;
    }
    // Sending the filenames(paths)
    for (int i = 0; i < n_files; i++){
        std::string filepath(buffer_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, result[i].c_str(), -1, &filepath[0], buffer_size, nullptr, nullptr);
        if (send(client_socket, filepath.c_str(), buffer_size, 0) < 0) {
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << " error sending message." << std::endl;
                //log_file.close();
            }
            if(DEBUG){
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" error sending message" << std::endl;
            }
            handle_disconnect(id, client_socket, log_file);
            return;
        }
    }

    // Sending files while the client wants them
    while(1){
        unsigned char choice_buf[5] = {0};
        
        if(receive_buffer(id, client_socket, log_file, (char*)choice_buf, sizeof(choice_buf)) == false){
            return;
        }
        int choice = (choice_buf[0] << 24) | (choice_buf[1] << 16) | (choice_buf[2] << 8) | (choice_buf[3]);
        if (choice == 0){
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << " requested no file." << std::endl;
                //log_file.close();
            }
            if(DEBUG){
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" requested no file. Disconnecting." << std::endl;
            }
            handle_disconnect(id, client_socket, log_file);
            return;
        } else {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << id << " requested file " << choice << std::endl;
            //log_file.close();
        }
        if (DEBUG) {
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" requested file " << choice << std::endl;
        }
        choice--;
        std::wifstream file(result[choice]);
        if (!file.is_open()) {
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << " unable to open the file." << std::endl;
                //log_file.close();
            }
            if (DEBUG) {
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcerr << L"CLT " << id  << L" unable to open the file." << std::endl;
            }
            handle_disconnect(id, client_socket, log_file);
            return;
        }
        file.imbue(std::locale("en_US.UTF-8"));
        std::wstringstream file_buffer;
        file_buffer << file.rdbuf();
        file.close();
        std::wstring file_content = file_buffer.str();
        //std::wcout << file_content << std::endl;
        int file_content_size = WideCharToMultiByte(CP_UTF8, 0, &file_content[0], -1, nullptr, 0, nullptr, nullptr);
        std::string file_content_s(file_content_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, &file_content[0], -1, &file_content_s[0], file_content_size, nullptr, nullptr);
        //std::wcout << file_content_size << std::endl;
        unsigned short n_file_content_blocks = (file_content_size / buffer_size) + 1;
        unsigned char filesize_buf[3] = {0};
        filesize_buf[0] = (n_file_content_blocks >> 8) & 0xFF;
        filesize_buf[1] = n_file_content_blocks & 0xFF; 
        if (send(client_socket, (char*)filesize_buf, sizeof(filesize_buf), 0) < 0) {
            {
                std::lock_guard<std::mutex> lock(log_mtx);
                //log_file.open(log_path, std::ios::app);
                log_file << get_formatted_time();
                log_file << L": Client " << id << " error sending message." << std::endl;
                //log_file.close();
            }
            if(DEBUG){
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" error sending message" << std::endl;
            }
            handle_disconnect(id, client_socket, log_file);
            return;
        }
        //std::wcout << n_file_content_blocks << std::endl;
        for (int i = 0; i < n_file_content_blocks; i++){
            memset(buffer, '\0', sizeof(buffer));
            int index = i * (buffer_size-1);
            int copy_len = buffer_size-1;
            // Copying 4095 byte chunks from the file, unless there's less, in which case just copy the remaining file
            if(index + copy_len > file_content_s.length()){
                copy_len = file_content_s.length() - index;
            }
            memcpy(buffer, &file_content_s[index], copy_len);
            if (send(client_socket, buffer, buffer_size, 0) < 0) {
                {
                    std::lock_guard<std::mutex> lock(log_mtx);
                    //log_file.open(log_path, std::ios::app);
                    log_file << get_formatted_time();
                    log_file << L": Client " << id << " error sending message." << std::endl;
                    //log_file.close();
                }
                if(DEBUG){
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" error sending message" << std::endl;
                }
                handle_disconnect(id, client_socket, log_file);
                return;
            }
        }
    }
    handle_disconnect(id, client_socket, log_file);
}

// Scheduler scans database folder for new files every 30 seconds
void scheduler_function(fs::path dir, inverted_index& index, fs::path& log_path){
    const int delay = 30;
    std::wofstream log_file;
    {
        std::lock_guard<std::mutex> lock(log_mtx);
        log_file.open(log_path, std::ios::app);
        log_file << get_formatted_time();
        log_file << L": Started scheduler. Looking for new files every " << delay << L" seconds." << std::endl;
        log_file.close();

    }
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"SCH: Started scheduler." << std::endl;
    }
    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(delay));
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"SCH: Scanning for new files." << std::endl;
        }
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Scheduler scanning for new files." << std::endl;
            log_file.close();
        }
        std::unordered_set<std::wstring> processed_files = index.get_processed_files();
        std::vector<std::wstring> new_files;
        for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::follow_directory_symlink)) {
            if (fs::is_regular_file(entry)) {
                if (!processed_files.contains(entry.path().generic_wstring())){
                    new_files.push_back(entry.path().generic_wstring());
                }
            }
        }
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"SCH: Finished scanning, found " << new_files.size() << L" new files." << std::endl;
        }
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Scheduler finished scanning. Found " << new_files.size() << " new file(s)." << std::endl;
            //log_file.close();
        }
        if(new_files.size() != 0){
            index.add_files(new_files);
        }
    }
}


int main()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    int worker_count = 8;

    //std::wcout << L"Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;
    
    //Debug mode?
    std::wstring choice;
    std::wcout << L"Enable debug mode? y/n" << std::endl;
    std::wcin >> choice;
    if (choice == L"y") {
        DEBUG = true;
    }
    std::wcout << L"Enter number of workers" << std::endl;
    std::wcin >> worker_count;
    //Number of threads?
  
    std::wstring log_name = get_formatted_time();
    log_name += L".log";
    fs::path log_path = fs::current_path() / log_name;

    std::wofstream log_file(log_path);
    log_file.open(log_path);
    if (log_file.is_open()) {
        log_file.close();
        std::wcout << L"Created log: " << std::endl << log_path << std::endl;
    } else {
        std::wcerr << L"Failed to create log: " << std::endl << log_path << std::endl;
        return 0;
    }


    std::wcout << std::endl << L"=== BULDING INDEX ===" << std::endl << std::endl;
    std::wcout << L"Enter full path for the folder you want scanned:" << std::endl;
    std::wstring db_path_str;
    std::wcin >> db_path_str;
    fs::path db_path(db_path_str);
    std::vector<std::wstring> files;
    std::wcout << std::endl;

    log_file.open(log_path, std::ios::app);
    log_file << get_formatted_time();
    log_file << L": Start scanning files." << std::endl;
    //log_file.close();

    // Recursively scan the database folder for files
    std::wcout << L"Scanning folder and subfolders for files." << std::endl;
    for (const auto& entry : fs::recursive_directory_iterator(db_path, fs::directory_options::follow_directory_symlink)) {
        if (fs::is_regular_file(entry)) {
            //std::cout << entry.path() << " " << entry.file_size() << std::endl;
            files.push_back(entry.path().generic_wstring());
        }
    }
    std::wcout << L"Finished scanning, found " << files.size() << L" files." << std::endl;

    //log_file.open(log_path, std::ios::app);
    log_file << get_formatted_time();
    log_file << L": Finished scanning files. Found " << files.size() << L" files. Building index." << std::endl;
    //log_file.close();

    std::wcout << L"Building the file index." << std::endl;

    inverted_index index(files, DEBUG);

    std::wcout << L"Done." << std::endl;

    //log_file.open(log_path, std::ios::app);
    log_file << get_formatted_time();
    log_file << L": Done building index." << std::endl;
    //log_file.close();

    if(DEBUG){
        std::wcout << L"Do you want to print out the index? (It will be really long) y/n" << std::endl;
        std::wcin >> choice;
        if (choice == L"y") {
            std::wcout << L":3" << std::endl;
            index.debug_list_files();
        }
    }


    thread_pool pool;
    pool.initialize(worker_count, DEBUG);

    //log_file.open(log_path, std::ios::app);
    log_file << get_formatted_time();
    log_file << L": Initialized client thread pool with " << worker_count << L" working threads." << std::endl;
    //log_file.close();

    std::wcout << std::endl << L"=== STARTING SERVER ===" << std::endl;

    //log_file.open(log_path, std::ios::app);
    log_file << get_formatted_time();
    log_file << L": Starting server." << std::endl;
    //log_file.close();

    // Initialize WSA variables
    WSADATA wsa_data;
    int wsaerr;
    WORD w_version_requested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(w_version_requested, &wsa_data);

    // Check for initialization success
    if (wsaerr != 0) {
        std::wcout << L"WSAStartup error " << wsa_data.szSystemStatus << std::endl;
        return 0;
    } else {
        std::wcout << L"WSAStartup successful" << std::endl;
    }

    // Creating a TCP socket
    SOCKET socket_server, socket_client;
    //AF_INET - TCP/IP
    //SOCK_STREAM - streaming socket (not datagram)
    //IPPROTO_TCP for TCP, duh
    socket_server = INVALID_SOCKET;
    socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Check for socket creation success
    if (socket_server == INVALID_SOCKET) {
        std::wcout << L"Socket Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    } else {
        std::wcout << L"Socket OK" << std::endl;
    }

    // Address structures
    sockaddr_in addr_server, addr_client;
    // TCP/IP protocol, setting the port and not binding socket to any specific IP
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(6969);
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_server, reinterpret_cast<SOCKADDR*>(&addr_server), sizeof(addr_server)) == SOCKET_ERROR) {
        std::wcout << L"bind() error " << WSAGetLastError() << std::endl;
        closesocket(socket_server);
        WSACleanup();
        return 0;
    } else {
        std::wcout << L"bind() OK" << std::endl;
    }

    //log_file.open(log_path, std::ios::app);
    log_file << get_formatted_time();
    log_file << L": Listening for connections." << std::endl;

    // Listen for incoming connections
    if (listen(socket_server, 256) == SOCKET_ERROR) {
        std::wcout << L"listen() error " << WSAGetLastError() << std::endl;
    } else {
        std::wcout << L"Listening for connections." << std::endl;
    }

    log_file << get_formatted_time();
    log_file << L": Starting scheduler." << std::endl;
    //log_file.close();

    // Detatching a thread is generally bad, but at worst it'll fail when trying to get the processed files from the index
    std::thread scheduler_thread = std::thread(scheduler_function, std::ref(db_path), std::ref(index), std::ref(log_path));
    scheduler_thread.detach();

    // Accepting all incoming connections
    int addr_client_size = sizeof(addr_client);
    size_t last_id = 0;
    while(socket_client = accept(socket_server, (sockaddr*) &addr_client, &addr_client_size)) {
        char ip_c[24];
        inet_ntop(AF_INET, &addr_client.sin_addr, ip_c, sizeof(ip_c)-1);
        std::wstring ip(ip_c, ip_c + strlen(ip_c));
        {
            std::lock_guard<std::mutex> lock(log_mtx);
            //log_file.open(log_path, std::ios::app);
            log_file << get_formatted_time();
            log_file << L": Client " << last_id << " connected. IP: " << ip << std::endl;
            //log_file.close();
        }
        pool.add_task(process_client, last_id, socket_client, std::ref(index), std::ref(log_file), db_path);
        last_id++;
    }
    closesocket(socket_server);
    WSACleanup();
    return 0;
}