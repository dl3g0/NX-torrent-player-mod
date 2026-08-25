#pragma once

#include <string>
#include <vector>

// User settings, persisted to sdmc:/switch/nx-torrent-player/config.json and
// opened with X from the browser. Everything here has a default that matches
// what the app did before it was configurable, so a missing (or corrupt) file
// is not an error -- it just means "defaults".
namespace config
{

enum class Tab
{
    LOCAL   = 0,
    STREMIO = 1,
};

// The default UI sizes, named so the Options screen can mark them in the list.
// Docked defaults smaller than handheld: the same logical size that fills a 6"
// panel leaves most of a TV empty.
constexpr int kDefaultDockedUiWidth   = 1600;
constexpr int kDefaultHandheldUiWidth = 1280;

struct Config
{
    int configVersion = 4;

    // Which category the browser opens on.
    Tab startupTab = Tab::STREMIO;

    // Writes the log to APPDATA_LOG. Off by default: it is unbuffered and the
    // engine dumps a [stats] line every 2s, so it writes to the SD card
    // continuously for the whole session. Worth it when diagnosing, not
    // otherwise.
    bool logging = false;

    // Preferred track languages, as ISO-639-1 ("es") or "auto".
    std::string audioLang = "es";
    std::string subLang   = "es";

    // Show subtitles at all. Off means mpv loads none rather than picking one.
    bool subtitles = false;

    // Lift the loudness of a 5.1 track downmixed to stereo (mpv's dynaudnorm).
    // On by default -- a 5.1 master folded to stereo sits well below a native
    // stereo track on the Switch's own speakers. Handheld only, and switchable
    // in Options.
    bool audioBoost = true;

    // Hardware-accelerated video decode. On by default. Off decodes in
    // software (ffmpeg) -- slower, and high-bitrate 1080p may drop frames, but
    // useful as a fallback if the nvtegra path ever misbehaves on a release.
    bool hwDecode = true;

    // Holds pieces of the in-flight video in RAM rather than writing them to
    // the SD card. Faster, saves SD wear and avoids SD write stalls; playback
    // cannot be resumed after the app closes and seek-back is limited to the
    // RAM buffer (kRamStreamBufferSize in torrentfs.c). Off by default.
    bool ramStream = true;

    // Once the playback buffer is comfortably ahead (>= 30 s), cap download
    // throughput to ~2x the stream's bitrate instead of bursting at full line
    // rate. Off by default (full line rate always).
    bool rateGovernor = false;

    // Filter 4K (2160p) sources from Stremio addon stream lists so the user is
    // not offered streams the Switch's decoder cannot handle smoothly. On by
    // default.
    bool hide4k = true;

    // Check GitHub for a newer release on launch. Free when there is no wifi
    // (the request simply fails and the app carries on), non-blocking.
    bool checkUpdates = true;

    // Logical UI width to lay out in, for docked and handheld separately. One
    // of kUiScales (1280 to 1920). 1280 is 100% (the stock size).
    int dockedUiWidth   = kDefaultDockedUiWidth;
    int handheldUiWidth = kDefaultHandheldUiWidth;

    // Colour scheme, as a theme::Scheme id. Purple is what the app has always
    // looked like. An id we do not know falls back to it at read time (see
    // theme::current()), so this is not validated here.
    std::string accent = "purple";

    // The UI language, as an i18n::langIds() id: "es" (default), "en" or "fr".
    std::string language = "es";

    // "dark", "light" or "system" (the console's own setting). Dark is what the
    // app has always been. Read at startup only: borealis does not support
    // swapping the variant while running. Anything unrecognised behaves as
    // "dark" (see theme::isLight()).
    std::string themeVariant = "dark";

    // How the Stremio tab draws its movies and shows: "posters" (upright cards
    // in a horizontally scrolling strip, the default), "cards" (a column of
    // rows, poster beside a raised panel with the title, the episode or the
    // year, the position and the progress bar) or "classic" (the flat row the
    // app shipped with). Anything unrecognised behaves as "cards" -- the style
    // is compared against the other two, never validated at read time.
    std::string listStyle = "posters";
};

// The live settings. Mutate, then call save().
Config& get();

// The console's language as ISO-639-1 ("fr"), or "en" if it cannot be read.
std::string consoleLang();

// An ISO-639-1 code (or "auto") turned into what mpv's alang/slang want: a
// comma-separated list, because a track's language tag can be 639-1 ("fr") or
// either 639-2 form ("fre" bibliographic / "fra" terminological) depending on
// who muxed the file. "" when there is nothing to prefer.
std::string mpvLangList(const std::string& code);

// The languages the Options screen offers, "auto" first. Codes and labels are
// index-matched.
const std::vector<std::string>& langCodes();
const std::vector<std::string>& langLabels();

// A language tag as somebody else spelled it -- a track's "lang" ("fre"), a
// Stremio subtitle addon's ("fr", "eng", sometimes "French") -- turned into a
// name to show. Unknown tags come back capitalised but otherwise untouched,
// which is the honest answer for the long tail of what addons send.
std::string langLabelFor(const std::string& tag);

// The subtitle language actually preferred right now: the setting, with "auto"
// resolved to the console's own language. ISO-639-1.
std::string preferredSubLang();

// The UI sizes the Options screen offers: logical widths (what dockedUiWidth /
// handheldUiWidth store) and their labels, index-matched. One list for both
// modes -- a label is the size relative to 1280, which does not depend on the
// output it is drawn to.
const std::vector<int>& uiWidths();
const std::vector<std::string>& uiWidthLabels();

// Reads config.json. Called once at startup, before the UI is built. Missing
// file / unreadable keys keep their default.
void load();

// Writes config.json. False if the SD card refused it.
bool save();

} // namespace config
