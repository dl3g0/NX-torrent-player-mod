#pragma once

#include <borealis.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "stremio.hpp"  // stremio::Subtitle, for the addon subtitle picker

struct mpv_handle;
struct mpv_render_context;
struct torrentfs;

// Fired once a magnet's on-device metadata resolves, with the magnet URI played
// and the real torrent name. Lets the Local tab fill in a name it did not have
// yet (a magnet added without a dn=). Set to nullptr to disable.
void setMagnetResolvedHook(
    std::function<void(const std::string& magnet, const std::string& name)> hook);

// A borealis view that streams a torrent (path to a .torrent file, or a magnet:
// URI) through our on-device engine and renders it full-screen with mpv on top
// of the borealis OpenGL context. Until mpv presents its first frame a borealis
// loading screen (buffering bar + torrent stats) is shown on top of the video.
// What the loading screen shows while the torrent opens. Empty = the app logo,
// no background (a local .torrent tells us nothing about what is inside it).
struct PlayerArt
{
    // On-disk image shown in the middle of the loading screen: the film's or the
    // show's poster. The cached thumbnail is enough at that size.
    std::string posterPath;

    // Full-screen horizontal background (16:9 backdrop)
    std::string bgId;   // cache key
    std::string bgUrl;  // where to fetch it

    // Title logo image (transparent PNG)
    std::string logoId;
    std::string logoUrl;
};

// Ties a playback to its Stremio library entry, so the player can resume where
// the account left off and report the position back (datastorePut). All fields
// empty (the default) = not a Stremio playback, nothing is sent anywhere.
struct WatchInfo
{
    std::string authKey;
    std::string itemId;   // library _id ("tt1234567")
    std::string videoId;  // what is playing: itemId for a film, "tt123:1:3" ep
    // "movie" or "series": the other half of the (type, id) pair every addon
    // resource is addressed by. Only the subtitle lookup needs it -- the stream
    // was already picked by the browser before the player was pushed.
    std::string type;
    // The episode after this one, in series order. When an episode plays to its
    // end, the library pointer is advanced to this so the show stays in Continue
    // Watching on the next episode instead of vanishing. Empty for films and the
    // last episode.
    std::string nextVideoId;
    double resumeSec = 0.0;  // start playback here (0 = from the beginning)
    std::string displayTitle;  // shown top-left on pause; "" falls back to the
                               // name derived from the source

    // Activities to pop when the video plays to its end (not on B). 1 closes
    // just the player (local). A Stremio play sits under the addon + source
    // lists, so 3 returns to the library (film) or the episode list (series).
    int endPop = 1;
};

class MpvView : public brls::Box
{
  public:
    // `art` is the artwork for the loading screen (see PlayerArt). `title`
    // overrides the name derived from the source, which for a magnet is just
    // "Torrent". `fileIndex` selects which file of a multi-file torrent to
    // stream; -1 (the default) streams the largest one.
    explicit MpvView(const std::string& source, const PlayerArt& art = {},
                     const std::string& title = "", int fileIndex = -1,
                     WatchInfo watch = {});
    ~MpvView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    void pumpEvents();
    void startEngine(const std::string& source, int fileIndex);
    bool startMpv();
    void registerPlayerActions();
    void buildLoadingOverlay(const std::string& title);
    void setBackgroundArt(const std::string& path);
    PlayerArt art;
    brls::Image* bgImage = nullptr;  // filled in once the full-size art lands

    // Opening a magnet announces to trackers and pulls the metadata off peers
    // (BEP 9) -- seconds of blocking work. It runs on a background thread, so
    // this guards against the result landing after the view is gone.
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    void retryStream();
    int requestedFileIndex = -1;
    bool isHttpStream = false;
    bool isLocalFile = false;
    std::string streamSource;
    bool engineFailed = false;
    std::string engineError;
    void updateLoadingOverlay();
    void updateBufferIndicator();
    void updateInfoOverlay();
    void updateSeekBar();

    // X during playback: a modal popup to switch the current video's audio and
    // subtitle track. Pauses playback behind it (it stays paused on close, A
    // resumes), and reads the tracks straight off mpv.
    void openTrackMenu();

    // Subtitles from the account's Stremio subtitle addons. The list is asked
    // for once, in the background, as soon as mpv has opened the file -- so by
    // the time anyone presses X it is there and the menu can just show it.
    // Needs watch.authKey/type/videoId; a local .torrent has none of those and
    // the row says so instead.
    enum class SubFetch
    {
        Idle,    // not asked (no Stremio context, or the file is not open yet)
        Busy,    // in flight
        Done,    // answered; onlineSubs may still be empty
        Failed,  // every addon errored -- onlineSubErr says why
    };
    void fetchOnlineSubs();
    // Downloads `index` of onlineSubs and hands it to mpv, selected.
    void loadOnlineSub(int index);
    std::vector<stremio::Subtitle> onlineSubs;
    SubFetch onlineSubState = SubFetch::Idle;
    std::string onlineSubErr;
    // Which of onlineSubs have been handed to mpv. They are mpv tracks from
    // then on, so the menu lists them from the track list and skips them here
    // -- the same subtitle must not appear twice in the one Subtitles list.
    std::set<int> loadedSubs;

    // Subtitle timing offset (mpv "sub-delay"), in seconds: positive shows a
    // line later. L/R adjust it, but only while the settings panel is open --
    // in the player itself those two buttons are the speed (see nudgeSpeed).
    double subDelay = 0.0;
    void setSubDelay(double seconds);  // clamps, pushes to mpv, reports
    // Set by the settings panel to its delay row, so an L/R nudge updates the
    // number on screen. Null when the panel is closed.
    std::function<void(double)> subDelaySink;

    // Playback speed, stepped through kSpeeds by L/R in the player.
    double playSpeed = 1.0;
    void nudgeSpeed(int dir);  // dir -1 slower, +1 faster

    // The "next episode" card, shown over the last seconds of an episode that
    // has one. A (while playing) or a tap on it leaves the player and opens the
    // next episode's sources; ignoring it just lets the file end as before.
    void updateNextCard();     // per frame: show/hide against the time left
    void goToNextEpisode();    // pop out of the player and open the next one
    brls::Box* nextCard      = nullptr;
    brls::Label* nextCardSub = nullptr;  // "Season 1 . Episode 4"
    bool nextCardShown       = false;

    // The settings panel is up. Suppresses the per-frame stick scrub, which is
    // polled straight off the controller and so would otherwise keep seeking
    // the video from under a menu that has the cursor.
    bool settingsOpen = false;

    // A pill, top centre, that shows what an L/R press just did and fades out
    // again. Text only: it says "Subtitles +0.10 s" or "Speed 1.25x", which
    // needs no glyph to be understood.
    void flashPill(const std::string& text);
    void updatePill();  // hides it again after its moment
    brls::Box* hintPill      = nullptr;
    brls::Label* pillLabel   = nullptr;
    bool pillFlashActive     = false;
    std::chrono::steady_clock::time_point pillFlashUntil;

    void onPlayPause();                 // the A-button behaviour, reused by a tap
    void setControlsVisible(bool show); // pause overlay + title + hint + seek bar
    void seekToFraction(double frac);   // seek to frac (0..1) of the duration
    void handleVideoTap(float x, float y);
    void triggerMultiStepSeek(int side, bool fromTouch = false);
    void updateDoubleTapSeek();
    int tapSide = 0;
    int consecutiveTapCount = 0;
    int cumulativeSeekSecs = 0;
    bool pendingTouchTap = false;
    std::chrono::steady_clock::time_point lastTapTime;
    std::chrono::steady_clock::time_point pendingSingleTapTime;
    bool pendingSingleTap = false;

    // Control lock (Y). Blocks every player input -- buttons, the stick and touch
    // -- for the session, so a running video is not disturbed by an accidental
    // press. Y toggles it. While locked, any other input flashes the lock hint
    // (top-right) instead of acting; updateLockHint hides it again after a moment.
    void toggleLock();
    void flashLock();
    void updateLockHint();
    bool controlsLocked  = false;
    bool lockFlashActive = false;
    std::chrono::steady_clock::time_point lockFlashUntil;

    // Dumps every engine counter to the log on a fixed interval, independently
    // of the ZR panel. Single snapshots hide the shape of the problem (a rate
    // read just before it collapses looks healthy); the trend is the evidence.
    void logStats();

    // Whether mpv currently has the loudness-boost filter on its chain. The
    // boost is handheld-only and the console can be docked mid-film, so this
    // tracks what was pushed to mpv to keep pumpEvents from re-setting "af"
    // every frame.
    bool boostApplied = false;

    torrentfs* tfs = nullptr;
    mpv_handle* mpv = nullptr;
    mpv_render_context* renderCtx = nullptr;

    // Loading screen (borealis views drawn as children, hidden once ready).
    brls::Box* loadingOverlay = nullptr;
    brls::Label* titleLabel   = nullptr;
    brls::Label* statusLabel  = nullptr;
    brls::Label* statsLabel   = nullptr;
    brls::Label* percentLabel = nullptr;
    brls::Box* barTrack       = nullptr;
    brls::Box* barFill        = nullptr;
    brls::ProgressSpinner* loadingSpinner = nullptr;

    // Small "buffering" spinner shown mid-playback when the stream stalls.
    brls::Box* bufferOverlay = nullptr;
    bool buffering           = false;

    // Pause icon + seek bar (elapsed / total time) shown while paused with A.
    brls::Box* pauseOverlay  = nullptr;
    brls::Box* pauseTitleBox = nullptr;  // title, top-left, shown while paused
    std::string pauseTitle;              // what it says
    brls::Box* optionsHint   = nullptr;  // "X Options" hint, top-right while paused
    brls::Box* speedPill     = nullptr;  // Live download speed indicator
    brls::Label* speedLabel  = nullptr;  // "📥 2.4 MB/s"
    void updateSpeedIndicator();
    std::chrono::steady_clock::time_point speedLastSample;
    int64_t speedLastBytes = 0;
    brls::Box* lockHint      = nullptr;  // Y-lock hint / indicator, far top-right
    brls::Label* lockLabel   = nullptr;  // padlock glyph: open (unlocked) / closed (locked)
    brls::Box* seekOverlay   = nullptr;
    brls::Box* seekFill      = nullptr;
    brls::Label* seekCur     = nullptr;
    brls::Label* seekTotal   = nullptr;
    bool userPaused          = false;
    // Whether the controls overlay is on screen. Decoupled from userPaused: a
    // touch on the video toggles this without changing playback, and the centre
    // button toggles playback. (Gamepad A still does both together.)
    bool controlsShown       = false;
    // Auto-hide: while playing, the overlay hides itself a few seconds after it
    // was shown (paused, it stays up). Reset whenever it is (re)shown.
    void updateControlsAutoHide();
    std::chrono::steady_clock::time_point controlsHideAt;

    // Scrubbing with the stick: left/right pause playback and move a target
    // along the seek bar; A commits the seek and resumes. The engine follows --
    // seeking moves the torrent playhead (source/stream.c seek_cb), so the
    // download window re-centres on wherever you land.
    bool seeking      = false;
    double seekTarget = 0.0;  // seconds, where the cursor currently points
    double seekDur    = 0.0;  // duration cached when scrubbing started
    brls::Box* seekCursor = nullptr;
    brls::Box* seekTrack  = nullptr;  // measured, to place fill+cursor in pixels

    // Analog scrub: driven per-frame from the stick axis, so it needs its own
    // clock and a hold timer to ramp the rate.
    void updateStickSeek();
    void beginSeek();
    // Abandon an in-progress scrub without committing: the video never left the
    // frame it paused on, so this just drops the seek bar and restores the
    // play/pause the user had. B calls it instead of leaving the stream.
    void cancelSeek();
    double seekHeld = 0.0;  // seconds the stick has been held off-centre
    // Was the stick off-centre last frame? The stick is an axis, not an event, so
    // this is what turns it into one -- needed to flash the lock pill once per
    // push while locked instead of on every frame it is held.
    bool stickWasPushed = false;
    std::chrono::steady_clock::time_point seekLastFrame;
    bool seekFrameValid = false;

    // Semi-transparent network/torrent info panel, toggled with ZR.
    brls::Box* infoOverlay   = nullptr;
    brls::Label* infoLabel   = nullptr;  // left column
    brls::Label* infoLabel2  = nullptr;  // right column
    bool infoShown           = false;
    int64_t infoLastBytes    = 0;
    double infoSpeed         = 0.0;
    int64_t infoLastCache    = 0;    // cache bytes at the last sample (SD write rate)
    uint32_t infoLastLatN[5] = {};   // syscall counts at the last sample (rates)
    uint32_t statsLastLatN[5] = {};  // same, for the log's [stats] cadence

    // Observed mpv properties (async, updated in pumpEvents): the UI thread
    // must never call mpv_get_property in a per-frame path — a wedged mpv
    // core froze the render thread with it (and, through the engine getters'
    // lock, the whole download).
    double obsPos = 0.0, obsDur = 0.0, obsCacheSecs = 0.0;
    bool obsCacheIdle = false;
    bool obsPausedForCache = false;
    bool obsCoreIdle = false;
    std::chrono::steady_clock::time_point infoLastSample;

    // Periodic stats dump (see logStats).
    std::chrono::steady_clock::time_point statsLastSample;
    std::chrono::steady_clock::time_point statsStart;
    int64_t statsLastBytes = 0;
    bool statsStarted      = false;

    bool ready         = false;  // mpv has presented the first video frame
    bool fileLoaded    = false;  // mpv opened the stream (header downloaded)
    bool overlayHidden = false;  // loading screen has been taken down
    bool ended         = false;  // reached EOF; the auto-close is scheduled
    int shownPct       = -1;     // last buffering % pushed to the bar

    // Download-speed sampling.
    int64_t lastBytes = 0;
    double speedBps   = 0.0;
    std::chrono::steady_clock::time_point lastSample;
    std::chrono::steady_clock::time_point streamStartTime;

    // Stremio watch-state sync: the position is sampled alongside logStats and
    // pushed to the API every couple of minutes, plus once at teardown -- so
    // "continue watching" on other devices tracks what was watched here.
    void maybePushWatchState(bool force);
    WatchInfo watch;
    double lastPosSec = 0.0, lastDurSec = 0.0;
    std::chrono::steady_clock::time_point watchPushLast;
    bool watchPushValid = false;
};

// Full-screen activity hosting an MpvView. Pops (and tears mpv down) on B.
class PlayerActivity : public brls::Activity
{
  public:
    explicit PlayerActivity(std::string source, PlayerArt art = {},
                            std::string title = "", int fileIndex = -1,
                            WatchInfo watch = {});
    brls::View* createContentView() override;

    // Opaque: borealis stops the activity-stack draw here and does NOT composite
    // the browser behind, whose nanovg background/highlight would otherwise be
    // flushed over (and hide) the immediate mpv video render.
    bool isTranslucent() override { return false; }

  private:
    std::string source;
    PlayerArt art;
    std::string title;
    int fileIndex;
    WatchInfo watch;
};
