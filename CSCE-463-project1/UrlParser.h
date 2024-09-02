// Name: Ivan Zaplatar
// Class: 436
// Semester: Fall 2024
#pragma once


#include "Url.h"
#include <vector>
#include <concepts>
#include <type_traits>
#include <iterator>
#include <string>
#include <stdexcept>
#include <iostream>

class UrlParser {
public:
	Url parse(const std::string& rawUrl) {
		
		std::string rawUrlCpy = rawUrl;

		std::vector<std::string> urlElements;

		std::vector<std::string> patterns = { "#", "?", "/", ":" };

		// Get scheme
		int schemeEnd = rawUrlCpy.find("://");
		std::string scheme;
		for (int i = 0; i < rawUrlCpy.length() && schemeEnd != -1 && i < schemeEnd + 3; i++) {
			if (i < schemeEnd) {
				scheme += rawUrlCpy[i];
			}
			rawUrlCpy[i] = '\0';
		}

		// Verify scheme
		if (scheme != "http"){
			std::cout << "\t  Parsing URL... failed with invalid scheme" << std::endl;
			return Url{"", "", -1, "", "", ""};
		}

		urlElements.push_back(scheme);

		// Get other parts of url
		for (std::string& pattern : patterns) {
			std::string urlElement;
			int patternStart = rawUrlCpy.find(pattern);
			for (int i = patternStart; i < rawUrlCpy.length() && patternStart != -1; i++) {
				urlElement = rawUrlCpy[i] != '\0' ? urlElement + rawUrlCpy[i] : urlElement;
				rawUrlCpy[i] = '\0';
			}
			urlElements.push_back(urlElement);
		}

		// Get the host
		std::string host;
		for (char c : rawUrlCpy) {
			host = c != '\0' ? host + c : host;
		}

		urlElements.push_back(host);

		// Convert port
		int port = 80;
		if (urlElements[4].length() != 0) {
			try {
				port = std::stoi(urlElements[4].substr(1));
			}
			catch (...) {
				port = -1;
			};
		}

		// Verify port
		if (!(1 <= port && port <= 65535)) {
			std::cout << "\t  Parsing URL... failed with invalid port" << std::endl;
			return Url{"", "", -1, "", "", "" };
		}

		// Construct the url with urlElements
		Url url;

		url.scheme = urlElements[0];
		url.host = urlElements[5];
		url.port = port;
		url.query = urlElements[2] == "?" ? "" : urlElements[2];
		url.path = urlElements[3].length() ? urlElements[3] : "/";
		url.fragment = urlElements[1];
		url.rawUrl = rawUrl;
		std::cout << "\t  Parsing URL... host " << url.host << ", port " << url.port << ", request " << std::endl;

		return url;

	}

	template<std::forward_iterator T>
	std::vector<Url> parse(T begin, T end) {
		using itValueType = typename std::iterator_traits<T>::value_type;

		if (!std::is_same<itValueType, std::string>()) {
			return {};
		}
		
		std::vector<Url> urls;
		for (auto it = begin; it != end; it++) {
			Url url = this->parse(*it);
			urls.push_back(url);
		}
		
		return urls;
	}

};