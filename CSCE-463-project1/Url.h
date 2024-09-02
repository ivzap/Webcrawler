// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#pragma once

#include <string>

struct Url {
	std::string scheme;
	std::string host;
	int port;
	std::string path;
	std::string query;
	std::string fragment;
	std::string rawUrl;
};