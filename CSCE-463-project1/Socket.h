// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#pragma once

#include "pch.h"
#include "Url.h"
#include <ctime>
#include <unordered_set>

#define INITIAL_BUF_SIZE 1000
#define THRESHHOLD 10000
class Socket {
public:
	Socket(int timeout);
	~Socket();
	SOCKET sock; // socket handle
	
	char* buf; // current buffer
	int allocatedSize; // bytes allocated for buf
	int curPos; // current position in buffer
	int timeout;

	void updateSeenHosts(const std::string& host);
	void updateSeenIps(const std::string& ip);
	bool seenHost(const std::string& host);
	bool seenIp(const std::string& ip);
	bool Read(void);
	bool Connect(const Url& url, bool robotCheck);
	void Shutdown();

	private:
		std::unordered_set<std::string> seenHosts;
		std::unordered_set<std::string> seenIps;


};