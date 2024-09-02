// csce-463-project1.cpp : This file contains the 'main' function. Program execution begins and ends there.
// 
// Name: Ivan Zaplatar
// Class: 436
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

std::pair<std::vector<std::string>, std::string> getParsedResponse(const Socket& s) {
    std::vector<std::string> header;
    int i = 0;
    for (; i < s.curPos; i += 2) {
        std::string line;
        while (i < s.curPos && s.buf[i] != '\r') {
            line += s.buf[i++];
        }
        if (line.length() == 0) break;

        header.push_back(line);
    }
    // remove any leading \r\n to the html.
    while (i < s.curPos && (s.buf[i] == '\r' || s.buf[i] == '\n')) i++;

    std::string html;
    for (; i < s.curPos; i++) {
        html += s.buf[i];
    }
    return std::move(std::make_pair(std::move(header), std::move(html)));
}


bool sendSocketRequest(Socket& s, const Url& url, bool robotCheck, int maxRead) {

    if (url.scheme == "") return false;

    bool connected = s.Connect(url, robotCheck);

    if (!connected) return false;

    int validSocketRead = s.Read(maxRead);

    if (!validSocketRead) return false;

    auto response = getParsedResponse(s);

    std::string code;
    if (response.first[0].length() >= 12) {
        code = response.first[0].substr(9, 3);
        std::cout << "\t  Verifying header... status code " << code << std::endl;
    }
    else {
        std::cout << "\t  Verifying header... invalid header" << std::endl;
        return false;
    }

    if (robotCheck) {
        if (code[0] != '4') {
            return false;
        }
        return true;
    }
    
    if (code[0] != '2') {
        return false;
    }

    // create new parser object
    std::shared_ptr<HTMLParserBase> parser(new HTMLParserBase);

    int nLinks;

    clock_t start = clock();

    const char* rawUrlCopy = url.rawUrl.c_str();

    char* linkBuffer = parser->Parse((char*)response.second.c_str(), response.second.length(), (char*)rawUrlCopy, (int)strlen(rawUrlCopy), &nLinks);

    // check for errors indicated by negative values
    if (nLinks < 0)
        nLinks = 0;

    clock_t end = clock();

    std::cout << "\t+ Parsing page... done in " << (double)(end - start) / CLOCKS_PER_SEC * 1000 << " ms with " << nLinks << " links" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    for (auto str : response.first) {
        std::cout << str << std::endl;
    }

    return true;
}


int main(int argc, char* argv[])
{

    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        printf("WSAStartup error %d\n", WSAGetLastError());
        exit(-1);
    }

    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <number of threads> <urls filename>" << std::endl;
        return 0;
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

    UrlParser p;

    std::unordered_set<std::string> uniqueHosts;
    
    {
        Socket s(10);

        for (const std::string& rawUrl : rawUrls) {

            std::cout << "URL: " << rawUrl << std::endl;

            Url url = p.parse(rawUrl);


            if (sendSocketRequest(s, url, true, 16*1024)) {
                // download the page request
                s.Shutdown();
                sendSocketRequest(s, url, false, 40000);
            }
            s.Shutdown();
        }
    }

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
