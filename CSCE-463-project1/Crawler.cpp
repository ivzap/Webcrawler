// Name: Ivan Zaplatar
// Class: 464
// Semester: Fall 2024
#include "pch.h"
#include "Url.h"
#include "Crawler.h"
#include "UrlParser.h"
#include "Socket.h"
#include <numeric>
#include <chrono>


std::pair<std::vector<std::string>, std::string> getParsedResponse(const Socket& s) {
    std::vector<std::string> header;
    int i = 0;
    for (; i < s.curPos; i += 2) {
        std::string line;
        while (i < s.curPos && s.buf[i] != '\r') {
            line += s.buf[i++];
        }
        if (line.length() == 0) break;

        header.push_back(line);
    }
    // remove any leading \r\n to the html.
    while (i < s.curPos && (s.buf[i] == '\r' || s.buf[i] == '\n')) i++;

    std::string html;
    for (; i < s.curPos; i++) {
        html += s.buf[i];
    }
    return std::move(std::make_pair(std::move(header), std::move(html)));
}


bool sendSocketRequest(struct sockaddr_in& server, const int id, Socket& s, Crawler* c, std::shared_ptr<HTMLParserBase> parser, const Url& url, bool robotCheck, int maxRead) {

    if (url.scheme == "") return false;

    bool connected = s.Connect(server, url, robotCheck, id);

    if (!connected) return false;

    int validSocketRead = s.Read(server, maxRead, robotCheck, id);

    if (!validSocketRead) return false;

    auto response = getParsedResponse(s);

    std::string code;
    if (response.first[0].length() >= 12) {
        code = response.first[0].substr(9, 3);
    }
    else {
        return false;
    }

    // get code counts
    if (code[0] == '5') {
        c->httpCodes["5xx"][id]++;
    }
    else if (code[0] == '4') {
        c->httpCodes["4xx"][id]++;
    }
    else if (code[0] == '3') {
        c->httpCodes["3xx"][id]++;
    }
    else if (code[0] == '2') {
        c->httpCodes["2xx"][id]++;
    }
    else {
        c->httpCodes["other"][id]++;
    }

    if (robotCheck) {
        if (code[0] != '4') {
            return false;
        }
        c->robotsCheckPassed[id]++;
        return true;
    }

    if (code[0] != '2') {
        return false;
    }

    int nLinks;

    const char* rawUrlCopy = url.rawUrl.c_str();

    char* linkBuffer = parser->Parse((char*)response.second.c_str(), response.second.length(), (char*)rawUrlCopy, (int)strlen(rawUrlCopy), &nLinks);

    // check for errors indicated by negative values
    if (nLinks < 0)
        nLinks = 0;

    c->linksFound[id] += nLinks;

    return true;
}

void Crawler::runStat() {
    statsThread = std::thread([this]() {
        // TODO: if we have processed everything from Q, STOP thread
        std::vector<int> prevPeriodValues(2, 0); // stores pages read, bytes read respectively
        int t = 0;
        while (!stopStatsThread) {
            this->getCrawlerStats(prevPeriodValues, t);
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            t += 2;
        }
    });

}
// elapsed time in seconds
void Crawler::getSummaryStats(double elapsedTime) {
    std::unique_lock<std::mutex> lock(statsMtx);
    long long int nExtractedUrls = accumulate(extractedUrls.begin(), extractedUrls.end(), 0);
    long long int nDns = accumulate(dnsLookups.begin(), dnsLookups.end(), 0);
    long long int nSiteRobots = accumulate(uniqueIpPassed.begin(), uniqueIpPassed.end(), 0);
    long long int nCrawled = accumulate(pagesRead.begin(), pagesRead.end(), 0);
    long long int nParsed = accumulate(linksFound.begin(), linksFound.end(), 0);
    double crawledDataMB = accumulate(bytesRead.begin(), bytesRead.end(), 0.0) / 1000000.0;
    std::map<std::string, int> codes;
    for (auto [code, threadSums] : httpCodes) {
        codes[code] = accumulate(threadSums.begin(), threadSums.end(), 0);
    }
    printf("Extracted %ld URLs @ %ld/s\n",
        nExtractedUrls, (long long int)(nExtractedUrls / elapsedTime));
    printf("Looked up %ld DNS names @ %ld/s\n", nDns, (long long int)(nDns / elapsedTime));
    printf("Attempted %ld site robots @ %ld/s\n", nSiteRobots, (long long int)(nSiteRobots / elapsedTime));
    printf("Crawled %ld pages @ %ld/s (%.2f MB)\n", nCrawled, (long long int)(nCrawled / elapsedTime), crawledDataMB);
    printf("Parsed %ld links @ %ld/s\n", nParsed, (long long int)(nParsed / elapsedTime));
    printf("HTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, other = %d\n",
        codes["2xx"], codes["3xx"], codes["4xx"], codes["5xx"], codes["other"]);
}

/*
Store a previous cnt array for both pages crawled and total bytes we crawled

if we get a response back we consider it 

*/
void Crawler::getCrawlerStats(std::vector<int>& prevPeriodValues, int time) {
    int qSize;
    std::unique_lock<std::mutex> lock(statsMtx);
    {
        std::unique_lock<std::mutex> lock(jobsMtx);
        qSize = Q.size();
    }
    
    int totalPagesRead = std::accumulate(pagesRead.begin(), pagesRead.end(), 0);
    int totalBytesRead = std::accumulate(bytesRead.begin(), bytesRead.end(), 0);
    
    printf("[%3d] Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK\n",
        time,
        qSize,
        std::accumulate(extractedUrls.begin(), extractedUrls.end(), 0),
        std::accumulate(uniqueHostPassed.begin(), uniqueHostPassed.end(), 0),
        std::accumulate(successfulDnsLookups.begin(), successfulDnsLookups.end(), 0),
        std::accumulate(uniqueIpPassed.begin(), uniqueIpPassed.end(), 0),
        std::accumulate(robotsCheckPassed.begin(), robotsCheckPassed.end(), 0),
        std::accumulate(pagesRead.begin(), pagesRead.end(), 0),
        std::accumulate(linksFound.begin(), linksFound.end(), 0)/1000);
    printf("      *** crawling %.1f pps @ %.1f Mbps\n", (totalPagesRead - prevPeriodValues[0])/2.0, (totalBytesRead - prevPeriodValues[1]) / 2.0 / 1000000.0);
    prevPeriodValues[0] = totalPagesRead;
    prevPeriodValues[1] = totalBytesRead;
}

void Crawler::insertJob(const std::string& rawUrl) {
    Q.push(rawUrl);
}


void Crawler::updateSeenIps(DWORD ip) {
    this->ips.insert(ip);
}

void Crawler::updateSeenHosts(const std::string& host) {
    this->seenHosts.insert(host);
}

bool Crawler::seenHost(const std::string& host) {
    return this->seenHosts.contains(host);
}

bool Crawler::seenIp(DWORD ip) {
    return this->ips.contains(ip);
}

void Crawler::endStatsThread(double elapsedTime) {
    stopStatsThread.store(true);

    getSummaryStats(elapsedTime);
}

// allows n number of threads to crawler
Crawler::Crawler(int n) {
	N = n;
    bytesRead.resize(n);
    pagesRead.resize(n);
    extractedUrls.resize(n);
    uniqueHostPassed.resize(n);
    successfulDnsLookups.resize(n);
    uniqueIpPassed.resize(n);
    robotsCheckPassed.resize(n);
    successfulCrawl.resize(n);
    linksFound.resize(n);
    dnsLookups.resize(n);
    robotsAttempted.resize(n);
    stopStatsThread.store(false);
    httpCodes["2xx"] = std::vector<int>(n, 0);
    httpCodes["3xx"] = std::vector<int>(n, 0);
    httpCodes["4xx"] = std::vector<int>(n, 0);
    httpCodes["5xx"] = std::vector<int>(n, 0);
    httpCodes["other"] = std::vector<int>(n, 0);

}

// run all threads including stat thread and start crawling
void Crawler::run() {
    this->runStat();
    clock_t start = clock();
	for (int i = 0; i < N; i++) {
		workers.emplace_back([this, i]() {
			UrlParser p;
			std::shared_ptr<HTMLParserBase> parser(new HTMLParserBase);
            Socket s(10, this);

			for(;;){
				// get job from the Q, threadSafe
                std::string rawUrl;
                {
                    std::lock_guard<std::mutex> lock(jobsMtx);
                    if (Q.size() == 0) {
                        break;
                    }

                    rawUrl = Q.front(); Q.pop();
                    this->extractedUrls[i]++;
                }
                // crawl on rawUrl
                Url url = p.parse(rawUrl);

                {
                    std::unique_lock<std::mutex> lock(this->statsMtx);
                    if (this->seenHost(url.host)) {
                        continue;
                    }
                    this->updateSeenHosts(url.host);
                    this->uniqueHostPassed[i]++;
                }

                // get ip
                struct sockaddr_in server;
                memset(&server, 0, sizeof(server));

                server.sin_family = AF_INET; // IPv4

                unsigned long ipv4 = inet_addr(url.host.c_str());

                if (ipv4 != INADDR_NONE) {
                    // its actually a ip address
                    // have we already seen it?
                    server.sin_addr.s_addr = ipv4;
                }
                else {
                    // we need to find the ip via dns request
                    struct hostent* host;
                    this->dnsLookups[i]++;
                    if ((host = (hostent*)gethostbyname(url.host.c_str())) == nullptr) {
                        // failed dns request
                        continue;
                    }
                    server.sin_addr = *((struct in_addr*)host->h_addr_list[0]);
                    this->successfulDnsLookups[i]++;
                }

                ipv4 = server.sin_addr.s_addr;

                server.sin_port = htons(url.port);
                {
                    std::unique_lock<std::mutex> lock(this->statsMtx);

                    if (ips.contains(ipv4)) {
                        // failed ip uniqueness test
                        continue;
                    }
                    else {
                        ips.insert(ipv4);
                        this->uniqueIpPassed[i]++;
                    }
                }

                if (sendSocketRequest(server, i, s, this, parser, url, true, 16 * 1024)) {
                    // download the page request
                    s.Shutdown();
                    sendSocketRequest(server, i, s, this, parser, url, false, 2097152);
                }
                s.Shutdown();
                

			}
		});
	}

    // wait for worker jobs to finish
    for (auto& thread : workers) {
        thread.join();
    }

    // wait for our stats thread to finish
    double elapsedTime = double(clock() - start) / CLOCKS_PER_SEC;
    this->endStatsThread(elapsedTime);
    this->statsThread.join();

}