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

#pragma comment(lib,"WS2_32")

namespace fs = std::filesystem;

#define buffer_size 4096


std::wstring clean_string(std::wstring input) {
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

int main()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);


    std::wcout << L"Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;
    std::wcout << "Enter anything to start client: ";
    std::wstring ws;
    std::wcin >> ws;
    std::wcin.clear();
    std::wcout.clear();
    std::wcout << L"=== STARTING CLIENT ===" << std::endl;

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
    SOCKET client_socket;
    client_socket = INVALID_SOCKET;
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Check for socket creation success
    if (client_socket == INVALID_SOCKET) {
        std::wcout << L"Socket error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    } else {
        std::wcout << L"Socket OK" << std::endl;
    }

    // Connect to the server
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr.s_addr);
    server_addr.sin_port = htons(6969);  // Use the same port as the server

    if (connect(client_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        std::wcout << L"Connect error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    } else {
        std::wcout << L"Connect OK" << std::endl;
    }
    // Send query type (Q - query, A - admin)
    std::wcout << std::endl << L"Select query type:" << std::endl;
    std::wcout << " 1 - Word query. Search for one or more words in the database." << std::endl;
    std::wcout << " 2 - Add file(s). Enter a folder whose files will be sent over to be added to the index." << std::endl;
    int choice = 0;
    while(choice != 1 && choice != 2){
        std::wcin >> choice;

        if (std::wcin.fail()) {
            std::wcin.clear();
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
            std::wcout << L"Invalid input, try again." << std::endl;
            continue;
        }

        if(choice != 1 && choice != 2){
            std::wcout << "Invalid choice, try again." << std::endl;
        } else {
            break;
        }
    }

    // Branch for query type 
    if(choice == 1){
        std::wcout << L"Enter your word query (space separated words, all punctuation will be ignored):" << std::endl;
        std::wstring query;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
        getline(std::wcin, query);
        query = clean_string(query);
        //std::wcout << L"Query to be sent:" << std::endl;
        //std::wcout << query << std::endl;

        // Allocate a buffer and perform the conversion
        std::string query_string(buffer_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, &query_string[0], buffer_size, nullptr, nullptr);

        if (send(client_socket, query_string.c_str(), buffer_size, 0) < 0) {
            std::wcout << L"Error sending message" << std::endl;
            closesocket(client_socket);
            return -1;
        }

        int ping = 2;
        std::wcout << "Pinging server for status every " << ping << " seconds." << std::endl;
        char ping_buf[2] = {'S', '\0'};
        char status_buf[2] = {0};
        while(true){
            if (send(client_socket, ping_buf, sizeof(ping_buf), 0) < 0) {
                std::wcout << L"Error sending message" << std::endl;
                closesocket(client_socket);
                return -1;
            }
            if (recv(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
                std::wcout << L"Error receiving message" << std::endl;
                closesocket(client_socket);
                return -1;
            }
            char status = status_buf[0];
            if (status == 'Q'){
                std::wcout << "Task is waiting in queue." << std::endl;
            } else if (status == 'P'){
                std::wcout << "Task is being processed." << std::endl;
            } else if (status == 'E'){
                std::wcout << "No such task exists." << std::endl;
                std::wcout << "How did you get here?" << std::endl;
            } else {
                std::wcout << "Task is complete!" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(ping));
        }
        //std::wcout << "Receiving amount of files found." << std::endl;
        
        unsigned char n_files_buf[5] = {0};
        if (recv(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
            std::wcout << L"Error receiving message" << std::endl;
            closesocket(client_socket);
            return -1;
        }
        int n_files = (n_files_buf[0] << 24) | (n_files_buf[1] << 16) | (n_files_buf[2] << 8) | (n_files_buf[3]);
        if (n_files == 0){
            std::wcout << "No files found, exiting." << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return 0;
        }
        std::wcout << n_files << L" files found." << std::endl;
        std::vector<std::wstring> found_files;
        char buffer[buffer_size] = {0};
        for (int i = 0; i < n_files; i++){
            if (recv(client_socket, buffer, buffer_size, 0) < 0) {
                std::wcout << L"Error receiving message" << std::endl;
                closesocket(client_socket);
                return -1;
            }
            int wide_string_size = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
            std::wstring filepath(wide_string_size, 0);
            MultiByteToWideChar(CP_UTF8, 0, buffer, -1, &filepath[0], wide_string_size);
            filepath.pop_back();
            found_files.push_back(filepath);
            std::wcout << " " << i+1 << ": " << filepath << std::endl;
        }
        //save loop
        while(1){
            std::wcout << "Select a number from 1-" << n_files << " to preview the file. Type 0 to exit." << std::endl;
            int choice = 0;
            std::wcin.clear();
            std::wcin >> choice;
            while (choice < 0 || choice > n_files){
                std::wcout << "Invalid choice." << std::endl;
                std::wcin.clear();
                std::wcin >> choice;
            }
            unsigned char choice_buf[5] = {0};
            choice_buf[0] = (choice >> 24) & 0xFF;
            choice_buf[1] = (choice >> 16) & 0xFF;
            choice_buf[2] = (choice >> 8) & 0xFF;
            choice_buf[3] = choice & 0xFF; 
            if (send(client_socket, (char*)choice_buf, sizeof(choice_buf), 0) < 0) {
                std::wcout << L"Error sending message" << std::endl;
                return 0;
            }
            if (choice == 0){
                std::wcout << "Exiting." << std::endl;
                break;
            }
            unsigned char filesize_buf[3] = {0};
            if (recv(client_socket, (char*)filesize_buf, sizeof(filesize_buf), 0) < 0) {
                std::wcout << L"Error receiving message" << std::endl;
                closesocket(client_socket);
                return -1;
            }
            unsigned short n_file_content_blocks = ((filesize_buf[0] << 8) | (filesize_buf[1]));
            //std::wcout << n_file_content_blocks << std::endl;
            std::wstring file_contents;
            for (int i = 0; i < n_file_content_blocks; i++){
                memset(buffer, '\0', sizeof(buffer));
                if (recv(client_socket, buffer, buffer_size, 0) < 0) {
                    std::wcout << L"Error receiving message" << std::endl;
                    closesocket(client_socket);
                    return -1;
                }
                int file_contents_size = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
                std::wstring temp(file_contents_size, 0);
                MultiByteToWideChar(CP_UTF8, 0, buffer, -1, &temp[0], file_contents_size);
                temp.pop_back();
                file_contents += temp;
            }
            std::wcout << "=== BEGIN FILE ===" << std::endl;
            std::wcout << file_contents << std::endl;
            std::wcout << "=== END FILE ===" << std::endl;
            std::wcout << "Do you want to save the file? y/n" << std::endl;
            std::wstring save;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
            getline(std::wcin, save);
            if(save[0] == 'y'){
                fs::path folder_path = fs::current_path() / "downloads";
                //std::wcout << folder_path << std::endl;
                if(!fs::exists(folder_path)){
                    if (fs::create_directory(folder_path)) {
                        std::wcout << L"Downloads folder successfully created." << std::endl;
                    } else {
                        std::wcerr << L"Failed to create folder downloads folder." << std::endl;
                    }
                }

                std::wstring file_name = found_files[choice-1];
                fs::path file_path = folder_path / fs::path(file_name).filename();
                std::wofstream out_file;
                out_file.open(file_path, std::ios::out);
                if (out_file.is_open()) {
                    out_file << file_contents;
                    out_file.close();
                    std::wcout << L"File written successfully to: " << std::endl << file_path << std::endl;
                } else {
                    std::wcerr << L"Failed to open file: " << std::endl << file_path << std::endl;
                }
            }
        }
        

    } else {

    }

    

    // A
    //

    /*
    // Sending data to the server
    while(1){
        char buffer[1024];
        std::wcout << L"Enter the message: ";
        std::cin.getline(buffer, 1024);
        int sbyte_count = send(client_socket, buffer, 1024, 0);
        if (sbyte_count == SOCKET_ERROR) {
            std::wcout << L"Client send error: " << WSAGetLastError() << std::endl;
            return -1;
        } else {
            std::wcout << L"Client: Sent " << sbyte_count << " bytes" << std::endl;
        }
    }*/

    closesocket(client_socket);
    WSACleanup();
    return 0;
}