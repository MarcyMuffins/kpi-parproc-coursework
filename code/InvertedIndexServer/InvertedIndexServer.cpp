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
#include "client_data_t.h"

#pragma comment(lib, "ws2_32.lib")

#define buffer_size 4096

namespace fs = std::filesystem;

constexpr bool DEBUG = true;
std::mutex print_mtx;

std::vector<std::wstring> client_function(size_t id, std::vector<std::wstring>& query, inverted_index& index){
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << "TDP " << id << ": Pool began processing task." << std::endl;
    }
    std::vector<std::wstring> result = index.search(query);
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << "TDP " << id << ": Pool finished processing task." << std::endl;
    }
    return result;
}

inline void handle_disconnect(size_t id, client_data_t& client_data, SOCKET& client_socket){
    time_t disconnect_time;
    time(&disconnect_time);
    client_data.disconnect_time = disconnect_time;
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"CLT " << id << L" disconnected. " << std::endl;
        std::wcout << L"    TIME: " << disconnect_time << std::endl;
    }
    //TODO before closing print log to file
    closesocket(client_socket);
}

void process_client(size_t id, SOCKET client_socket, sockaddr_in client_addr, inverted_index& index, std::map<size_t, client_data_t>& client_data, thread_pool& pool){
    time_t connect_time;
    time(&connect_time);

    int timeout = 60 * 1000; // 60 seconds
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    char ip_c[24];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_c, sizeof(ip_c)-1);
    std::wstring ip(ip_c, ip_c + strlen(ip_c));

    std::vector<std::wstring> query;
    query.clear();

    std::vector<std::wstring> result;
    result.clear();

    time_t disconnect_time = 0;
    client_data[id] = {
        id,
        ip,
        query,
        result,
        connect_time,
        disconnect_time
    };
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"CLT " << id << L" connected. " << std::endl;
        std::wcout << L"    IP: " << ip << std::endl;
        std::wcout << L"    TIME: " << connect_time << std::endl << std::endl;
    }

    unsigned char n_blocks_buff[2];
    char buffer[buffer_size];
    memset(buffer, '\0', sizeof(buffer));
    int bytes_received = recv(client_socket, buffer, buffer_size, 0);
    if (bytes_received == 0) {
        if(DEBUG){
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" disconnected. " << std::endl;
        }
        handle_disconnect(id, client_data[id], client_socket);
        return;
    } else if (bytes_received < 0) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            if (DEBUG) {
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" timeout. " << std::endl;
            }
        } else {
            if (DEBUG) {
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" Error. " << std::endl;
            }
        }
        handle_disconnect(id, client_data[id], client_socket);
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
    client_data[id].query = query;
    if (DEBUG) {
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"CLT " << id << L" query:" << std::endl;
        for(const auto& i : query){
            std::wcout << i << " ";
        }
        std::wcout << std::endl << std::endl;
    }
    size_t id_pool = pool.add_task(client_function, id, std::ref(query), std::ref(index));

    char ping_buf[2] = {0};
    char status_buf[2] = {0};
    while (true) {
        int bytes_received = recv(client_socket, (char*)ping_buf, sizeof(ping_buf), 0);
        if (bytes_received > 0 && ping_buf[0] == 'S') {
            result = pool.get_status(id_pool);
            if(result.empty()){
                if(DEBUG){
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" no files found. " << std::endl;
                }
                status_buf[0] = 'C';
                if (send(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
                    std::wcout << L"CLT " << id << L" error sending message" << std::endl;
                    handle_disconnect(id, client_data[id], client_socket);
                    return;
                }
                break;

            } else if (result.at(0).length() == 1){
                if(result.at(0) == L"N"){
                    status_buf[0] = 'N';
                } else if(result.at(0) == L"Q"){
                    status_buf[0] = 'Q';
                } else if(result.at(0) == L"P"){
                    status_buf[0] = 'P';
                }
                if (send(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
                    std::wcout << L"CLT " << id << L" error sending message" << std::endl;
                    handle_disconnect(id, client_data[id], client_socket);
                    return;
                }
            } else {
                status_buf[0] = 'C';
                if (send(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
                    std::wcout << L"CLT " << id << L" error sending message" << std::endl;
                    handle_disconnect(id, client_data[id], client_socket);
                    return;
                }
                break;
            }
        } else if (bytes_received == 0) {
            if(DEBUG){
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" disconnected. " << std::endl;
            }
            handle_disconnect(id, client_data[id], client_socket);
            return;
        } else {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                if (DEBUG) {
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" timeout. " << std::endl;
                }
            } else {
                if (DEBUG) {
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" Error. " << std::endl;
                }
            }
            handle_disconnect(id, client_data[id], client_socket);
            return;
        }
    }
    unsigned int n_files = result.size();
    unsigned char n_files_buf[5] = {0};
    n_files_buf[0] = (n_files >> 24) & 0xFF;
    n_files_buf[1] = (n_files >> 16) & 0xFF;
    n_files_buf[2] = (n_files >> 8) & 0xFF;
    n_files_buf[3] = n_files & 0xFF; 
    if (send(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
        std::wcout << L"CLT " << id << L" error sending message" << std::endl;
        handle_disconnect(id, client_data[id], client_socket);
        return;
    }
    if(n_files == 0){
        handle_disconnect(id, client_data[id], client_socket);
        return;
    }
    if(DEBUG){
        std::lock_guard<std::mutex> lock(print_mtx);
        std::wcout << L"CLT " << id << L" found query in " << n_files << " files:" << std::endl;
        for(const auto& i : result){
            std::wcout << " " << i << std::endl;
        }
        std::wcout << std::endl;
    }
    for (int i = 0; i < n_files; i++){
        std::string filepath(buffer_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, result[i].c_str(), -1, &filepath[0], buffer_size, nullptr, nullptr);
        if (send(client_socket, filepath.c_str(), buffer_size, 0) < 0) {
            std::wcout << L"CLT " << id << L" error sending message" << std::endl;
            handle_disconnect(id, client_data[id], client_socket);
            return;
        }
    }
    while(1){
        unsigned char choice_buf[5] = {0};
        bytes_received = recv(client_socket, (char*)choice_buf, sizeof(choice_buf), 0);
        if (bytes_received == 0) {
            if(DEBUG){
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" disconnected. " << std::endl;
            }
            handle_disconnect(id, client_data[id], client_socket);
            return;
        } else if (bytes_received < 0) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                if (DEBUG) {
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" timeout. " << std::endl;
                }
            } else {
                if (DEBUG) {
                    std::lock_guard<std::mutex> lock(print_mtx);
                    std::wcout << L"CLT " << id << L" Error. " << std::endl;
                }
            }
            handle_disconnect(id, client_data[id], client_socket);
            return;
        }
        int choice = (choice_buf[0] << 24) | (choice_buf[1] << 16) | (choice_buf[2] << 8) | (choice_buf[3]);
        if (choice == 0){
            if(DEBUG){
                std::lock_guard<std::mutex> lock(print_mtx);
                std::wcout << L"CLT " << id << L" requested no file. Disconnecting." << std::endl;
            }
            handle_disconnect(id, client_data[id], client_socket);
            return;
        }
        if (DEBUG) {
            std::lock_guard<std::mutex> lock(print_mtx);
            std::wcout << L"CLT " << id << L" requested file " << choice << std::endl;
        }
        choice--;
        std::wifstream file(result[choice]);
        if (!file.is_open()) {
            std::wcerr << L"CLT " << id  << L" unable to open the file." << std::endl;
            handle_disconnect(id, client_data[id], client_socket);
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
            std::wcout << L"CLT " << id << L" error sending message" << std::endl;
            handle_disconnect(id, client_data[id], client_socket);
            return;
        }
        //std::wcout << n_file_content_blocks << std::endl;
        for (int i = 0; i < n_file_content_blocks; i++){
            memset(buffer, '\0', sizeof(buffer));
            int index = i * (buffer_size-1);
            int copy_len = buffer_size-1;
            if(index + copy_len > file_content_s.length()){
                copy_len = file_content_s.length() - index;
            }
            memcpy(buffer, &file_content_s[index], copy_len);
            if (send(client_socket, buffer, buffer_size, 0) < 0) {
                std::wcout << L"CLT " << id << L" error sending message" << std::endl;
                handle_disconnect(id, client_data[id], client_socket);
                return;
            }
        }
    }
    handle_disconnect(id, client_data[id], client_socket);
}

int main()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    std::wcout << L"Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;

    std::wcout << std::endl << L"=== BULDING INDEX ===" << std::endl << std::endl;
    //std::wcout << L"Enter full path for the folder you want scanned:" << std::endl;
    std::wstring p = L"C:\\Users\\MarcyMuffins\\Desktop\\UNI\\SEM_7\\PARPROC\\repo\\code\\db";
    //std::wcin >> p;
    fs::path curr(p);
    std::vector<std::wstring> files;
    std::wcout << std::endl;

    std::wcout << L"Scanning folder and subfolders for files." << std::endl;
    for (const auto& entry : fs::recursive_directory_iterator(curr, fs::directory_options::follow_directory_symlink)) {
        if (fs::is_regular_file(entry)) {
            //std::cout << entry.path() << " " << entry.file_size() << std::endl;
            files.push_back(entry.path().generic_wstring());
        }
    }
    std::wcout << L"Finished scanning, found " << files.size() << L" files." << std::endl;

    std::wcout << L"Building the file index." << std::endl;
    inverted_index index(files);
    std::wcout << L"Done." << std::endl;

    if(DEBUG && false){
        std::wcout << L"Do you want to print out the index? (It will be really long) y/n" << std::endl;
        std::wcin >> p;
        if (p == L"y") {
            std::wcout << L":3" << std::endl;
            index.debug_list_files();
        }
    }
    std::wcout << std::endl << L"=== STARTING SERVER ===" << std::endl;

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
    //TCP/IP protocol, setting the port and not binding socket to any specific IP
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

    // Listen for incoming connections
    if (listen(socket_server, 256) == SOCKET_ERROR) {
        std::wcout << L"listen() error " << WSAGetLastError() << std::endl;
    } else {
        std::wcout << L"Listening for connections." << std::endl;
    }

    std::map<size_t, client_data_t> client_data;
    thread_pool pool;
    std::thread client_thread;
    pool.initialize(4, true);
    int addr_client_size = sizeof(addr_client);
    size_t last_id = 0;
    while(socket_client = accept(socket_server, (sockaddr*) &addr_client, &addr_client_size)) {
        client_thread = std::thread(process_client, last_id, socket_client, addr_client, std::ref(index), std::ref(client_data), std::ref(pool));
        client_thread.detach();
        last_id++;
    }
    closesocket(socket_server);
    WSACleanup();
    return 0;


}