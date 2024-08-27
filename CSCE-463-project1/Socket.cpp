// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#include "pch.h"
#include "Socket.h"
#include "Url.h"

#include <iostream>
//#include <winsock.h>



Socket::~Socket() {
	delete[] buf;
	Shutdown();
}

Socket::Socket(int timeout)
{
	sock = INVALID_SOCKET;
	buf = new char[INITIAL_BUF_SIZE];
	allocatedSize = INITIAL_BUF_SIZE;
	this->timeout = timeout;
	curPos = 0;
}

bool Socket::Connect(const Url& url) {
	if (sock != INVALID_SOCKET) // must shutdown old socket socket
		return false;
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		printf("socket() error %d\n", WSAGetLastError());
		return false;
	}

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	server.sin_family = AF_INET; // IPv4

	clock_t dnsStart = clock();

	unsigned long ipv4 = inet_addr(url.host.c_str());

	if (ipv4 != INADDR_NONE) {
		server.sin_addr.s_addr = ipv4;
	}
	else {
		struct hostent* host;
		if ((host = (hostent*)gethostbyname(url.host.c_str())) == nullptr) {
			std::cout << "\t  Doing DNS... failed with " << WSAGetLastError() << std::endl;
			return false;
		}
		server.sin_addr = *((struct in_addr*)host->h_addr_list[0]);
	}
	clock_t dnsEnd = clock();

	double dnsElapsed = (double)(dnsEnd - dnsStart) / CLOCKS_PER_SEC;

	std::cout << "\t  Doing DNS... " << "done in " << dnsElapsed * 1000 << " ms, " << "found " << inet_ntoa(server.sin_addr) << std::endl;

	server.sin_port = htons(url.port);

	clock_t connStart = clock();

	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		std::cout << "\t* Connecting on page... failed with " << WSAGetLastError() << std::endl;
		return false;
	}

	clock_t connEnd = clock();

	double connElapsed = (double)(connEnd - connStart) / CLOCKS_PER_SEC;


	// send get request
	std::string sendBuf = "GET " + url.path + url.query + " HTTP/1.0\r\nUser-Agent: myTamuCrawler/1.0\r\nHost: " + url.host + "\r\nConnection: close\r\n\r\n";

	if (send(sock, sendBuf.c_str(), sendBuf.length(), 0) == SOCKET_ERROR)
	{
		std::cout << "\t* Connecting on page... failed with " << WSAGetLastError() << "ms" << std::endl;
		return false;
	}

	std::cout << "\t* Connecting on page... done in " << connElapsed * 1000 << " ms" << std::endl;

	return true;

}

bool Socket::Read(void)
{
	fd_set fds;

	timeval timeout;
	timeout.tv_sec = this->timeout;
	timeout.tv_usec = 0;

	clock_t start = clock();

	curPos = 0; // reset to start of buffer.
	while (true)
	{

		// reset socket set since select modifies inputs

		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int ret;
		if ((ret = select(0, &fds, NULL, NULL, &timeout)) > 0)
		{
			// new data available; now read the next segment
			int bytes = recv(sock, buf + curPos, allocatedSize - curPos, 0);
			if (bytes == SOCKET_ERROR) {
				std::cout << "\t  Loading... failed with " << WSAGetLastError() << " on recv" << std::endl;
				return false;
			}
			if (bytes == 0) {
				buf[curPos] = '\0';
				// verify valid http(HTTP/) response
				if (strncmp(buf, "HTTP/", 5) != 0) {
					std::cout << "\t  Loading... " << "failed with non-HTTP header (does not begin with HTTP/)" << std::endl;
					return false;
				}
				std::cout << "\t  Loading... done in " << (double)(clock() - start) / CLOCKS_PER_SEC * 1000 << " ms with " << curPos << " bytes" << std::endl;
				return true; // normal completion
			}
			curPos += bytes; // adjust where the next recv goes
			if (allocatedSize - curPos < THRESHHOLD) { // always require 1000 extra bytes, performance sensitive here!
				void* newBuf = realloc(buf, allocatedSize + THRESHHOLD);
				if (newBuf == nullptr) {
					std::cout << "\t  Loading... failed on realloc" << std::endl;
					return false; // RAII will free buffer
				}
				buf = (char*)newBuf;
				allocatedSize += THRESHHOLD;
			}
		}
		else if (ret == 0) {
			// report timeout
			std::cout << "\t  Loading... failed with timeout" << std::endl;
			return false;
		}
		else {
			std::cout << "\t  Loading... failed with " << WSAGetLastError() << " on select" << std::endl;
			return false;
		}

	}


	return false;
}

void Socket::Shutdown() {
	if (sock == INVALID_SOCKET) return;

	if (closesocket(sock) == SOCKET_ERROR) {
		if (WSACleanup() == SOCKET_ERROR) {
			exit(1);
		}
	}
}