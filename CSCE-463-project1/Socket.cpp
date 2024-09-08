// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#include "pch.h"
#include "Socket.h"
#include "Url.h"
#include <synchapi.h>
#include <iostream>
//#include <winsock.h>

Socket::~Socket() {
	delete[] buf;
	Shutdown();
}

// socket class should always set sock to invalid_sock when done with it, never leave it dangling.
Socket::Socket(int timeout, Crawler* crawler)
{
	this->crawler = crawler;
	sock = INVALID_SOCKET;
	buf = new char[INITIAL_BUF_SIZE];
	allocatedSize = INITIAL_BUF_SIZE;
	this->timeout = timeout;
	curPos = 0;
}

// id is the thread id currently from the crawler
bool Socket::Connect(const Url& url, bool robotCheck, const int id) {
	// if we reuse object dont let buffer grow to inf
	if (allocatedSize > 32 * 1024) {
		//std::cout << "Buffer exceeded 32KB, resizing to original size..." << std::endl;
		void* newBuf = realloc(buf, INITIAL_BUF_SIZE);
		if (newBuf == nullptr) {
			return false;
		}
		curPos = 0;
		memset(buf, '\0', INITIAL_BUF_SIZE * sizeof(char));
		allocatedSize = INITIAL_BUF_SIZE;
		buf = (char*)newBuf;

	}

	if (sock != INVALID_SOCKET) {
		this->Shutdown();
	}

	if(robotCheck)
	{

		std::unique_lock<std::mutex> lock(crawler->statsMtx);
		if (crawler->seenHost(url.host)) {
			return false;
		}
		crawler->updateSeenHosts(url.host);
		crawler->uniqueHostPassed[id]++;
	}

	if (robotCheck) {
		// host uniqueness passed, increment unique hosts counter
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
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
		// TODO: only dns on robot to prevent doing it twice
		struct hostent* host;
		if ((host = (hostent*)gethostbyname(url.host.c_str())) == nullptr) {
			
			return false;
		}
		server.sin_addr = *((struct in_addr*)host->h_addr_list[0]);
		if (robotCheck) {
			std::unique_lock<std::mutex> lock(crawler->statsMtx);
			crawler->successfulDnsLookups[id]++;
		}
	}

	std::string ip = inet_ntoa(server.sin_addr);

	server.sin_port = htons(url.port);

	clock_t dnsEnd = clock();

	double dnsElapsed = (double)(dnsEnd - dnsStart) / CLOCKS_PER_SEC;


	if (robotCheck) {
		std::unique_lock<std::mutex> lock(crawler->statsMtx);
		if(crawler->seenIp(ip)) {
			return false;
		}
		crawler->updateSeenIps(ip);
		crawler->uniqueIpPassed[id]++;
	}


	//

	clock_t connStart = clock();

	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
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
		if (robotCheck) {
			// connecting on robots failed
		}
		else {
			// normal page conection failed
		}
		return false;
	}

	return true;

}

bool Socket::Read(int maxRead, bool robotCheck, const int id)
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
			
			// have we exceeded our max limit?
			if (curPos > maxRead) {
				return false;
			}
			if (bytes == SOCKET_ERROR) {
				return false;
			}
			if (bytes == 0) {
				
				// TODO: increment ids pages parsed
				buf[curPos] = '\0';
				// verify valid http(HTTP/) response
				if (strncmp(buf, "HTTP/", 5) != 0) {
					return false;
				}

				if (!robotCheck)
					crawler->pagesRead[id]++;
				
				return true; // normal completion
			}
			curPos += bytes; // adjust where the next recv goes
			// TODO: adjust how many bytes process id has read by bytes
			if(!robotCheck)
				crawler->bytesRead[id] += bytes;
			if (allocatedSize - curPos < THRESHHOLD) { // always require 1000 extra bytes, performance sensitive here!
				void* newBuf = realloc(buf, allocatedSize + THRESHHOLD);
				if (newBuf == nullptr){
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
			if (curPos > 0) {
				// slow download
			}
			else {
				// timeout
			}

			return false;
		}
		else {
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