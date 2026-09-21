#include "introdb.hpp"

#include <borealis.hpp>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>

#include "http.hpp"

namespace introdb
{

namespace
{

std::mutex g_cacheMutex;
std::map<std::string, IntroSegments> g_cache;

std::string cacheKey(const std::string& imdbId, int season, int episode, bool isMovie)
{
    if (isMovie)
        return imdbId + ":movie";
    return imdbId + ":" + std::to_string(season) + ":" + std::to_string(episode);
}

bool extractSegment(const std::string& json, const std::string& key, Segment& seg)
{
    std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat);
    if (k == std::string::npos) return false;
    size_t colon = json.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    size_t val = json.find_first_not_of(" \t\r\n", colon + 1);
    if (val == std::string::npos || json.compare(val, 4, "null") == 0) return false;
    if (json[val] != '{') return false;

    size_t endObj = json.find('}', val);
    if (endObj == std::string::npos) return false;
    std::string obj = json.substr(val, endObj - val + 1);

    auto getDouble = [](const std::string& str, const char* field) -> double {
        std::string p = std::string("\"") + field + "\"";
        size_t pos = str.find(p);
        if (pos == std::string::npos) return -1.0;
        size_t c = str.find(':', pos + p.size());
        if (c == std::string::npos) return -1.0;
        size_t numStart = str.find_first_not_of(" \t\r\n", c + 1);
        if (numStart == std::string::npos) return -1.0;
        char* endPtr = nullptr;
        double v = std::strtod(str.c_str() + numStart, &endPtr);
        if (endPtr == str.c_str() + numStart) return -1.0;
        return v;
    };

    double startSec = getDouble(obj, "start_sec");
    double endSec   = getDouble(obj, "end_sec");

    if (startSec < 0.0)
    {
        double startMs = getDouble(obj, "start_ms");
        if (startMs >= 0.0) startSec = startMs / 1000.0;
    }
    if (endSec < 0.0)
    {
        double endMs = getDouble(obj, "end_ms");
        if (endMs >= 0.0) endSec = endMs / 1000.0;
    }

    if (endSec > startSec && startSec >= 0.0)
    {
        seg.startSec = startSec;
        seg.endSec   = endSec;
        return true;
    }
    return false;
}

IntroSegments parseSegmentsResponse(const std::string& resp)
{
    IntroSegments res;
    res.hasIntro       = extractSegment(resp, "intro", res.intro);
    res.hasRecap       = extractSegment(resp, "recap", res.recap);
    res.hasOutro       = extractSegment(resp, "outro", res.outro);
    res.hasPostCredits = extractSegment(resp, "post_credits", res.postCredits);
    return res;
}

} // namespace

bool parseEpisodeVideoId(const std::string& videoId, std::string& outImdbId, int& outSeason, int& outEpisode)
{
    size_t c1 = videoId.find(':');
    if (c1 == std::string::npos) return false;
    size_t c2 = videoId.find(':', c1 + 1);
    if (c2 == std::string::npos) return false;

    outImdbId = videoId.substr(0, c1);
    std::string s = videoId.substr(c1 + 1, c2 - c1 - 1);
    std::string e = videoId.substr(c2 + 1);
    if (s.empty() || e.empty()) return false;
    if (s.find_first_not_of("0123456789") != std::string::npos) return false;
    if (e.find_first_not_of("0123456789") != std::string::npos) return false;

    try
    {
        outSeason  = std::stoi(s);
        outEpisode = std::stoi(e);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void fetchSegmentsAsync(const std::string& imdbId, int season, int episode, bool isMovie,
                        std::shared_ptr<bool> alive,
                        std::function<void(IntroSegments)> callback)
{
    if (imdbId.empty())
    {
        if (alive && *alive && callback) callback(IntroSegments{});
        return;
    }

    std::string key = cacheKey(imdbId, season, episode, isMovie);
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_cache.find(key);
        if (it != g_cache.end())
        {
            IntroSegments cached = it->second;
            brls::sync([alive, callback, cached]() {
                if (alive && !*alive) return;
                if (callback) callback(cached);
            });
            return;
        }
    }

    std::string url;
    if (isMovie)
    {
        url = "https://api.introdb.app/segments?imdb_id=" + http::urlEncode(imdbId) + "&is_movie=true";
    }
    else
    {
        url = "https://api.introdb.app/segments?imdb_id=" + http::urlEncode(imdbId) +
              "&season=" + std::to_string(season) +
              "&episode=" + std::to_string(episode);
    }

    brls::async([url, key, alive, callback]() {
        std::string resp, err;
        bool ok = http::get(url, resp, err);
        IntroSegments segs;
        if (ok && !resp.empty())
        {
            segs = parseSegmentsResponse(resp);
            brls::Logger::info("[introdb] fetched {} -> intro:{} recap:{} outro:{}",
                               key, segs.hasIntro, segs.hasRecap, segs.hasOutro);
        }
        else
        {
            brls::Logger::info("[introdb] no segments for {} ({})", key, err.empty() ? "empty" : err.c_str());
        }

        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            g_cache[key] = segs;
        }

        brls::sync([alive, callback, segs]() {
            if (alive && !*alive) return;
            if (callback) callback(segs);
        });
    });
}

void clearCache()
{
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_cache.clear();
}

} // namespace introdb
