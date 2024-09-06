#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <unordered_set>
#include <atomic>

class Crawler {
	
	public:
		Crawler(int);
		void insertJob(const std::string& rawUrl);
		void run();// starts N threads
		void runStat(); // starts the stat thread
		void updateSeenHosts(const std::string& host);
		void updateSeenIps(const std::string& ip);
		bool seenHost(const std::string& host);
		bool seenIp(const std::string& ip);
		void getCrawlerStats();
		void endStatsThread();
		std::mutex jobsMtx;
		int N;

		std::mutex statsMtx;
		std::thread statsThread;
		std::atomic<bool> stopStatsThread;
		std::vector<std::thread> workers;
		std::queue<std::string> Q;
		// Stats
		std::vector<int> extractedUrls;
		std::vector<int> uniqueHostPassed;
		std::vector<int> successfulDnsLookups;
		std::vector<int> uniqueIpPassed;
		std::vector<int> robotsCheckPassed;
		std::vector<int> successfulCrawl;
		std::vector<int> linksFound;
		// Uniqueness
		std::unordered_set<std::string> seenHosts;
		std::unordered_set<std::string> seenIps;





		
};