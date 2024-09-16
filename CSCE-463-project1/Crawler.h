// Name: Ivan Zaplatar
// Class: 464
// Semester: Fall 2024
#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <unordered_set>
#include <atomic>
#include <map>
#include <unordered_map>
#include <set>

class Crawler {
	
	public:
		Crawler(int);
		void insertJob(const std::string& rawUrl);
		void run();// starts N threads
		void runStat(); // starts the stat thread
		void updateSeenHosts(const std::string& host);
		void updateSeenIps(DWORD ip);
		bool seenHost(const std::string& host);
		bool seenIp(DWORD ip);
		void getCrawlerStats(std::vector<int>& , int);
		void endStatsThread(double);
		void getSummaryStats(double);
		int getTamuLinkCount();
		std::mutex jobsMtx;
		int N;

		std::mutex statsMtx;
		std::mutex seenTamuHostsMtx;
		std::thread statsThread;
		std::atomic<bool> stopStatsThread;
		std::vector<std::thread> workers;
		std::queue<std::string> Q;
		// Stats
		std::vector<int> bytesRead;
		std::vector<int> pagesRead;
		std::vector<int> extractedUrls;
		std::vector<int> uniqueHostPassed;
		std::vector<int> successfulDnsLookups;
		std::vector<int> uniqueIpPassed;
		std::vector<int> robotsCheckPassed;
		std::vector<int> robotsAttempted;
		std::vector<int> successfulCrawl;
		std::vector<int> linksFound;
		std::vector<int> dnsLookups;
		std::vector<int> tamuLinks;
		std::set<std::string> seenTamuHosts;
		std::map<std::string, std::vector<int>> httpCodes;
		// Uniqueness
		std::set<DWORD> ips;
		std::unordered_set<std::string> seenHosts;





		
};