#pragma once

#include <borealis.hpp>

#include <functional>
#include <string>
#include <vector>

#include "player.hpp"   // PlayerArt / WatchInfo, handed to the screens below
#include "stremio.hpp"

// The Stremio drill-down: series -> seasons -> episodes -> addons -> streams.
// Each level is an Activity so B walks back up one step, which is what the
// Switch's back button is expected to do.

struct Row
{
    std::string label;
    std::string sub;     // second line, dimmer (file name, episode title, ...)
    std::string detail;  // right-aligned (seeders, size, episode count, ...)

    // Optional artwork on the left (an episode still). Both are needed: the id
    // keys the on-disk cache, the url is where to fetch it on a miss.
    std::string artId;
    std::string artUrl;

    // Watch progress through this row's video (0..1), from the Stremio library
    // state; negative = no bar. Only the last watched episode of a show carries
    // one -- Stremio stores a single position per library item.
    double progress = -1.0;
};

// A plain vertical list of selectable rows. Every level below is this list with
// different rows, so the navigation, focus and scrolling behave identically.
class ListActivity : public brls::Activity
{
  public:
    // `iconPath` is an on-disk image (the title's poster) shown next to the
    // header title; "" leaves the header icon hidden.
    ListActivity(std::string title, std::string subtitle, std::vector<Row> rows,
                 std::function<void(int)> onSelect, std::string iconPath = "");
    ~ListActivity() override;
    brls::View* createContentView() override;

    // Makes the list re-derive its rows when playback has since changed the
    // watch state (see stremio::libraryGen). `rebuild` returns the fresh rows;
    // it must not touch the network -- it is called on a focus event. Used by
    // the episode list so its progress bars update when you come back.
    void setRebuildOnReturn(std::function<std::vector<Row>()> rebuild);

    // Row the cursor starts on (default 0). The episode list uses it to land on
    // the in-progress episode when a started show is opened.
    void setInitialFocus(int index) { initialFocus = index; }

  private:
    std::string title, subtitle, iconPath;
    std::vector<Row> rows;
    std::function<void(int)> onSelect;

    brls::Box* listBox = nullptr;  // holds the RowCells, for in-place rebuild
    int initialFocus = -1;
    std::function<std::vector<Row>()> rebuildRows;
    brls::GenericEvent::Subscription focusSub {};
    bool focusSubbed = false;
    uint32_t seenGen = 0;
    // Guards the deferred rebuild: the activity may be popped before it runs.
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    void populate(brls::Box* list);
    void onGlobalFocus(brls::View* focused);
};

// Entry point: opens a library item. Films skip straight to the addon list;
// series go through season/episode pickers first.
void openLibraryItem(const std::string& authKey, const stremio::LibItem& item);

// Opens one episode's detail screen (synopsis + source cards) straight from its
// id, without going through the series/season pickers. For the player's
// end-of-episode card: it only ever knew the next episode's id, so everything
// else -- the title, the still, the episode AFTER that one -- has to be fetched,
// which is one Cinemeta round trip behind a loading screen. `art` carries the
// show's poster over; the episode's own still replaces the background.
//
// Declared here rather than being reached through a hook because the player
// already includes this header's neighbours, and a hook would only hide which
// screen actually opens.
// `replaceCurrent` says the caller is itself an episode screen being walked
// away from (the prev/next chevrons): no loading page is shown and the current
// activity is swapped out once the meta lands. False -- the player's card,
// which has already popped itself -- puts a loading page up meanwhile.
// `onFail` runs on the UI thread when the episode cannot be opened, so a
// caller that changed its own appearance to say "working" can put it back --
// success replaces the screen, so only failure ever returns.
void openEpisodeById(const std::string& authKey, const std::string& seriesId,
                     const std::string& videoId, const PlayerArt& art,
                     bool replaceCurrent          = false,
                     std::function<void()> onFail = nullptr);
