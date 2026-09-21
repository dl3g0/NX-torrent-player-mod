#pragma once

#include <functional>
#include <memory>
#include <string>

namespace introdb
{

struct Segment
{
    double startSec = 0.0;
    double endSec   = 0.0;
};

struct IntroSegments
{
    bool hasIntro       = false;
    Segment intro;
    bool hasRecap       = false;
    Segment recap;
    bool hasOutro       = false;
    Segment outro;
    bool hasPostCredits = false;
    Segment postCredits;
};

// Parses a Stremio videoId formatted like "tt0944947:1:4" into IMDb ID, season, and episode.
// Returns false if the format doesn't match a series episode.
bool parseEpisodeVideoId(const std::string& videoId, std::string& outImdbId, int& outSeason, int& outEpisode);

// Asynchronously queries IntroDB (https://api.introdb.app/segments) for segment timestamps.
// Uses an in-memory cache to avoid duplicate network requests for the same media.
// `alive` guards the callback from being invoked if the calling view has been destroyed.
void fetchSegmentsAsync(const std::string& imdbId, int season, int episode, bool isMovie,
                        std::shared_ptr<bool> alive,
                        std::function<void(IntroSegments)> callback);

// Clears the in-memory IntroDB cache.
void clearCache();

} // namespace introdb
