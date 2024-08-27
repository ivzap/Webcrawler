// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#pragma once

#include "pch.h"
#include "Url.h"
#include <ctime>

#define INITIAL_BUF_SIZE 1024
#define THRESHHOLD 100
class Socket {
public:
	Socket(int timeout);
	~Socket();
	SOCKET sock; // socket handle
	char* buf; // current buffer
	int allocatedSize; // bytes allocated for buf
	int curPos; // current position in buffer
	int timeout;

	bool Read(void);
	bool Connect(const Url& url);
	void Shutdown();

};