// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#include "pch.h"
#include "Socket.h"
#include "Url.h"
#include <synchapi.h>
#include <iostream>
//#include <winsock.h>

void Socket::updateSeenIps(const std::string& ip) {
	this->seenIps.insert(ip);
}

void Socket::updateSeenHosts(const std::string& host) {
	this->seenHosts.insert(host);
}

bool Socket::seenHost(const std::string& host) {
	return this->seenHosts.contains(host);
}

bool Socket::seenIp(const std::string& ip) {
	return this->seenHosts.contains(ip);
}

Socket::~Socket() {
	delete[] buf;
	Shutdown();
}

// socket class should always set sock to invalid_sock when done with it, never leave it dangling.
Socket::Socket(int timeout)
{
	sock = INVALID_SOCKET;
	buf = new char[INITIAL_BUF_SIZE];
	allocatedSize = INITIAL_BUF_SIZE;
	this->timeout = timeout;
	curPos = 0;
}

/*
	Why not have a DNS lookup table?
	- user wants to webcrawl
		> make head request
			if allowed to connect via /robots.txt
				make http request

*/

bool Socket::Connect(const Url& url, bool robotCheck) {
	// if we reuse object dont let buffer grow to inf
	if (allocatedSize > 32 * 1024) {
		std::cout << "Buffer exceeded 32KB, resizing to original size..." << std::endl;
		void* newBuf = realloc(buf, INITIAL_BUF_SIZE);
		if (newBuf == nullptr) {
			std::cout << "WARNING: realloc failed in Connect()" << std::endl;
			return false;
		}

		buf = (char*)newBuf;

	}

	if (sock != INVALID_SOCKET) {
		this->Shutdown();
	}

	if (robotCheck && this->seenHost(url.host)) {
		std::cout << "\t  Checking host uniqueness... failed" << std::endl;
		return false;
	}

	this->updateSeenHosts(url.host);

	if(robotCheck)
		std::cout << "\t  Checking host uniqueness... passed" << std::endl;

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
	else{
		struct hostent* host;
		if ((host = (hostent*)gethostbyname(url.host.c_str())) == nullptr) {
			if(robotCheck)
				std::cout << "\t  Doing DNS... failed with " << WSAGetLastError() << std::endl;
			return false;
		}
		server.sin_addr = *((struct in_addr*)host->h_addr_list[0]);
	}

	std::string ip = inet_ntoa(server.sin_addr);

	server.sin_port = htons(url.port);

	clock_t dnsEnd = clock();

	double dnsElapsed = (double)(dnsEnd - dnsStart) / CLOCKS_PER_SEC;

	if(robotCheck)
		std::cout << "\t  Doing DNS... " << "done in " << dnsElapsed * 1000 << " ms, " << "found " << inet_ntoa(server.sin_addr) << std::endl;

	if (robotCheck && this->seenIp(ip)) {
		std::cout << "\t  Checking IP uniqueness... failed" << std::endl;
		return false;
	}
	if (robotCheck)
		std::cout << "\t  Checking IP uniqueness... passed" << std::endl;

	this->updateSeenIps(ip);

	clock_t connStart = clock();

	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		if(robotCheck)
			std::cout << "\t  Connecting on robots... failed with " << WSAGetLastError() << std::endl;
		else
			std::cout << "\t* Connecting on page... failed with " << WSAGetLastError() << std::endl;
		return false;
	}

	clock_t connEnd = clock();

	double connElapsed = (double)(connEnd - connStart) / CLOCKS_PER_SEC;

	// send get request
	std::string urlAndPath = robotCheck ? "/robots.txt" : url.path + url.query;
	std::string reqType = robotCheck ? "HEAD" : "GET";
	std::string sendBuf = reqType + " " + urlAndPath + " " + "HTTP/1.0\r\nUser-Agent: myTamuCrawler/1.0\r\nHost:" + " " + url.host + "\r\nConnection: close\r\n\r\n";
	
	if (send(sock, sendBuf.c_str(), sendBuf.length(), 0) == SOCKET_ERROR)
	{
		if (robotCheck)
			std::cout << "\t  Connecting on robots... failed with " << WSAGetLastError() << std::endl;
		else
			std::cout << "\t* Connecting on page... failed with " << WSAGetLastError() << std::endl;
		return false;
	}

	if (robotCheck)
		std::cout << "\t  Connecting on robots... done in " << connElapsed * 1000 << " ms" << std::endl;
	else
		std::cout << "\t* Connecting on page... done in " << connElapsed * 1000 << " ms" << std::endl;



	return true;

}

bool Socket::Read(int maxRead)
{
	fd_set fds;

	timeval timeout;
	timeout.tv_sec = this->timeout;
	timeout.tv_usec = 0;

	

	curPos = 0; // reset to start of buffer.
	while (true)
	{
		clock_t start = clock();
		// reset socket set since select modifies inputs

		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int ret;
		if ((ret = select(0, &fds, NULL, NULL, &timeout)) > 0)
		{
			// new data available; now read the next segment
			int bytes = recv(sock, buf + curPos, allocatedSize - curPos, 0);
			// have we exceeded our total time limit
			/*double totalTime = double(clock() - start) / CLOCKS_PER_SEC;
			if (totalTime > this->timeout) {
				std::cout << "\t  Loading... failed with slow download" << std::endl;
				return false;
			}*/
			// have we exceeded our max limit?
			if (curPos > maxRead) {
				std::cout << "\t  Loading... failed with exceeding max" << std::endl;
				return false;
			}
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
			// adjust the timeout param by the time elapsed
			double remainingTimeoutSec = timeout.tv_sec + (double)timeout.tv_usec / 1e6;

			double elapsedTimeSec = (double)(clock() - start) / CLOCKS_PER_SEC;
			
			remainingTimeoutSec = max(0, remainingTimeoutSec-elapsedTimeSec);

			double remainingSec;

			long remainingUs = modf(remainingTimeoutSec, &remainingSec) * 1e6;
			timeout.tv_usec = (long)remainingUs;
			timeout.tv_sec = remainingSec;
		}
		else if (ret == 0) {
			// report timeout or slow download
			if(curPos > 0)
				std::cout << "\t  Loading... failed with slow download" << std::endl;
			else
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
	// prevent shutting down null / nonexistant sockets
	if (sock == INVALID_SOCKET)
		return;

	if (closesocket(sock) == SOCKET_ERROR) {
		if (WSACleanup() == SOCKET_ERROR) {
			std::cout << "close socket failed: "<< WSAGetLastError() << std::endl;
			exit(5);
		}
	}
	sock = INVALID_SOCKET;
}