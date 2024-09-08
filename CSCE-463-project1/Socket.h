// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#pragma once

#include "pch.h"
#include "Url.h"
#include <ctime>
#include <unordered_set>
#include "Crawler.h"

#define INITIAL_BUF_SIZE 3000
#define THRESHHOLD 20000
class Socket {
public:
	Socket(int timeout, Crawler* crawler);
	~Socket();
	SOCKET sock; // socket handle
	
	char* buf; // current buffer
	int allocatedSize; // bytes allocated for buf
	int curPos; // current position in buffer
	int timeout;

	bool Read(int maxRead, bool robotCheck, const int id);
	bool Connect(const Url& url, bool robotCheck, const int id);
	void Shutdown();

	private:
		Crawler* crawler;
	

};