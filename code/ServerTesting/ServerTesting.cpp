#define NOMINMAX

#include <iostream>
#include <string>
#include <io.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <cwctype>
#include <locale>
#include <vector>
#include <thread>
#include <chrono>
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>

using namespace std::chrono;

#pragma comment(lib,"WS2_32")

namespace fs = std::filesystem;

#define buffer_size 4096

struct time_struct{
    int done = 0;
    std::wstring query;
    std::chrono::steady_clock::time_point connection_time;
    std::chrono::steady_clock::time_point queue_wait_end_time;
    std::chrono::steady_clock::time_point start_search_time;
    std::chrono::steady_clock::time_point end_search_time;
    int files_found = 0;
    std::chrono::steady_clock::time_point get_filenames_time;
    std::chrono::steady_clock::time_point download_file_time;
};

void client_function(int id, std::vector<time_struct>& st){// Creating a TCP socket
    char buffer[buffer_size] = {0};
    SOCKET client_socket;
    client_socket = INVALID_SOCKET;
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Check for socket creation success
    if (client_socket == INVALID_SOCKET) {
        std::wcout << L"Socket error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    } else {
        //std::wcout << L"Socket OK" << std::endl;
    }

    // Connect to the server
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr.s_addr);
    server_addr.sin_port = htons(6969);  // Use the same port as the server

    if (connect(client_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        std::wcout << L"Connect error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    } else {
        //std::wcout << L"Connect OK" << std::endl;
    }
    st[id].connection_time = high_resolution_clock::now();
    char start_buf[2] = {0};
    if (recv(client_socket, start_buf, sizeof(start_buf), 0) < 0) {
        std::wcout << L"Error receiving message" << std::endl;
        closesocket(client_socket);
        return;
    }
    st[id].queue_wait_end_time = high_resolution_clock::now();
    char type_buf[2] = { 'C', '\0' };
    if (send(client_socket, type_buf, sizeof(type_buf), 0) < 0) {
        std::wcout << L"Error sending message" << std::endl;
        closesocket(client_socket);
        return;
    }
    std::wstring query = st[id].query;
    std::string query_string(buffer_size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, &query_string[0], buffer_size, nullptr, nullptr);

    if (send(client_socket, query_string.c_str(), buffer_size, 0) < 0) {
        std::wcout << L"Error sending message" << std::endl;
        closesocket(client_socket);
        return;
    }
    char status_buf[2] = {0};
    if (recv(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
        std::wcout << L"Error receiving message" << std::endl;
        closesocket(client_socket);
        return;
    }
    st[id].start_search_time = high_resolution_clock::now();
    if (recv(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
        std::wcout << L"Error receiving message" << std::endl;
        closesocket(client_socket);
        return;
    }
    st[id].end_search_time = high_resolution_clock::now();
    unsigned char n_files_buf[5] = {0};
    if (recv(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
        std::wcout << L"Error receiving message" << std::endl;
        closesocket(client_socket);
        return;
    }
    int n_files = (n_files_buf[0] << 24) | (n_files_buf[1] << 16) | (n_files_buf[2] << 8) | (n_files_buf[3]);
    if(n_files == 0){
        st[id].done = 1;
        closesocket(client_socket);
        return;
    }
    st[id].files_found = 1;
    for (int i = 0; i < n_files; i++){
        if (recv(client_socket, buffer, buffer_size, 0) < 0) {
            std::wcout << L"Error receiving message" << std::endl;
            closesocket(client_socket);
            return;
        }
    }
    st[id].get_filenames_time = high_resolution_clock::now();
    int choice = 1;
    unsigned char choice_buf[5] = {0};
    choice_buf[0] = (choice >> 24) & 0xFF;
    choice_buf[1] = (choice >> 16) & 0xFF;
    choice_buf[2] = (choice >> 8) & 0xFF;
    choice_buf[3] = choice & 0xFF; 
    if (send(client_socket, (char*)choice_buf, sizeof(choice_buf), 0) < 0) {
        std::wcout << L"Error sending message" << std::endl;
        return;
    }
    unsigned char filesize_buf[3] = {0};
    if (recv(client_socket, (char*)filesize_buf, sizeof(filesize_buf), 0) < 0) {
        std::wcout << L"Error receiving message" << std::endl;
        closesocket(client_socket);
        return;
    }
    unsigned short n_file_content_blocks = ((filesize_buf[0] << 8) | (filesize_buf[1]));
    //std::wcout << n_file_content_blocks << std::endl;
    std::wstring file_contents;
    for (int i = 0; i < n_file_content_blocks; i++){
        memset(buffer, '\0', sizeof(buffer));
        if (recv(client_socket, buffer, buffer_size, 0) < 0) {
            std::wcout << L"Error receiving message" << std::endl;
            closesocket(client_socket);
            return;
        }
        int file_contents_size = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
        std::wstring temp(file_contents_size, 0);
        MultiByteToWideChar(CP_UTF8, 0, buffer, -1, &temp[0], file_contents_size);
        temp.pop_back();
        file_contents += temp;
    }
    st[id].download_file_time = high_resolution_clock::now();
    st[id].done = 1;
    closesocket(client_socket);
}

int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

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

    std::random_device rd;
    std::mt19937 g(rd());

    std::vector<int> client_count_list = {4, 8, 16, 32, 48, 64};
    int repeat = 10;
    std::wcout << L"Every test will be averaged over " << repeat << " runs." << std::endl;
    for(const auto& client_count : client_count_list){
        //std::wcout << L"Starting server testing with " << client_count << " clients." << std::endl;
        unsigned long long queue_time = 0;
        unsigned long long queue_to_search_time = 0;
        unsigned long long search_time = 0;
        unsigned long long filename_time = 0;
        unsigned long long file_time = 0;
        for(int r = 0; r < repeat; r++){
            std::vector<time_struct> st(client_count);

            // take 1 uncommon word
            // and take 1-3 common words
            // mix them around

            std::vector<std::wstring> uncommon = {L"woman", L"vampyr", L"time", L"cliché", L"plot", L"camera", L"bible", L"kill"};
            std::vector<std::wstring> common = {L"the", L"and", L"have", L"good", L"them", L"but", L"like", L"you", L"he"};

            for(int i = 0; i < client_count; i++){
                std::vector<std::wstring> temp;
                temp.push_back(uncommon[g() % uncommon.size()]);
                int c = 1 + (g() % 3);
                for(int j = 0; j < c; j++){
                    temp.push_back(common[g() % common.size()]);
                }
                std::shuffle(temp.begin(), temp.end(), g);
                std::wstringstream temp_stream;
                for (size_t i = 0; i < temp.size(); ++i) {
                    temp_stream << temp[i];
                    if (i != temp.size() - 1) {
                        temp_stream << L" ";
                    }
                }
                st[i].query = temp_stream.str();
            }

            std::thread thread;
            for(int i = 0; i < client_count; i++){
                thread = std::thread(client_function, i, std::ref(st));
                thread.detach();
            }
            int count = 0;
            while(count < client_count){
                std::this_thread::sleep_for(std::chrono::seconds(5));
                count = 0;
                for(const auto& i : st){
                    count += i.done;
                }
            }
            unsigned long long duration;
            duration = 0;
            for(int i = 0; i < client_count; i++){
                duration += duration_cast<microseconds>(st[i].queue_wait_end_time - st[i].connection_time).count();
            }
            duration = duration / client_count;
            queue_time += duration;

            duration = 0;
            for(int i = 0; i < client_count; i++){
                duration += duration_cast<microseconds>(st[i].start_search_time - st[i].queue_wait_end_time).count();
            }
            duration = duration / client_count;
            queue_to_search_time += duration;

            duration = 0;
            for(int i = 0; i < client_count; i++){
                duration += duration_cast<microseconds>(st[i].end_search_time - st[i].start_search_time).count();
            }
            duration = duration / client_count;
            search_time += duration;

            duration = 0;
            int found_files_count = 0;
            for(int i = 0; i < client_count; i++){
                if(st[i].files_found == 1){
                    duration += duration_cast<microseconds>(st[i].get_filenames_time - st[i].end_search_time).count();
                    found_files_count++;
                }
            }
            duration = duration / found_files_count;
            filename_time += duration;

            duration = 0;
            for(int i = 0; i < client_count; i++){
                if(st[i].files_found == 1){
                    duration += duration_cast<microseconds>(st[i].download_file_time - st[i].get_filenames_time).count();
                }
            }
            duration = duration / found_files_count;
            file_time += duration;
        }
        queue_time = queue_time / 4;
        queue_to_search_time = queue_to_search_time / 4;
        search_time = search_time / 4;
        filename_time = filename_time / 4;
        file_time = file_time / 4;
        //std::wcout << "All clients stopped." << std::endl;
        //std::wcout << "=== " << client_count << " CLIENTS ===" << std::endl;
        std::wcout << (double)(queue_time * 0.001) << ' ';
        std::wcout << (double)(queue_to_search_time * 0.001) << ' ';
        std::wcout << (double)(search_time * 0.001) << ' ';
        std::wcout << (double)(filename_time * 0.001) << ' ';
        std::wcout << (double)(file_time * 0.001) << std::endl;// << std::endl;
        /*
        std::wcout << "Average queue wait time: " << (double)(queue_time * 0.001) << " milliseconds" << std::endl;
        std::wcout << "Average queue to search time: " << (double)(queue_to_search_time * 0.001) << " milliseconds" << std::endl;
        std::wcout << "Average search time: " << (double)(search_time * 0.001) << " milliseconds" << std::endl;
        std::wcout << "Average filename download time: " << (double)(filename_time * 0.001) << " milliseconds" << std::endl;
        std::wcout << "Average file download time: " << (double)(file_time * 0.001) << " milliseconds" << std::endl << std::endl;
        */
    }
    WSACleanup();
}
/*


*/