#include <iostream>
#include <string>
#include <io.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <cwctype>


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
        }
        else {
            cleaned += L' ';
        }
    }
    return cleaned;
}


//wide string testing shit
int main()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    std::wstring path = L"C:\\Users\\MarcyMuffins\\Desktop\\UNI\\SEM_7\\PARPROC\\repo\\code\\test_utf\\6750_10.txt";
    std::wcout << path << std::endl << std::endl;
    std::wifstream file(path);
    if (!file.is_open()) {
        std::wcerr << L"Unable to open the file!" << std::endl;
        return 0;
    }

    std::wstringstream buffer;
    buffer << file.rdbuf(); // Читаємо файл у рядок
    file.close();
    std::wstring fileContent = buffer.str();
    std::wcout << "File contents:\n" << fileContent << "\n" << std::endl;

    /*
    std::wstring ws2 = clean_string(ws1);
    std::wcout << ws2 << std::endl;
    std::wistringstream stream(ws2);
    std::wstring word;
    while (stream >> word) {
        std::wcout << word << std::endl;
    }*/
    return 0;
}