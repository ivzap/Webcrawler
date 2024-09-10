// csce-463-project1.cpp : This file contains the 'main' function. Program execution begins and ends there.
// 
// Name: Ivan Zaplatar
// Class: 464
// Semester: Fall 2024
//

#include "pch.h"
#include "Socket.h"
#include "UrlParser.h"
#include "Url.h"
#include <iostream>
#include <utility>
#include <fstream>
#include <filesystem>
#include <unordered_set>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <number of threads> <urls filename>" << std::endl;
        return 0;
    }

    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        printf("WSAStartup error %d\n", WSAGetLastError());
        exit(-1);
    }

    std::fstream urlsFile(argv[2], std::ios::in);

    if (!urlsFile.is_open()) {
        std::cout << "File doesn't exist" << std::endl;
        return 1;
    }
    else {
        try {
            std::filesystem::path file{ argv[2] };
            std::cout << "Opened " << argv[2] << " with size " << std::filesystem::file_size(file) << " bytes" << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cout << "OS API error happened when determining the size of the file, " << argv[2] << std::endl;
            return 1;
        }
    }

    std::vector<std::string> rawUrls;

    std::string line;
    while (std::getline(urlsFile, line)) {
        rawUrls.push_back(line);
    }

    Crawler c(atoi(argv[1]));

    for (const std::string& s : rawUrls) {
        c.insertJob(s);
    }

    c.run();
    

    WSACleanup();

    return 0;

}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
