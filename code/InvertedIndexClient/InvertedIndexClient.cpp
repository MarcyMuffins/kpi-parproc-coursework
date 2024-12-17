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

    // Removing the <br /> tag
    size_t pos;
    std::wstring sub = L"<br />";
    while ((pos = input.find(sub)) != std::wstring::npos) {
        input.erase(pos, sub.length());
        input.insert(pos, L" ");
    }
    // Sanitizing the string. everything not alphanumeric or space will be turned into a space, everything else is made lowercase
    for (int i = 0; i < input.length(); i++) {
        wchar_t ch = input.at(i);
        if (std::iswalnum(ch) || std::iswspace(ch)) {
            cleaned += std::towlower(ch);
        } else {
            cleaned += L' ';
        }
    }
    return cleaned;
}

int main()
{
    // Making sure the input and output streams accept UTF-16
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);


    //std::wcout << L"Note: This is purely debug code, it will not do any checks on whether or not your inputs are appropriate." << std::endl;
    //std::wcout << "Enter anything to start client: ";
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

    std::wcout << "Waiting for server signal to start sending data." << std::endl;
    char start_buf[2] = {0};
    if (recv(client_socket, start_buf, sizeof(start_buf), 0) < 0) {
        std::wcout << L"Error receiving message" << std::endl;
        closesocket(client_socket);
        return -1;
    }
    std::wcout << "Server started processing the client." << std::endl;

    char buffer[buffer_size] = {0};
    // Send query type (C - client, A - admin)
    std::wcout << std::endl << L"Select query type:" << std::endl;
    std::wcout << " 1 - Word query. Search for one or more words in the database." << std::endl;
    std::wcout << " 2 - Add file(s). Enter a folder whose files will be sent over to be added to the index." << std::endl;
    int choice = 0;
    while(choice != 1 && choice != 2){
        std::wcin >> choice;
        // Handling error inputs
        if (std::wcin.fail()) {
            // Clearing the wcin buffer
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

    char type_buf[2] = {0};
    //branch for query type 
    if(choice == 1){
        type_buf[0] = 'C';
        if (send(client_socket, type_buf, sizeof(type_buf), 0) < 0) {
            std::wcout << L"Error sending message" << std::endl;
            closesocket(client_socket);
            return -1;
        }
        std::wcout << L"Enter your word query (space separated words, all punctuation will be ignored):" << std::endl;
        std::wstring query;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
        getline(std::wcin, query);
        query = clean_string(query);

        // Converting a wstring to a regular string, Windows specific
        std::string query_string(buffer_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, &query_string[0], buffer_size, nullptr, nullptr);

        if (send(client_socket, query_string.c_str(), buffer_size, 0) < 0) {
            std::wcout << L"Error sending message" << std::endl;
            closesocket(client_socket);
            return -1;
        }

        char status_buf[2] = {0};
        if (recv(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
            std::wcout << L"Error receiving message" << std::endl;
            closesocket(client_socket);
            return -1;
        }
        if (status_buf[0] == 'Q'){
            std::wcout << "Server started searching for the query in the index." << std::endl;
        } else {
            std::wcerr << "Something went wrong." << std::endl;
            closesocket(client_socket);
            return -1;
        }

        if (recv(client_socket, status_buf, sizeof(status_buf), 0) < 0) {
            std::wcout << L"Error receiving message" << std::endl;
            closesocket(client_socket);
            return -1;
        }
        if (status_buf[0] == 'F'){
            std::wcout << "Server finished searching for the query in the index." << std::endl;
        } else {
            std::wcerr << "Something went wrong." << std::endl;
            closesocket(client_socket);
            return -1;
        }
        
        unsigned char n_files_buf[5] = {0};
        if (recv(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
            std::wcout << L"Error receiving message" << std::endl;
            closesocket(client_socket);
            return -1;
        }
        // Extracting the 4 byte int from the buffer, little endian
        int n_files = (n_files_buf[0] << 24) | (n_files_buf[1] << 16) | (n_files_buf[2] << 8) | (n_files_buf[3]);
        if (n_files == 0){
            std::wcout << "No files found, exiting." << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return 0;
        }
        std::wcout << n_files << L" files found." << std::endl;
        std::vector<std::wstring> found_files;
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
        // File view/save loop
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
            // Encoding the 4 byte int into the buffer, little endian
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
        type_buf[0] = 'A';
        if (send(client_socket, type_buf, sizeof(type_buf), 0) < 0) {
            std::wcout << L"Error sending message" << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return -1;
        }
        std::wcout << L"Enter full path for the folder you want scanned:" << std::endl;
        std::wstring p;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
        std::wcin >> p;
        fs::path scan_folder(p);
        std::vector<std::wstring> files;
        std::wcout << std::endl;

        std::wcout << L"Scanning folder and subfolders for files." << std::endl;
        for (const auto& entry : fs::recursive_directory_iterator(scan_folder, fs::directory_options::follow_directory_symlink)) {
            if (fs::is_regular_file(entry)) {
                files.push_back(entry.path().generic_wstring());
            }
        }
        std::wcout << L"Finished scanning, found " << files.size() << L" files." << std::endl;

        unsigned int n_files = files.size();
        unsigned char n_files_buf[5] = {0};
        if (files.size() == 0){
            std::wcout << L"Found no files, exiting." << std::endl;
            if (send(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
                std::wcout << L"Error sending message" << std::endl;
                closesocket(client_socket);
                WSACleanup();
                return -1;
            }
            closesocket(client_socket);
            WSACleanup();
            return 0;
        }

        std::wcout << L"Do you want to list the files? y/n" << std::endl;
        std::wstring list;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
        getline(std::wcin, list);
        if(list[0] == 'y'){
            for(const auto& i : files){
                std::wcout << i << std::endl;
            }
        }
        n_files_buf[0] = (n_files >> 24) & 0xFF;
        n_files_buf[1] = (n_files >> 16) & 0xFF;
        n_files_buf[2] = (n_files >> 8) & 0xFF;
        n_files_buf[3] = n_files & 0xFF; 
        if (send(client_socket, (char*)n_files_buf, sizeof(n_files_buf), 0) < 0) {
            std::wcout << L"Error sending message" << std::endl;
            closesocket(client_socket);
            WSACleanup();
            return -1;
        }
        for(int i = 0; i < n_files; i++){
            // Sending the relative filepath so the correct folders are created by the server in the database location
            fs::path filepath(files[i]);
            std::wstring relative_filepath = fs::relative(filepath, scan_folder).wstring();

            std::string relative_filepath_c(buffer_size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, relative_filepath.c_str(), -1, &relative_filepath_c[0], buffer_size, nullptr, nullptr);
            //std::wcout << relative_filepath << std::endl;
            if (send(client_socket, relative_filepath_c.c_str(), buffer_size, 0) < 0) {
                std::wcout << L"Error sending message" << std::endl;
                closesocket(client_socket);
                WSACleanup();
                return -1;
            }

            std::wifstream file(files[i]);
            if (!file.is_open()) {
                std::wcout << "Unable to open file" << std::endl;
                closesocket(client_socket);
                WSACleanup();
                return -1;
            }
            // Open the file as en_US.UTF-8, sorry non latin alphabets
            file.imbue(std::locale("en_US.UTF-8"));
            std::wstringstream file_buffer;
            file_buffer << file.rdbuf();
            file.close();
            std::wstring file_content = file_buffer.str();
            // First call of WideCharToMultiByte() to get the length needed 
            int file_content_size = WideCharToMultiByte(CP_UTF8, 0, &file_content[0], -1, nullptr, 0, nullptr, nullptr);
            std::string file_content_s(file_content_size, '\0');
            // Second call to transfer data
            WideCharToMultiByte(CP_UTF8, 0, &file_content[0], -1, &file_content_s[0], file_content_size, nullptr, nullptr);
            unsigned short n_file_content_blocks = (file_content_size / buffer_size) + 1;
            unsigned char filesize_buf[3] = {0};
            filesize_buf[0] = (n_file_content_blocks >> 8) & 0xFF;
            filesize_buf[1] = n_file_content_blocks & 0xFF; 
            
            if (send(client_socket, (char*)filesize_buf, sizeof(filesize_buf), 0) < 0) {
                std::wcout << L"Error sending message" << std::endl;
                closesocket(client_socket);
                WSACleanup();
                return -1;
            }

            for (int i = 0; i < n_file_content_blocks; i++){
                memset(buffer, '\0', sizeof(buffer));
                int index = i * (buffer_size-1);
                int copy_len = buffer_size-1;
                if(index + copy_len > file_content_s.length()){
                    copy_len = file_content_s.length() - index;
                }
                memcpy(buffer, &file_content_s[index], copy_len);
                if (send(client_socket, buffer, buffer_size, 0) < 0) {
                    std::wcout << L"Error sending message" << std::endl;
                    closesocket(client_socket);
                    WSACleanup();
                    return -1;
                }
            }
        }
    }
    closesocket(client_socket);
    WSACleanup();
    return 0;
}