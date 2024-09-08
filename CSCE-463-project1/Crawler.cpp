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


bool sendSocketRequest(const int id, Socket& s, Crawler* c, std::shared_ptr<HTMLParserBase> parser, const Url& url, bool robotCheck, int maxRead) {

    if (url.scheme == "") return false;

    bool connected = s.Connect(url, robotCheck, id);

    if (!connected) return false;

    int validSocketRead = s.Read(maxRead, robotCheck, id);

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
        while (!stopStatsThread) {
            this->getCrawlerStats(prevPeriodValues);
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    });

}

/*
Store a previous cnt array for both pages crawled and total bytes we crawled

if we get a response back we consider it 

*/
void Crawler::getCrawlerStats(std::vector<int>& prevPeriodValues) {
    std::unique_lock<std::mutex> lock(statsMtx);
    {
        std::unique_lock<std::mutex> lock(jobsMtx);
        std::cout << "Current size of pending queue: "
            << Q.size()
            << std::endl;
    }

    std::cout << "Number of extracted URLs from queue: "
        << std::accumulate(extractedUrls.begin(), extractedUrls.end(), 0)
        << std::endl;
    std::cout <<"Number of URLs that have passed host uniqueness : " 
        << std::accumulate(uniqueHostPassed.begin(), uniqueHostPassed.end(), 0)
        << std::endl;
    int totalPagesRead = std::accumulate(pagesRead.begin(), pagesRead.end(), 0);
    int totalBytesRead = std::accumulate(bytesRead.begin(), bytesRead.end(), 0);
    std::cout << (totalPagesRead - prevPeriodValues[0]) / 2 <<" pps, "
        << (double)(totalBytesRead - prevPeriodValues[1])/2.0/1000000.0 << " Mbps" << std::endl;
    prevPeriodValues[0] = totalPagesRead;
    prevPeriodValues[1] = totalBytesRead;
}

void Crawler::insertJob(const std::string& rawUrl) {
    Q.push(rawUrl);
}


void Crawler::updateSeenIps(const std::string& ip) {
    this->seenIps.insert(ip);
}

void Crawler::updateSeenHosts(const std::string& host) {
    this->seenHosts.insert(host);
}

bool Crawler::seenHost(const std::string& host) {
    return this->seenHosts.contains(host);
}

bool Crawler::seenIp(const std::string& ip) {
    return this->seenHosts.contains(ip);
}

void Crawler::endStatsThread() {
    stopStatsThread.store(true);
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

                if (sendSocketRequest(i, s, this, parser, url, true, 16 * 1024)) {
                    // download the page request
                    s.Shutdown();
                    sendSocketRequest(i, s, this, parser, url, false, 2097152);
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
    this->endStatsThread();
    this->statsThread.join();

}