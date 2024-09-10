// Name: Ivan Zaplatar
// Class: 464
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
bool Socket::Connect(struct sockaddr_in& server, const Url& url, bool robotCheck, const int id) {
	// if we reuse object dont let buffer grow to inf
	if (allocatedSize > 32 * 1024) {
		void* newBuf = realloc(buf, INITIAL_BUF_SIZE);
		if (newBuf == nullptr) {
			return false;
		}
		curPos = 0;
		allocatedSize = INITIAL_BUF_SIZE;
		buf = (char*)newBuf;

	}

	if (sock != INVALID_SOCKET) {
		this->Shutdown();
	}

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		return false;
	}

	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		return false;
	}

	// send get request
	std::string urlAndPath = robotCheck ? "/robots.txt" : url.path + url.query;
	std::string reqType = robotCheck ? "HEAD" : "GET";
	std::string sendBuf = reqType + " " + urlAndPath + " " + "HTTP/1.0\r\nUser-Agent: myTamuCrawler/1.0\r\nHost:" + " " + url.host + "\r\nConnection: close\r\n\r\n";
	
	if (send(sock, sendBuf.c_str(), sendBuf.length(), 0) == SOCKET_ERROR)
	{
		return false;
	}

	return true;

}

bool Socket::Read(struct sockaddr_in& server, int maxRead, bool robotCheck, const int id)
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