#include "player.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>


#include <switch.h>  // appletSetMediaPlaybackState (keep the screen awake)

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <borealis/views/dialog.hpp>
#include <borealis/views/header.hpp>
#include <borealis/views/cells/cell_selector.hpp>

#include <cctype>

#include "appdata.hpp"
#include "browse.hpp"  // openEpisodeById, for the next-episode card
#include "config.hpp"
#include "stremio.hpp"  // cached artwork -> blurred background
#include "theme.hpp"    // muted text for the in-menu hints

extern "C" {
#include "torrentfs.h"
#include "stream.h"
#include "torrent.h"  // magnet metadata-fetch progress counters
}

// See setMagnetResolvedHook (player.hpp): fired with the played magnet and its
// resolved name so the Local tab can save a name it did not have yet.
static std::function<void(const std::string&, const std::string&)>
    g_magnetResolvedHook;
void setMagnetResolvedHook(
    std::function<void(const std::string&, const std::string&)> hook)
{
    g_magnetResolvedHook = std::move(hook);
}

namespace
{
void* getProcAddress(void*, const char* name)
{
    glfwGetCurrentContext();
    return (void*)glfwGetProcAddress(name);
}

// Seconds of video mpv must buffer before playback starts (and the loading
// screen reaches 100% / hands over to the video). Bump this for smoother
// playback on scarce swarms at the cost of a longer wait.
constexpr double kBufferSecs = 15.0;

// The loudness boost (Options -> Boost quiet audio). Realtime normalisation to
// a consistent target: quiet 5.1 dialogue comes up without clipping the loud
// scenes, and a native stereo track is left roughly where it is.
constexpr const char* kAudioBoostFilter = "dynaudnorm=f=200:g=11:p=0.9:m=10";

// How long before the end the "next episode" card comes up.
constexpr double kNextCardSecs = 30.0;

// "tt1234567:1:4" -> "Season 1 \xC2\xB7 Episode 4", or "" when the id does not
// carry the pair (a film, or a catalog whose ids are shaped differently).
std::string episodeLabelOf(const std::string& videoId)
{
    size_t c1 = videoId.find(':');
    if (c1 == std::string::npos) return "";
    size_t c2 = videoId.find(':', c1 + 1);
    if (c2 == std::string::npos) return "";
    std::string s = videoId.substr(c1 + 1, c2 - c1 - 1);
    std::string e = videoId.substr(c2 + 1);
    if (s.empty() || e.empty()) return "";
    if (s.find_first_not_of("0123456789") != std::string::npos) return "";
    if (e.find_first_not_of("0123456789") != std::string::npos) return "";
    return "Season " + s + " \xC2\xB7 Episode " + e;
}

// The playback speeds offered, in order. L/R in the player step through this
// list and the panel's Speed row lists it -- one definition so the two cannot
// disagree. mpv resamples the audio to keep the pitch at any of them.
const std::vector<double> kSpeeds = { 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0 };
const std::vector<std::string> kSpeedLabels = { "0.5x",  "0.75x", "Normal",
                                                "1.25x", "1.5x",  "1.75x",
                                                "2x" };

// Whether the boost should be on right now. Handheld only: the built-in
// speakers are what it exists for -- docked, the TV or receiver has all the
// gain anyone needs and the compression would only flatten the mix.
//
// appletGetOperationMode() is a cached read of a variable libnx updates from
// the applet message loop borealis already pumps, not an IPC round trip, so it
// is safe on a per-frame path -- unlike the nifm call that used to freeze it.
bool audioBoostWanted()
{
    return config::get().audioBoost
           && appletGetOperationMode() == AppletOperationMode_Handheld;
}

// Seconds the cursor moves per D-pad press while scrubbing.
constexpr double kSeekStepSecs = 5.0;

// Analog scrubbing. Rate is (tilt^2) * base, so a light push creeps and a full
// push flies, then ramps by up to kSeekAccelMax the longer it's held -- an hour
// of video is unreachable at a fixed rate, but a fixed *fast* rate makes it
// impossible to land on a scene.
constexpr float kStickDeadzone   = 0.15f;
constexpr double kSeekBaseRate   = 30.0;  // seconds of video per second, full tilt
constexpr double kSeekAccelMax   = 12.0;  // held-down multiplier ceiling
constexpr double kSeekAccelSecs  = 4.0;   // seconds of holding to reach it

// Material Icons padlock glyphs (present in the bundled MaterialIcons-Regular.ttf,
// same font as the Options gear): the control-lock pill shows the open one as a
// hint / while unlocked, the closed one while locked.
constexpr const char* kGlyphLockOpen   = "\xEE\xA2\x98";  // U+E898 lock_open
constexpr const char* kGlyphLockClosed = "\xEE\xA2\x97";  // U+E897 lock

namespace
{
// The centre play/pause button, drawn directly with nanovg (two bars while
// playing, a triangle while paused) rather than a font glyph -- the Material
// play/pause codepoints rendered as the wrong/garbled glyph in a Label here.
// Reads the live pause flag through a pointer, so it always shows the right icon.
class PlayPauseButton : public brls::Box
{
  public:
    const bool* paused = nullptr;
    void draw(NVGcontext* vg, float x, float y, float w, float h,
              brls::Style style, brls::FrameContext* ctx) override
    {
        brls::Box::draw(vg, x, y, w, h, style, ctx);  // the circular background
        float cx = x + w / 2.0f, cy = y + h / 2.0f;
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 240));
        if (paused && *paused)
        {
            // Play: a right-pointing triangle.
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - 15.0f, cy - 27.0f);
            nvgLineTo(vg, cx + 28.0f, cy);
            nvgLineTo(vg, cx - 15.0f, cy + 27.0f);
            nvgClosePath(vg);
            nvgFill(vg);
        }
        else
        {
            // Pause: two rounded bars.
            const float bw = 13.0f, bh = 52.0f, gap = 16.0f;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx - gap / 2 - bw, cy - bh / 2, bw, bh, 3.0f);
            nvgRoundedRect(vg, cx + gap / 2, cy - bh / 2, bw, bh, 3.0f);
            nvgFill(vg);
        }
    }
};
} // namespace

// Seconds -> "M:SS" (or "H:MM:SS" past an hour).
std::string fmtTime(double s)
{
    if (s < 0 || s != s)  // clamp negatives / NaN
        s = 0;
    int t = (int)s, h = t / 3600, m = (t % 3600) / 60, sec = t % 60;
    char buf[16];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, sec);
    else
        std::snprintf(buf, sizeof(buf), "%d:%02d", m, sec);
    return buf;
}

// Pops `n` activities back-to-back. A free function, not a member: the first pop
// destroys the player view, so `this` cannot drive the rest. Each pop chains the
// next from its completion callback, so they run in order without animation.
void popActivities(int n)
{
    if (n <= 0) return;
    brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                   [n]() { popActivities(n - 1); });
}

// The same, with a final step. Each pop completes asynchronously, so whatever
// comes next has to hang off the last one's callback rather than run straight
// after the call.
void popActivitiesThen(int n, std::function<void()> then)
{
    if (n <= 0)
    {
        if (then) then();
        return;
    }
    brls::Application::popActivity(brls::TransitionAnimation::NONE, [n, then]() {
        popActivitiesThen(n - 1, then);
    });
}

} // namespace

MpvView::MpvView(const std::string& source, const PlayerArt& art,
                 const std::string& titleOverride, int fileIndex,
                 WatchInfo watchInfo)
    : art(art)
    , watch(std::move(watchInfo))
{
    // Tell the OS a video is playing, so it does not dim the screen or sleep
    // during a long film -- there is no controller input while watching, which
    // is exactly what the idle timer counts. Cleared in the destructor.
    appletSetMediaPlaybackState(true);

    this->setGrow(1.0f);
    this->setBackgroundColor(nvgRGB(0, 0, 0));
    // Take focus off the browser (so its selection highlight stops drawing over
    // the video) but show no highlight box on the video itself.
    this->setFocusable(true);
    this->setHideHighlight(true);

    // Build the UI BEFORE touching the engine. torrentfs_open used to run right
    // here and return early on failure, leaving a view with no children at all
    // -- a plain black screen with nothing to say what went wrong.
    {
        // A caller that knows the real name (Stremio) passes it in: a magnet
        // carries none until its metadata lands, so this is the only way to show
        // it during the wait.
        std::string title = titleOverride;
        if (title.empty()) title = source;
        if (!titleOverride.empty())
        {
            // already the display name
        }
        else if (title.rfind("magnet:", 0) == 0)
        {
            title = "Torrent";
        }
        else
        {
            size_t slash = title.find_last_of("/\\");
            if (slash != std::string::npos) title = title.substr(slash + 1);
            if (title.size() > 8 &&
                title.compare(title.size() - 8, 8, ".torrent") == 0)
                title = title.substr(0, title.size() - 8);
        }
        lastSample = std::chrono::steady_clock::now();
        // What the pause overlay shows: the caller's episode/film title when it
        // gave one, otherwise the name derived above.
        pauseTitle = watch.displayTitle.empty() ? title : watch.displayTitle;
        buildLoadingOverlay(title);
    }
    registerPlayerActions();

    startEngine(source, fileIndex);
}

// Opens the torrent off the UI thread. For a magnet this announces to trackers
// and pulls the metadata from peers (BEP 9) -- seconds of blocking work. Doing
// it inline froze the whole app on a black screen until it finished or failed.
void MpvView::startEngine(const std::string& source, int fileIndex)
{
    if (statusLabel)
        statusLabel->setText(source.rfind("magnet:", 0) == 0
                                 ? "Fetching metadata..."
                                 : "Opening torrent...");

    auto liveFlag = this->alive;
    brls::async([this, liveFlag, source, fileIndex]() {
        char err[256] = { 0 };
        // Latched into the engine at open; pick it up from config for this video.
        torrentfs_set_ram_stream(config::get().ramStream ? 1 : 0);
        torrentfs* t = torrentfs_open_file(source.c_str(), APPDATA_CACHE,
                                           fileIndex, err, sizeof(err));
        std::string e = err;

        brls::sync([this, liveFlag, t, e, source]() {
            // B may have been pressed while we waited: `this` is gone and the
            // engine we just opened would leak.
            if (!*liveFlag)
            {
                if (t) torrentfs_close(t);
                return;
            }
            if (!t)
            {
                brls::Logger::error("torrentfs_open failed: {}", e);
                if (statusLabel) statusLabel->setText("Failed: " + e);
                return;
            }
            tfs = t;
            // A magnet from the Local list may have had no name -- now that its
            // metadata resolved, hand the real name back so it can be saved.
            if (g_magnetResolvedHook && source.compare(0, 7, "magnet:") == 0)
                g_magnetResolvedHook(source, torrentfs_name(t));
            if (!startMpv() && statusLabel)
                statusLabel->setText("Player initialisation failed");
        });
    });
}

// Brings mpv up against the now-open engine. False if anything failed.
bool MpvView::startMpv()
{
    mpv = mpv_create();
    if (!mpv)
    {
        brls::Logger::error("mpv_create failed");
        return false;
    }

    mpv_set_option_string(mpv, "vo", "libmpv");
    // Hardware decode by default (Options -> Hardware decoding). Software decode
    // ("no") sidesteps the nvtegra/GPU path -- a way to test whether the random
    // freezes come from it.
    bool hwDec = config::get().hwDecode;
    mpv_set_option_string(mpv, "hwdec", hwDec ? "auto" : "no");
    if (hwDec)
        mpv_set_option_string(mpv, "hwdec-extra-frames", "32");
    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "terminal", "no");
    // Switch audio output is stereo; force a proper downmix or 5.1 dialogue
    // (centre channel) is dropped.
    mpv_set_option_string(mpv, "audio-channels", "stereo");
    // Do NOT normalize the downmix: "yes" scales the 5.1->stereo matrix down to
    // avoid clipping, which was one reason 5.1 tracks played quiet. "no" keeps the
    // standard downmix levels so the centre (dialogue) folds in at full strength.
    mpv_set_option_string(mpv, "audio-normalize-downmix", "no");
    // Even so a 5.1 master downmixed to stereo sits well below a native stereo
    // track on the Switch's own speakers, which is what the boost is for (see
    // audioBoostWanted -- handheld only, and switchable in Options). Kept in
    // sync with the dock state from pumpEvents afterwards.
    boostApplied = audioBoostWanted();
    mpv_set_option_string(mpv, "af", boostApplied ? kAudioBoostFilter : "");

    // Preferred track languages (Options). mpv falls back to the file's default
    // track when nothing matches, so a wrong guess costs nothing.
    std::string alang = config::mpvLangList(config::get().audioLang);
    if (!alang.empty()) mpv_set_option_string(mpv, "alang", alang.c_str());
    std::string slang = config::mpvLangList(config::get().subLang);
    if (!slang.empty()) mpv_set_option_string(mpv, "slang", slang.c_str());
    // "no" means: load no subtitle track at all, rather than pick one and hide
    // it -- which also saves decoding them.
    mpv_set_option_string(mpv, "sid", config::get().subtitles ? "auto" : "no");

    // Subtitle look. mpv's defaults are a 55pt face with a hard 3.0 black border,
    // no shadow and no blur -- heavy, and the outline reads as a sticker cut out
    // of the picture. This is the streaming-service treatment instead: a slightly
    // smaller semibold face, a thinner outline softened by a touch of blur, and a
    // real drop shadow to lift it off bright scenes.
    //
    // NOT setting sub-font: this toolchain's libass (0.17.3) is built without
    // fontconfig, so there is no font provider to resolve a family name against.
    // Naming one risks losing subtitle rendering altogether, which is far worse
    // than a face we did not choose.
    //
    // Colours are opaque on purpose. mpv takes #AARRGGBB, but ASS stores alpha
    // inverted and it is not worth betting the look on which convention wins
    // here -- the softness comes from blur and shadow offset, not from alpha.
    mpv_set_option_string(mpv, "sub-font-size", "46");
    mpv_set_option_string(mpv, "sub-bold", "yes");
    mpv_set_option_string(mpv, "sub-color", "#FFFFFF");
    mpv_set_option_string(mpv, "sub-border-color", "#000000");
    mpv_set_option_string(mpv, "sub-border-size", "2.6");
    mpv_set_option_string(mpv, "sub-shadow-color", "#000000");
    mpv_set_option_string(mpv, "sub-shadow-offset", "1.4");
    mpv_set_option_string(mpv, "sub-blur", "0.35");
    // A little tracking: bold sans at this size sets too tight otherwise.
    mpv_set_option_string(mpv, "sub-spacing", "0.4");
    // Off the very bottom edge (mpv default 34), clear of the seek bar.
    mpv_set_option_string(mpv, "sub-margin-y", "50");
    // The one debatable line: without it none of the above reaches ASS/SSA subs,
    // which keep whatever the release baked in. "yes" restyles them for a
    // consistent look but flattens hand-styled signs (typical in anime rips);
    // "scale" -- mpv's default -- would leave those alone. Border names are the
    // 0.37 spelling; they became sub-outline-* in 0.38.
    mpv_set_option_string(mpv, "sub-ass-override", "yes");
    // Subtitles pulled from an addon are frequently CP1252, not UTF-8 (that is
    // what OpenSubtitles has on file for most European languages), and this
    // toolchain's mpv is built without uchardet -- so "auto" cannot detect
    // anything and every accented character came out mangled. Naming a codepage
    // without a "+" means "use it only if the text is not valid UTF-8", so a
    // UTF-8 file is still read as UTF-8. CP1252 is the right guess for the
    // languages this app offers; a Cyrillic or Greek subtitle would still need
    // its own, which is a setting nobody has asked for yet.
    mpv_set_option_string(mpv, "sub-codepage", "cp1252");
    mpv_set_option_string(mpv, "cache", "yes");
    // Never let mpv auto-pause playback to rebuffer -- once we start, we keep
    // playing. We do the initial buffering ourselves: start paused, fill the
    // demuxer cache, then unpause on reveal (see updateLoadingOverlay/draw).
    mpv_set_option_string(mpv, "cache-pause", "no");
    mpv_set_option_string(mpv, "pause", "yes");
    // Give the demuxer enough headroom to buffer kBufferSecs before we unpause.
    mpv_set_option_string(mpv, "demuxer-max-bytes", "128MiB");
    mpv_set_option_string(mpv, "demuxer-readahead-secs", "30");
    // With cache=yes, mpv's cache-secs (default 10) *overrides*
    // demuxer-readahead-secs, so the demuxer stopped at exactly 10 s of
    // readahead. kBufferSecs is 15, so "buffered enough" could never become
    // true and the loading screen hung forever no matter how much was
    // downloaded. Must stay comfortably above kBufferSecs.
    mpv_set_option_string(mpv, "cache-secs", "60");
    // On Switch the hardware decoder's GPU work is async; without a glFinish
    // after mpv's render, glfwSwapBuffers can present before the video is drawn
    // (black frame). This is what the other Switch mpv players set too.
    mpv_set_option_string(mpv, "opengl-glfinish", "yes");
    mpv_set_option_string(mpv, "vd-lavc-dr", "yes");

    if (mpv_initialize(mpv) < 0)
    {
        brls::Logger::error("mpv_initialize failed");
        return false;
    }

    mpv_opengl_init_params glInit{ getProcAddress, nullptr };
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit },
        { MPV_RENDER_PARAM_INVALID, nullptr },
    };
    if (mpv_render_context_create(&renderCtx, mpv, params) < 0)
    {
        brls::Logger::error("mpv_render_context_create failed");
        renderCtx = nullptr;
        return false;
    }
    brls::Logger::info("mpv render context created OK");

    // Capture mpv's internal log so we can see vo/render/hwdec state.
    mpv_request_log_messages(mpv, "v");

    // Resume where the Stremio account left off: mpv seeks there right after
    // the open, and the engine's playhead (stream.c seek_cb) follows, so the
    // download window re-centres on the resume point instead of the start.
    if (watch.resumeSec > 1.0)
    {
        char st[32];
        std::snprintf(st, sizeof(st), "%.1f", watch.resumeSec);
        mpv_set_option_string(mpv, "start", st);
        brls::Logger::info("[player] resuming at {}s (Stremio watch state)",
                           (int)watch.resumeSec);
    }

    // Buffered-seconds feed for the engine's calm mode, as an async observe:
    // a synchronous mpv_get_property from draw() waits on the mpv core, and
    // when the core is wedged behind a blocking stream read (playhead piece
    // not downloaded yet) that froze the RENDER thread with it -- the "small"
    // lag spikes. Property-change events arrive through pumpEvents instead.
    mpv_observe_property(mpv, 0, "demuxer-cache-duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "demuxer-cache-idle", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);

    // Wire our torrent:// stream (source/stream.c) and start playback.
    stream_register(mpv, tfs);
    const char* cmd[] = { "loadfile", "torrent://stream", nullptr };
    mpv_command(mpv, cmd);
    brls::Logger::info("loadfile torrent://stream issued");
    return true;
}

// Controls. Registered from the constructor, not from startMpv: the engine may
// still be resolving a magnet (or may have failed outright), and B has to work
// the whole time -- that is the only way out of a stuck load.
void MpvView::registerPlayerActions()
{
    // Y locks / unlocks every other control for the session (see toggleLock).
    // Registered first, and never gated, so it is always the way out of a lock.
    this->registerAction(
        "Lock", brls::BUTTON_Y,
        [this](brls::View*) {
            toggleLock();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    // B returns to the browser.
    this->registerAction(
        "Back", brls::BUTTON_B,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            // Scrubbing: B cancels the seek and returns to where playback was,
            // instead of leaving the stream.
            if (seeking) { cancelSeek(); return true; }
            brls::Logger::info("[teardown] B pressed -> popActivity()");
            bool ok = brls::Application::popActivity();
            brls::Logger::info("[teardown] popActivity() returned {}", ok);
            return true;
        },
        false, false, brls::SOUND_BACK);

    // A toggles pause during playback (ignored while still buffering). Tapping the
    // video does the same (see the gesture in buildLoadingOverlay).
    this->registerAction(
        "Pause", brls::BUTTON_A,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            // While the next-episode card is up and the video is running, A
            // takes it -- that is what the card is asking for and what every
            // other player does. Paused, A still resumes: the card stays put
            // and the tap on it is the way through.
            if (nextCardShown && !userPaused && !seeking)
            {
                goToNextEpisode();
                return true;
            }
            onPlayPause();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    // Left/right scrub. The first nudge pauses and pins the cursor to the
    // current position; further nudges move it. allowRepeating so holding the
    // stick scrubs continuously. A commits (see the Pause action), B leaves.
    auto scrub = [this](double delta) {
        if (controlsLocked) { flashLock(); return true; }
        if (!ready || !mpv)
            return true;
        if (!seeking)
            beginSeek();
        seekTarget += delta;
        if (seekTarget < 0.0) seekTarget = 0.0;
        if (seekDur > 0.0 && seekTarget > seekDur) seekTarget = seekDur;
        return true;
    };

    this->registerAction(
        "Seek -", brls::BUTTON_LEFT,
        [scrub](brls::View*) { return scrub(-kSeekStepSecs); },
        false, true, brls::SOUND_NONE);
    this->registerAction(
        "Seek +", brls::BUTTON_RIGHT,
        [scrub](brls::View*) { return scrub(kSeekStepSecs); },
        false, true, brls::SOUND_NONE);

    // L / R step the playback speed. These actions only fire while the player
    // itself has the cursor -- with the settings panel up, focus is inside it
    // and the same two buttons are the subtitle delay there (see openTrackMenu).
    auto speedStep = [this](int dir) {
        if (controlsLocked) { flashLock(); return true; }
        if (!ready || !mpv) return true;
        nudgeSpeed(dir);
        return true;
    };
    this->registerAction(
        "Slower", brls::BUTTON_LB,
        [speedStep](brls::View*) { return speedStep(-1); },
        false, false, brls::SOUND_NONE);
    this->registerAction(
        "Faster", brls::BUTTON_RB,
        [speedStep](brls::View*) { return speedStep(1); },
        false, false, brls::SOUND_NONE);

    // ZR toggles the network/torrent info panel.
    this->registerAction(
        "Info", brls::BUTTON_RT,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            infoShown = !infoShown;
            brls::Logger::info("[player] ZR info toggle -> {}", infoShown);
            if (infoOverlay)
                infoOverlay->setVisibility(infoShown
                                               ? brls::Visibility::VISIBLE
                                               : brls::Visibility::GONE);
            return true;
        },
        false, false, brls::SOUND_NONE);

    // X opens the audio/subtitle picker for the current video.
    this->registerAction(
        "Options", brls::BUTTON_X,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            openTrackMenu();
            return true;
        },
        false, false, brls::SOUND_CLICK);
}

void MpvView::setControlsVisible(bool show)
{
    controlsShown = show;
    brls::Visibility v = show ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (pauseOverlay) pauseOverlay->setVisibility(v);
    if (pauseTitleBox) pauseTitleBox->setVisibility(v);
    if (optionsHint) optionsHint->setVisibility(v);
    if (seekOverlay) seekOverlay->setVisibility(v);
    // The centre button draws play vs pause from the live flag itself, so nothing
    // to update here.
    // Arm the play-only auto-hide each time the overlay is (re)shown.
    if (show)
        controlsHideAt =
            std::chrono::steady_clock::now() + std::chrono::seconds(4);
    // The Y-lock pill rides with the pause overlay as a "Y to lock" hint (open
    // padlock). flashLock is what flips it to the closed padlock while locked.
    if (lockHint)
    {
        lockHint->setVisibility(v);
        if (lockLabel) lockLabel->setText(kGlyphLockOpen);
    }
}

void MpvView::toggleLock()
{
    controlsLocked = !controlsLocked;
    brls::Logger::info("[player] controls {}",
                       controlsLocked ? "LOCKED" : "unlocked");
    if (controlsLocked)
    {
        // Locking: leave any scrub/pause state and hide the overlay so the video
        // runs undisturbed. A blocked input then only flashes the lock pill.
        seeking = false;
        if (userPaused && mpv)
        {
            mpv_set_property_string(mpv, "pause", "no");
            userPaused = false;
        }
        setControlsVisible(false);
    }
    flashLock();  // confirm the new state ("Locked" / "Unlocked")
}

// Show the lock pill for a moment: on a blocked input while locked, and to
// confirm a toggle. updateLockHint hides it again.
void MpvView::flashLock()
{
    if (!lockHint) return;
    if (lockLabel)
        lockLabel->setText(controlsLocked ? kGlyphLockClosed : kGlyphLockOpen);
    lockHint->setVisibility(brls::Visibility::VISIBLE);
    lockFlashActive = true;
    lockFlashUntil =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(1400);
}

void MpvView::updateControlsAutoHide()
{
    // Only while actually playing (paused keeps the controls up), and not mid-
    // scrub. A tap that re-shows the overlay pushes controlsHideAt out again.
    if (!controlsShown || userPaused || seeking)
        return;
    if (std::chrono::steady_clock::now() >= controlsHideAt)
        setControlsVisible(false);
}

void MpvView::updateLockHint()
{
    if (!lockFlashActive) return;
    if (std::chrono::steady_clock::now() < lockFlashUntil) return;
    lockFlashActive = false;
    // Flash over. Hide the pill unless the controls overlay is still up and using
    // it as the "Y to lock" hint, in which case just restore that label.
    //
    // The test is controlsShown, NOT userPaused: the overlay also shows during
    // playback (it auto-hides after 4s), and testing userPaused made the pill
    // vanish out from under a visible overlay. That was the touch bug -- unlock by
    // tapping the pill (which arms a flash), tap the video straight after to bring
    // the whole overlay up, and ~1.4s later this hid the pill while the rest of
    // the overlay stayed.
    if (controlsShown && !controlsLocked)
    {
        if (lockLabel) lockLabel->setText(kGlyphLockOpen);
    }
    else if (lockHint)
    {
        lockHint->setVisibility(brls::Visibility::GONE);
    }
}

void MpvView::onPlayPause()
{
    if (!ready || !mpv)
        return;

    // Commit a scrub: jump there and resume. mpv's seek reaches our stream's
    // seek_cb, which moves the torrent playhead, so the download window follows.
    if (seeking)
    {
        seeking = false;
        char t[32];
        std::snprintf(t, sizeof(t), "%.3f", seekTarget);
        const char* cmd[] = { "seek", t, "absolute", nullptr };
        mpv_command_async(mpv, 0, cmd);
        userPaused = false;
        mpv_set_property_string(mpv, "pause", "no");
        setControlsVisible(false);
        return;
    }

    userPaused = !userPaused;
    mpv_set_property_string(mpv, "pause", userPaused ? "yes" : "no");
    setControlsVisible(userPaused);
}

void MpvView::cancelSeek()
{
    if (!seeking)
        return;
    seeking = false;
    // No seek was committed while scrubbing, so mpv is still paused on the frame
    // it started from. Restore what the user had -- playing, or paused with the
    // overlay -- and take the seek bar down.
    if (mpv)
        mpv_set_property_string(mpv, "pause", userPaused ? "yes" : "no");
    setControlsVisible(userPaused);
}

void MpvView::seekToFraction(double frac)
{
    if (!ready || !mpv || obsDur <= 0.0)
        return;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    double target = frac * obsDur;
    char t[32];
    std::snprintf(t, sizeof(t), "%.3f", target);
    const char* cmd[] = { "seek", t, "absolute", nullptr };
    mpv_command_async(mpv, 0, cmd);
    obsPos     = target;   // reflect on the bar now (obsPos is async otherwise)
    seekTarget = target;
    // Lifting the finger off a tap/drag resumes playback and takes the bar down.
    seeking    = false;
    userPaused = false;
    mpv_set_property_string(mpv, "pause", "no");
    setControlsVisible(false);
}

namespace
{
// A SelectorCell whose value (the track name) truncates to the cell width instead
// of wrapping and overflowing the dialog, and marquee-scrolls while the cell is
// focused -- same idea as the source cards' lines.
class TrackCell : public brls::SelectorCell
{
  public:
    TrackCell()
    {
        // The value label ships with shrink=0, so a long track name forced its
        // full content width -- overflowing the 720-wide dialog and spilling left
        // over the title. Let the VALUE shrink (and truncate) while the TITLE
        // holds its width, so the row fits and onLayout clips the value with an
        // ellipsis. It marquees on focus (below). The strings are also clamped in
        // openTrackMenu as a floor.
        title->setShrink(0.0f);
        detail->setSingleLine(true);
        detail->setShrink(1.0f);
        detail->setMaxWidth(500.0f);
        // Left-align is REQUIRED for the marquee: Label::setAnimated is a no-op
        // unless the alignment is LEFT. It also gives the natural "start...end"
        // truncation. (The value was right-aligned by default.)
        detail->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    }
    void onFocusGained() override
    {
        brls::SelectorCell::onFocusGained();
        detail->setAnimated(true);
    }
    void onFocusLost() override
    {
        brls::SelectorCell::onFocusLost();
        detail->setAnimated(false);
    }
};

// The settings panel's own activity. Translucent, so borealis keeps drawing the
// player underneath -- which is the whole point of a side panel: the picture
// stays on screen while you change what is drawn over it. (PlayerActivity is
// opaque, so the stack drawn is exactly those two; the panel's nanovg calls
// flush after MpvView::draw has rendered mpv, so they land on top of the video
// rather than under it.)
class PlayerSettingsActivity : public brls::Activity
{
  public:
    PlayerSettingsActivity(brls::View* content, std::function<void()> onGone)
        : content(content), onGone(std::move(onGone))
    {
    }
    ~PlayerSettingsActivity() override
    {
        if (onGone) onGone();
    }
    bool isTranslucent() override { return true; }
    brls::View* createContentView() override { return content; }

  private:
    brls::View* content;
    std::function<void()> onGone;
};

// How wide the panel is, in the logical space borealis lays out in. Wide enough
// for "Subtitles" and a track name beside it, narrow enough to leave most of
// the frame showing.
constexpr float kPanelW = 560.0f;

// The subtitle delay as it is shown, everywhere it is shown. Two decimals, so a
// 0.1 step off a preset reads as "+0.10 s" rather than being rounded away into
// the nearest preset -- which is what the old dropdown did.
std::string subDelayText(double d)
{
    if (d == 0.0) return "None";
    char b[32];
    std::snprintf(b, sizeof(b), "%+.2f s", d);
    return b;
}

// A section heading inside the panel: the label in the accent, over a hairline.
// brls::Header would do, but it is sized for a full-width settings list and its
// rule runs the whole way across -- too heavy repeated three times in 560px.
brls::Box* panelSection(const char* title, float marginTop)
{
    auto* box = new brls::Box();
    box->setAxis(brls::Axis::COLUMN);
    box->setMargins(marginTop, 0.0f, 6.0f, 0.0f);

    auto* l = new brls::Label();
    l->setText(title);
    l->setFontSize(17.0f);
    l->setTextColor(theme::accent());
    l->setMarginBottom(6.0f);
    box->addView(l);

    auto* rule = new brls::Box();
    rule->setHeight(1.0f);
    rule->setBackgroundColor(theme::scrim(38));
    box->addView(rule);
    return box;
}
} // namespace

// Kicks off the one subtitle-addon lookup this playback makes. Safe to call
// more than once: only the first attempt goes anywhere.
void MpvView::fetchOnlineSubs()
{
    if (onlineSubState != SubFetch::Idle) return;
    // A local .torrent has no Stremio identity, so there is nothing to ask
    // about. Left Idle: the menu tells the user why the row is empty.
    if (watch.authKey.empty() || watch.videoId.empty()) return;

    onlineSubState = SubFetch::Busy;
    // "movie" unless we were told otherwise -- a WatchInfo built before this
    // field existed, or any path that forgot it, still gets a useful answer for
    // a film, and an episode id would not resolve as a movie anyway.
    std::string type = watch.type.empty() ? "movie" : watch.type;

    auto live = this->alive;
    stremio::fetchSubtitlesAsync(
        watch.authKey, type, watch.videoId,
        [this, live](stremio::SubtitlesResult r) {
            // The player can be gone: B during the buffering wait tears the
            // view down while this is still in flight.
            if (!*live) return;
            onlineSubs     = r.subs;
            onlineSubErr   = r.error;
            onlineSubState = r.ok ? SubFetch::Done : SubFetch::Failed;
            brls::Logger::info("[player] addon subtitles: {} ({})",
                               onlineSubs.size(),
                               r.ok ? "ok" : onlineSubErr.c_str());
        });
}

// Downloads onlineSubs[index] and hands the file to mpv, selected.
void MpvView::loadOnlineSub(int index)
{
    if (index < 0 || index >= (int)onlineSubs.size() || !mpv) return;

    const stremio::Subtitle& sub = onlineSubs[index];
    std::string title = config::langLabelFor(sub.lang);
    if (title.empty()) title = "Subtitle";
    if (!sub.addon.empty()) title += " - " + sub.addon;

    brls::Application::notify("Downloading " + title + "...");

    auto live = this->alive;
    stremio::downloadSubtitleAsync(
        sub, [this, live, index, title, lang = sub.lang](std::string path) {
            if (!*live) return;
            if (path.empty() || !mpv)
            {
                brls::Application::notify("Subtitle download failed");
                return;
            }
            // "cached" rather than "select": picking the same subtitle twice
            // then re-selects the track already loaded instead of adding a
            // duplicate. The title and language are what the Subtitles row
            // will show for it from here on.
            const char* cmd[] = { "sub-add",      path.c_str(), "cached",
                                  title.c_str(),  lang.c_str(), nullptr };
            if (mpv_command(mpv, cmd) < 0)
            {
                brls::Application::notify("Subtitle could not be loaded");
                return;
            }
            loadedSubs.insert(index);
            brls::Application::notify(title + " loaded");
            brls::Logger::info("[player] loaded subtitle {}", path);
        });
}

// Clamps, pushes to mpv and shows the readout pill. The single point where the
// delay changes -- the menu and the L/R shortcut both come through here.
void MpvView::setSubDelay(double seconds)
{
    // Past ten seconds either way it is not a sync problem any more, it is the
    // wrong subtitle file.
    constexpr double kMax = 10.0;
    if (seconds < -kMax) seconds = -kMax;
    if (seconds > kMax) seconds = kMax;
    // Snap: the shortcut steps by 0.1 and floating point drift would otherwise
    // show up in the readout.
    subDelay = std::round(seconds * 100.0) / 100.0;

    if (mpv)
    {
        char v[24];
        std::snprintf(v, sizeof(v), "%.2f", subDelay);
        mpv_set_property_string(mpv, "sub-delay", v);
    }

    // The panel's own row while it is open -- it shows the exact figure, which
    // is the whole point of stepping by 0.1 -- and the pill otherwise.
    if (subDelaySink)
        subDelaySink(subDelay);
    else
        flashPill("Subtitles  " + subDelayText(subDelay));
}

// Steps through kSpeeds. Clamped at both ends rather than wrapping: running
// into 2x and coming out at 0.5x is never what a press meant.
void MpvView::nudgeSpeed(int dir)
{
    if (!mpv) return;
    int cur   = 0;
    double bd = 1e18;
    for (size_t i = 0; i < kSpeeds.size(); i++)
    {
        double d = playSpeed > kSpeeds[i] ? playSpeed - kSpeeds[i]
                                          : kSpeeds[i] - playSpeed;
        if (d < bd) { bd = d; cur = (int)i; }
    }
    int next = cur + dir;
    if (next < 0 || next >= (int)kSpeeds.size()) return;

    playSpeed = kSpeeds[(size_t)next];
    char v[24];
    std::snprintf(v, sizeof(v), "%.4g", playSpeed);
    mpv_set_property_string(mpv, "speed", v);
    flashPill("Speed  " + kSpeedLabels[(size_t)next]);
}

void MpvView::flashPill(const std::string& text)
{
    if (!hintPill || !pillLabel) return;
    pillLabel->setText(text);
    hintPill->setVisibility(brls::Visibility::VISIBLE);
    pillFlashActive = true;
    pillFlashUntil =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(1600);
}

// Shows the card over the last kNextCardSecs, and only when there is somewhere
// to go. obsPos/obsDur are the asynchronously observed properties -- this runs
// every frame, and mpv_get_property on this path is what once froze the render
// thread.
void MpvView::updateNextCard()
{
    if (!nextCard) return;

    double left = obsDur - obsPos;
    bool want   = ready && !ended && !settingsOpen && !watch.nextVideoId.empty()
                && obsDur > 0.0 && left > 0.0 && left <= kNextCardSecs;
    if (want == nextCardShown) return;

    nextCardShown = want;
    if (want && nextCardSub)
    {
        std::string sub = episodeLabelOf(watch.nextVideoId);
        nextCardSub->setText(sub.empty() ? "Up next" : sub);
    }
    nextCard->setVisibility(want ? brls::Visibility::VISIBLE
                                 : brls::Visibility::GONE);
}

// Leaves the player and opens the next episode's sources. Everything it needs
// is copied out first: popping frees this view long before the push happens.
void MpvView::goToNextEpisode()
{
    if (ended || watch.nextVideoId.empty()) return;
    // Also stops the EOF handler from popping underneath us if the file runs
    // out while the pops are in flight.
    ended = true;
    nextCardShown = false;
    if (nextCard) nextCard->setVisibility(brls::Visibility::GONE);

    std::string authKey = watch.authKey;
    std::string series  = watch.itemId;
    std::string next    = watch.nextVideoId;
    PlayerArt showArt   = art;
    int pops            = watch.endPop;

    brls::Logger::info("[player] next episode -> {} (pop {})", next, pops);
    // Deferred: we are inside draw()'s update pass (or a tap handler), and
    // popping -- which frees this view -- mid-draw is not safe.
    brls::sync([pops, authKey, series, next, showArt]() {
        popActivitiesThen(pops, [authKey, series, next, showArt]() {
            openEpisodeById(authKey, series, next, showArt);
        });
    });
}

void MpvView::updatePill()
{
    if (!pillFlashActive) return;
    if (std::chrono::steady_clock::now() < pillFlashUntil) return;
    pillFlashActive = false;
    if (hintPill) hintPill->setVisibility(brls::Visibility::GONE);
}

void MpvView::openTrackMenu()
{
    if (!ready || !mpv)
        return;

    // Playback is deliberately NOT paused here any more. The panel leaves the
    // picture on screen, and every control in it -- the subtitle timing above
    // all -- can only be judged against moving video. The controls overlay does
    // come down, though: a play button, a title and a seek bar beside the panel
    // are clutter over the very frame it exists to let you watch.
    setControlsVisible(false);
    settingsOpen = true;

    // Human label for a track: uppercased language, then its title, else "Track N".
    auto label = [](const std::string& lang, const std::string& title,
                    int64_t id) {
        std::string s = lang;
        for (auto& c : s) c = (char)std::toupper((unsigned char)c);
        if (!title.empty()) s += (s.empty() ? "" : " - ") + title;
        if (s.empty()) s = "Track " + std::to_string(id);
        return s;  // full name kept: the cell truncates/marquees, the dropdown shows it whole
    };

    // Walk mpv's track-list once, splitting audio and subtitle tracks and noting
    // which is selected. Subtitles get an "Off" entry so the one selector both
    // switches and disables them.
    std::vector<std::string> aLabels, sLabels{ "Off" };
    std::vector<int64_t> aIds, sIds{ -1 };
    int aCur = 0, sCur = 0;  // sCur 0 = Off

    mpv_node node;
    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) >= 0)
    {
        if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list)
            for (int i = 0; i < node.u.list->num; i++)
            {
                mpv_node& tn = node.u.list->values[i];
                if (tn.format != MPV_FORMAT_NODE_MAP || !tn.u.list) continue;

                std::string type, lang, title;
                int64_t id = 0;
                bool sel = false;
                for (int k = 0; k < tn.u.list->num; k++)
                {
                    const char* key = tn.u.list->keys[k];
                    mpv_node& v     = tn.u.list->values[k];
                    if (!std::strcmp(key, "type") && v.format == MPV_FORMAT_STRING)
                        type = v.u.string;
                    else if (!std::strcmp(key, "id") && v.format == MPV_FORMAT_INT64)
                        id = v.u.int64;
                    else if (!std::strcmp(key, "lang") && v.format == MPV_FORMAT_STRING)
                        lang = v.u.string;
                    else if (!std::strcmp(key, "title") && v.format == MPV_FORMAT_STRING)
                        title = v.u.string;
                    else if (!std::strcmp(key, "selected") && v.format == MPV_FORMAT_FLAG)
                        sel = v.u.flag;
                }

                if (type == "audio")
                {
                    if (sel) aCur = (int)aIds.size();
                    aLabels.push_back(label(lang, title, id));
                    aIds.push_back(id);
                }
                else if (type == "sub")
                {
                    if (sel) sCur = (int)sIds.size();
                    sLabels.push_back(label(lang, title, id));
                    sIds.push_back(id);
                }
            }
        mpv_free_node_contents(&node);
    }

    // ---- the panel ------------------------------------------------------
    // A right-hand panel rather than the centred dialog this used to be. Every
    // control here changes something you can only judge by looking at the
    // picture -- a subtitle's size, its timing, which track is playing -- and a
    // box in the middle of the screen covers exactly what you need to see.
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::ROW);
    root->setJustifyContent(brls::JustifyContent::FLEX_END);
    root->setGrow(1.0f);

    // The uncovered part of the frame. Not a scrim -- it is left clear on
    // purpose -- but it is what a tap outside the panel lands on.
    auto* rest = new brls::Box();
    rest->setGrow(1.0f);
    rest->setHeightPercentage(100.0f);
    rest->addGestureRecognizer(new brls::TapGestureRecognizer(
        rest, []() { brls::Application::popActivity(); }));
    root->addView(rest);

    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setWidth(kPanelW);
    panel->setHeightPercentage(100.0f);
    panel->setPadding(40.0f, 32.0f, 24.0f, 32.0f);
    // Near-opaque: this sits over moving video, and anything lighter makes
    // every label fight the frame behind it.
    panel->setBackgroundColor(theme::isLight() ? nvgRGBA(246, 246, 250, 242)
                                               : nvgRGBA(18, 18, 22, 238));
    root->addView(panel);

    // B closes it. Registered on the panel, and actions bubble from the focused
    // cell up the parents, so it works wherever the cursor is inside.
    panel->registerAction(
        "Close", brls::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity();
            return true;
        },
        false, false, brls::SOUND_BACK);

    auto* title = new brls::Label();
    title->setText("Playback");
    title->setFontSize(28.0f);
    title->setTextColor(theme::text());
    title->setMarginBottom(4.0f);
    panel->addView(title);

    // Everything below scrolls: the subtitle section grows by three rows the
    // moment a file has subtitles, and at the 100% UI size that is already most
    // of the panel's height.
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    // CENTERED, like every other list in the app: one cell per press.
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    scroll->setContentView(content);
    panel->addView(scroll);

    // The index whose value sits closest to `x`: these are doubles read back from
    // mpv, so matching them by equality would fail on the first rounding.
    auto nearest = [](const std::vector<double>& v, double x) {
        int best   = 0;
        double bd  = 1e18;
        for (size_t i = 0; i < v.size(); i++)
        {
            double d = x > v[i] ? x - v[i] : v[i] - x;
            if (d < bd) { bd = d; best = (int)i; }
        }
        return best;
    };
    // Reads a double property, falling back to 1.0 -- the neutral value for both
    // of the ones below.
    auto getScale = [this](const char* prop) {
        double d = 1.0;
        mpv_get_property(mpv, prop, MPV_FORMAT_DOUBLE, &d);
        return d;
    };
    auto setScale = [this](const char* prop, double value) {
        char v[24];
        std::snprintf(v, sizeof(v), "%.4g", value);
        mpv_set_property_string(mpv, prop, v);
    };

    content->addView(panelSection("AUDIO", 8.0f));

    // Always show the track, even when there is only one -- it tells you what
    // is playing (language/title).
    if (!aLabels.empty())
    {
        auto* a = new TrackCell();
        a->init("Track", aLabels, aCur, [this, aIds](int sel) {
            char v[24];
            std::snprintf(v, sizeof(v), "%lld", (long long)aIds[sel]);
            mpv_set_property_string(mpv, "aid", v);
        });
        content->addView(a);
    }

    // Playback speed. Also on L/R in the player itself (see nudgeSpeed), which
    // steps through this same list -- hence the shared kSpeeds.
    {
        playSpeed = getScale("speed");
        auto* sp  = new brls::SelectorCell();
        sp->init("Speed", kSpeedLabels, nearest(kSpeeds, playSpeed),
                 [this, setScale](int sel) {
                     playSpeed = kSpeeds[(size_t)sel];
                     setScale("speed", playSpeed);
                 });
        content->addView(sp);
    }

    content->addView(panelSection("SUBTITLES", 18.0f));

    // ---- subtitles: one list, wherever they come from --------------------
    // The tracks muxed into the file and the ones the account's Stremio
    // subtitle addons offer are the same choice to whoever is watching, so they
    // are one selector. What differs is only what picking costs: an mpv track
    // switches instantly, an addon entry is fetched first (loadOnlineSub) and
    // then joins the track list -- which is why a loaded one is skipped in the
    // addon section below rather than being listed twice.
    {
        // Whether each entry is an mpv track or an addon subtitle to fetch.
        // sub >= 0 indexes onlineSubs; otherwise sid is the mpv track, -1 for
        // "Off" and -2 for the inert status entry at the end.
        struct Pick
        {
            int64_t sid = -1;
            int sub     = -1;
        };
        constexpr int64_t kNoteSid = -2;
        std::vector<Pick> picks;
        std::vector<std::string> labels;
        int cur = 0;

        for (size_t i = 0; i < sIds.size(); i++)
        {
            picks.push_back({ sIds[i], -1 });
            labels.push_back(sLabels[i]);
            if ((int)i == sCur) cur = (int)labels.size() - 1;
        }

        // The addon ones we have not pulled yet. A loaded one is already an
        // mpv track above, carrying the title and language sub-add was given.
        for (size_t i = 0; i < onlineSubs.size(); i++)
        {
            if (loadedSubs.count((int)i)) continue;
            std::string l = config::langLabelFor(onlineSubs[i].lang);
            if (l.empty()) l = "Unknown";
            if (!onlineSubs[i].addon.empty()) l += " - " + onlineSubs[i].addon;
            picks.push_back({ -1, (int)i });
            labels.push_back(l);
        }

        // A last, inert entry when the lookup has something to say for itself.
        // Picking it does nothing -- it is there so an empty-looking list is
        // explained rather than just empty.
        const char* note = nullptr;
        if (watch.authKey.empty() || watch.videoId.empty())
            note = nullptr;  // local torrent: no addons were ever in play
        else if (onlineSubState == SubFetch::Busy ||
                 onlineSubState == SubFetch::Idle)
            note = "Searching addons...";
        else if (onlineSubState == SubFetch::Failed)
            note = "Addons unavailable";
        if (note)
        {
            picks.push_back({ kNoteSid, -1 });
            labels.push_back(note);
        }

        if (labels.size() > 1)  // something besides "Off"
        {
            auto* s = new TrackCell();
            s->init("Subtitles", labels, cur, [this, picks](int sel) {
                const Pick& p = picks[(size_t)sel];
                if (p.sid == kNoteSid) return;  // the status entry: not a choice
                if (p.sub >= 0)
                {
                    loadOnlineSub(p.sub);
                    return;
                }
                if (p.sid < 0)
                {
                    mpv_set_property_string(mpv, "sid", "no");
                    return;
                }
                char v[24];
                std::snprintf(v, sizeof(v), "%lld", (long long)p.sid);
                mpv_set_property_string(mpv, "sid", v);
            });
            content->addView(s);
        }
        else  // nothing anywhere: say so rather than offering a bare "Off"
        {
            auto* s = new brls::SelectorCell();
            s->init("Subtitles", { "None" }, 0, [](int) {});
            content->addView(s);
        }
    }

    // Subtitle size, as a percentage of the size the app configures at startup
    // (sub-font-size): sub-scale multiplies it, so 100% is that baseline rather
    // than mpv's own default. Only offered when there is something to scale --
    // including a subtitle only an addon has yet, since picking it is one step
    // away in the row above.
    if (sIds.size() > 1 || !onlineSubs.empty())
    {
        const std::vector<double> scales = { 0.75, 0.9, 1.0, 1.15, 1.3, 1.5 };
        auto* z = new brls::SelectorCell();
        z->init("Subtitle size",
                { "75%", "90%", "100%", "115%", "130%", "150%" },
                nearest(scales, getScale("sub-scale")),
                [scales, setScale](int sel) { setScale("sub-scale", scales[sel]); });
        content->addView(z);
    }

    // Subtitle timing. Offered whenever there is any subtitle to shift --
    // including one just pulled from an addon, which is where a release that
    // does not match the rip usually comes from. Finer steps near zero: that is
    // where the answer almost always is, and a 21-entry list of even steps is
    // worse to walk than an uneven one that puts the useful values up front.
    if (sIds.size() > 1 || !onlineSubs.empty())
    {
        // A readout, not a dropdown. L/R step it by 0.1 s while the panel is
        // open, so the figure is very often not one of a preset list -- and a
        // selector could only ever show the nearest preset, which is how
        // +0.10 s used to display as "None".
        auto* sd = new brls::DetailCell();
        sd->setText("Subtitle delay");
        sd->setDetailText(subDelayText(subDelay));
        sd->setDetailTextColor(
            brls::Application::getTheme()["brls/list/listItem_value_color"]);
        sd->setFocusable(false);  // nothing to activate: L/R are the control
        sd->setLineBottom(0.0f);
        content->addView(sd);

        // Fed by setSubDelay for as long as the panel is up.
        subDelaySink = [sd](double d) { sd->setDetailText(subDelayText(d)); };

        auto* hint = new brls::Label();
        hint->setText("L / R shift the subtitles by 0.1 s. Later is positive. "
                      "The video keeps playing behind this, which is the only "
                      "way to see whether it lands.");
        hint->setFontSize(15.0f);
        hint->setTextColor(theme::textMuted());
        hint->setLineHeight(1.35f);
        hint->setMargins(6.0f, 12.0f, 4.0f, 12.0f);
        content->addView(hint);

        // L/R belong to the delay in here. The player registers the same two
        // buttons for the speed, but those never fire while the panel has the
        // cursor -- actions are dispatched up the focused view's own tree.
        auto nudge = [this](double delta) {
            setSubDelay(subDelay + delta);
            return true;
        };
        panel->registerAction(
            "Subtitles earlier", brls::BUTTON_LB,
            [nudge](brls::View*) { return nudge(-0.1); }, false, true,
            brls::SOUND_NONE);
        panel->registerAction(
            "Subtitles later", brls::BUTTON_RB,
            [nudge](brls::View*) { return nudge(0.1); }, false, true,
            brls::SOUND_NONE);
    }

    auto liveFlag = this->alive;
    brls::Application::pushActivity(
        new PlayerSettingsActivity(root, [this, liveFlag]() {
            if (!*liveFlag) return;
            settingsOpen = false;
            subDelaySink = nullptr;  // the row it fed is about to be freed
            // Back to whatever the overlay was doing: up while paused, and up
            // mid-scrub too -- that is where the seek bar lives.
            setControlsVisible(userPaused || seeking);
        }));
}

MpvView::~MpvView()
{
    brls::Logger::info("[teardown] ~MpvView enter");

    // Playback is ending: let the OS dim/sleep on idle again.
    appletSetMediaPlaybackState(false);

    // Last chance to tell Stremio where playback stopped. Uses the position
    // sampled during playback (see logStats) -- mpv is about to be destroyed,
    // and the push itself runs on a background thread with no reference to us.
    maybePushWatchState(true);

    // Tell any in-flight startEngine() that we are gone, so it closes the engine
    // it opened instead of writing into a destroyed view.
    *alive = false;

    // Cancel first so the mpv demuxer's parked read returns (~20 ms); mpv's
    // shutdown touches GL, so it MUST stay on this (UI/GL) thread -- doing it on
    // a background thread crashed. This part is fast.
    if (tfs)
        torrentfs_cancel(tfs);
    if (renderCtx)
        mpv_render_context_free(renderCtx);
    if (mpv)
        mpv_terminate_destroy(mpv);

    // Close the engine synchronously (~1 s: it joins the acceptors/workers).
    //
    // This used to be handed to a detached std::thread so the menu came back
    // instantly, which is what crashed on B: std::thread's constructor throws
    // std::system_error when the thread can't be created, and a throw out of a
    // destructor (implicitly noexcept) is an immediate std::terminate/abort.
    // abort() then runs libnx's exit path -> userAppExit -> socketExit(), which
    // tears down the BSD socket layer while the engine's workers are still
    // live -- their next socket call dereferences a NULL devoptab and data
    // aborts. Joining here keeps the engine's lifetime strictly inside this
    // destructor, so nothing can outlive it.
    torrentfs* t = tfs;
    tfs          = nullptr;
    mpv          = nullptr;
    renderCtx    = nullptr;
    if (t)
        torrentfs_close(t);

    brls::Logger::info("[teardown] ~MpvView leave (engine closed)");
}

void MpvView::pumpEvents()
{
    if (!mpv)
        return;

    // Pause when the console takes focus away (HOME menu / overlay), so the video
    // does not keep playing -- and its audio blaring -- in the background. Left
    // paused; the user resumes with A or a tap when they come back.
    if (ready && !userPaused && appletGetFocusState() != AppletFocusState_InFocus)
    {
        userPaused = true;
        mpv_set_property_string(mpv, "pause", "yes");
        setControlsVisible(true);
    }

    // The boost follows the dock state, which can change mid-film -- putting the
    // console on the dock has to drop it, and taking it off has to bring it
    // back. Only touches mpv on an actual transition.
    if (bool want = audioBoostWanted(); want != boostApplied)
    {
        boostApplied = want;
        mpv_set_property_string(mpv, "af", want ? kAudioBoostFilter : "");
        brls::Logger::info("[audio] loudness boost {}", want ? "on" : "off");
    }

    while (true)
    {
        mpv_event* ev = mpv_wait_event(mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE)
            break;
        switch (ev->event_id)
        {
            case MPV_EVENT_FILE_LOADED:
                // Header downloaded and demuxed: we're now buffering, not
                // connecting.
                fileLoaded = true;
                brls::Logger::info("[mpv event] file loaded");
                // Ask the subtitle addons now: the buffering wait is dead time
                // for the network anyway, and the list has to be there before
                // the first X press for the menu to be worth anything.
                fetchOnlineSubs();
                break;
            case MPV_EVENT_END_FILE:
            {
                // Played to the end (not stop/error): close the player and
                // return. B is handled separately and pops only one level.
                auto* ef = (mpv_event_end_file*)ev->data;
                if (ef && ef->reason == MPV_END_FILE_REASON_EOF && !ended)
                {
                    ended   = true;
                    int pops = watch.endPop;
                    brls::Logger::info("[mpv event] EOF -> pop {}", pops);
                    // Deferred: we are inside draw()'s pumpEvents, and popping
                    // (which frees this view) mid-draw is unsafe.
                    brls::sync([pops]() { popActivities(pops); });
                }
                break;
            }
            case MPV_EVENT_PLAYBACK_RESTART:
                // First frame is decoded, but with cache-pause-initial mpv is
                // still buffering-paused here; readiness is driven off buffered
                // seconds in updateLoadingOverlay(), not this event.
                brls::Logger::info("[mpv event] playback restart (first frame)");
                break;
            case MPV_EVENT_LOG_MESSAGE:
            {
                auto* m = (mpv_event_log_message*)ev->data;
                brls::Logger::debug("[mpv:{}] {}: {}", m->level, m->prefix,
                                    m->text);
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
            {
                auto* p = (mpv_event_property*)ev->data;
                if (!p || !p->data)
                    break;
                if (p->format == MPV_FORMAT_DOUBLE)
                {
                    double v = *(double*)p->data;
                    if (std::strcmp(p->name, "demuxer-cache-duration") == 0)
                    {
                        obsCacheSecs = v;
                        if (tfs)
                            torrentfs_set_backlog(tfs, (int)(v * 1000.0));
                    }
                    else if (std::strcmp(p->name, "time-pos") == 0)
                        obsPos = v;
                    else if (std::strcmp(p->name, "duration") == 0)
                        obsDur = v;
                }
                else if (p->format == MPV_FORMAT_FLAG &&
                         std::strcmp(p->name, "demuxer-cache-idle") == 0)
                    obsCacheIdle = *(int*)p->data != 0;
                break;
            }
            default:
                brls::Logger::info("[mpv event] {}",
                                   mpv_event_name(ev->event_id));
                break;
        }
    }
}

// Paints `path` as the background, blurring it first when the artwork is a
// poster (see PlayerArt::blurBg).
void MpvView::setBackgroundArt(const std::string& path)
{
    if (!bgImage || path.empty()) return;
    std::string use = art.blurBg ? stremio::blurredPosterPath(path) : "";
    bgImage->setImageFromFile(use.empty() ? path : use);
}

void MpvView::buildLoadingOverlay(const std::string& title)
{
    brls::Theme theme = brls::Application::getTheme();

    // The dark theme's text_disabled is RGB(80,80,80) -- it is meant for a
    // disabled control on a flat background, and over the artwork it is barely
    // there. Secondary text here is dimmer than the title but still readable.
    const NVGcolor dimText = nvgRGB(190, 190, 195);

    // Full-screen centred column over the (black) video.
    loadingOverlay = new brls::Box();
    // Carries NO padding of its own: a percentage-sized child resolves against
    // the content box while an absolute one is placed against the padding box,
    // so the full-bleed background below came out 120px short (a black strip
    // down the right edge). The padding lives on the inner column instead.
    loadingOverlay->setAxis(brls::Axis::COLUMN);
    loadingOverlay->setGrow(1.0f);
    loadingOverlay->setBackgroundColor(theme.getColor("brls/background"));

    // Full-screen artwork behind everything else. Absolute so it is out of the
    // column's flow, and added first so the column draws on top of it.
    if (!art.bgId.empty() || !art.posterPath.empty())
    {
        bgImage = new brls::Image();
        bgImage->setPositionType(brls::PositionType::ABSOLUTE);
        bgImage->setPositionTop(0.0f);
        bgImage->setPositionLeft(0.0f);
        bgImage->setWidthPercentage(100.0f);
        bgImage->setHeightPercentage(100.0f);
        // FILL, not FIT: a poster is 2:3 and the screen is 16:9, so fitting it
        // would letterbox the "background" into a strip down the middle.
        bgImage->setScalingType(brls::ImageScalingType::FILL);
        // Faint enough that the text over it stays readable.
        bgImage->setAlpha(0.18f);
        loadingOverlay->addView(bgImage);

        // Show the thumbnail we already have right away, then swap in the
        // full-size art when it lands -- rather than a black screen for as long
        // as the download takes.
        setBackgroundArt(art.posterPath);
        if (!art.bgId.empty())
        {
            auto liveFlag = this->alive;
            stremio::fetchHqArtAsync(art.bgId, art.bgUrl,
                                     [this, liveFlag](std::string path) {
                                         if (!*liveFlag || path.empty()) return;
                                         setBackgroundArt(path);
                                     });
        }
    }

    // Everything the user actually reads, centred over the background.
    auto* column = new brls::Box();
    column->setAxis(brls::Axis::COLUMN);
    column->setJustifyContent(brls::JustifyContent::CENTER);
    column->setAlignItems(brls::AlignItems::CENTER);
    column->setGrow(1.0f);
    column->setPadding(0, 60, 0, 60);
    loadingOverlay->addView(column);

    // The poster when the caller knows what we're playing, otherwise the app
    // logo (bundled at romfs:/NX-torrent-player-bg-rounded.png).
    auto* logo = new brls::Image();
    if (!art.posterPath.empty())
    {
        logo->setImageFromFile(art.posterPath);
        logo->setDimensions(148.0f, 222.0f);  // posters are 2:3
        logo->setCornerRadius(8.0f);           // rounded poster corners
    }
    else
    {
        logo->setImageFromRes("NX-torrent-player-bg-rounded.png");
        logo->setDimensions(148.0f, 148.0f);
    }
    logo->setScalingType(brls::ImageScalingType::FIT);
    logo->setMargins(0, 0, 28, 0);
    column->addView(logo);

    auto* titleLabel = new brls::Label();
    titleLabel->setText(title);
    titleLabel->setFontSize(28);
    titleLabel->setTextColor(theme.getColor("brls/text"));
    titleLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    titleLabel->setMargins(0, 0, 44, 0);
    column->addView(titleLabel);

    statusLabel = new brls::Label();
    statusLabel->setText("Connecting to peers...");
    statusLabel->setFontSize(21);
    statusLabel->setTextColor(theme.getColor("brls/text"));
    statusLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    statusLabel->setMargins(0, 0, 22, 0);
    column->addView(statusLabel);

    // Progress bar: a rounded track (spanning the screen width) holding an
    // accent-coloured fill.
    auto* barTrack = new brls::Box();
    barTrack->setAxis(brls::Axis::ROW);
    barTrack->setAlignItems(brls::AlignItems::CENTER);
    barTrack->setWidthPercentage(100.0f);
    barTrack->setHeight(10.0f);
    barTrack->setCornerRadius(5.0f);
    barTrack->setBackgroundColor(nvgRGBA(255, 255, 255, 38));

    barFill = new brls::Box();
    barFill->setHeight(10.0f);
    barFill->setWidthPercentage(0.0f);
    barFill->setCornerRadius(5.0f);
    barFill->setBackgroundColor(theme.getColor("brls/accent"));
    barTrack->addView(barFill);
    column->addView(barTrack);

    percentLabel = new brls::Label();
    percentLabel->setText("0%");
    percentLabel->setFontSize(18);
    percentLabel->setTextColor(theme.getColor("brls/text"));
    percentLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    percentLabel->setMargins(16, 0, 0, 0);
    column->addView(percentLabel);

    statsLabel = new brls::Label();
    statsLabel->setText("");
    statsLabel->setFontSize(16);
    statsLabel->setTextColor(dimText);
    statsLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    statsLabel->setMargins(30, 0, 0, 0);
    column->addView(statsLabel);

    // Animated spinner so it's clear the app is working even when the swarm is
    // slow to feed us.
    auto* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->setDimensions(52.0f, 52.0f);
    spinner->setMargins(34, 0, 0, 0);
    spinner->animate(true);
    column->addView(spinner);

    // Hint that B cancels and returns to the list.
    auto* backHint = new brls::Label();
    backHint->setText("Press B to go back");
    backHint->setFontSize(18);
    backHint->setTextColor(dimText);
    backHint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    backHint->setMargins(40, 0, 0, 0);
    column->addView(backHint);

    this->addView(loadingOverlay);

    // Separate, small buffering badge shown over the video when playback stalls
    // (the swarm can't keep up). Hidden by default.
    bufferOverlay = new brls::Box();
    bufferOverlay->setAxis(brls::Axis::ROW);
    bufferOverlay->setAlignItems(brls::AlignItems::CENTER);
    bufferOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    // Below the top-right controls cluster (Options + lock pills reach ~104px), so
    // the buffering badge no longer sits on top of it, and right-aligned to the
    // lock pill (the cluster sits at right:60).
    bufferOverlay->setPositionTop(124.0f);
    bufferOverlay->setPositionRight(60.0f);
    bufferOverlay->setPadding(12.0f, 20.0f, 12.0f, 20.0f);
    bufferOverlay->setCornerRadius(8.0f);
    bufferOverlay->setBackgroundColor(nvgRGBA(0, 0, 0, 170));
    bufferOverlay->setVisibility(brls::Visibility::GONE);

    auto* bufSpinner =
        new brls::ProgressSpinner(brls::ProgressSpinnerSize::NORMAL);
    bufSpinner->setDimensions(28.0f, 28.0f);
    bufSpinner->animate(true);
    bufferOverlay->addView(bufSpinner);

    auto* bufLabel = new brls::Label();
    bufLabel->setText("Buffering...");
    bufLabel->setFontSize(18);
    bufLabel->setTextColor(nvgRGB(255, 255, 255));
    bufLabel->setMargins(0, 0, 0, 12);
    bufferOverlay->addView(bufLabel);

    this->addView(bufferOverlay);

    // Centre play/pause button, shown with the controls overlay. The overlay box
    // spans the screen (to centre the button) but is NOT tappable itself -- only
    // the bounded button below is, so a tap anywhere else falls through to the
    // video's show/hide-overlay tap.
    pauseOverlay = new brls::Box();
    pauseOverlay->setAxis(brls::Axis::ROW);
    pauseOverlay->setJustifyContent(brls::JustifyContent::CENTER);
    pauseOverlay->setAlignItems(brls::AlignItems::CENTER);
    pauseOverlay->setGrow(1.0f);
    pauseOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    pauseOverlay->setPositionTop(0.0f);
    pauseOverlay->setPositionLeft(0.0f);
    pauseOverlay->setWidthPercentage(100.0f);
    pauseOverlay->setHeightPercentage(100.0f);
    pauseOverlay->setVisibility(brls::Visibility::GONE);
    {
        auto* btn = new PlayPauseButton();
        btn->paused = &userPaused;  // draws play vs pause from the live flag
        btn->setDimensions(140.0f, 140.0f);
        btn->setCornerRadius(70.0f);
        btn->setBackgroundColor(nvgRGBA(0, 0, 0, 90));
        // Tap the centre button to play/pause (works like the A button). Bounded,
        // so taps elsewhere toggle the overlay instead.
        btn->addGestureRecognizer(
            new brls::TapGestureRecognizer(btn, [this]() {
                if (controlsLocked) { flashLock(); return; }
                onPlayPause();
            }));
        pauseOverlay->addView(btn);
    }
    this->addView(pauseOverlay);

    // Title, top-left, shown alongside the pause icon. Semi-transparent pill so
    // it reads over any frame.
    pauseTitleBox = new brls::Box();
    pauseTitleBox->setPositionType(brls::PositionType::ABSOLUTE);
    pauseTitleBox->setPositionTop(48.0f);
    pauseTitleBox->setPositionLeft(60.0f);
    pauseTitleBox->setPadding(12.0f, 22.0f, 12.0f, 22.0f);
    pauseTitleBox->setCornerRadius(8.0f);
    pauseTitleBox->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    pauseTitleBox->setVisibility(brls::Visibility::GONE);
    {
        auto* tl = new brls::Label();
        tl->setText(pauseTitle);
        tl->setFontSize(26.0f);
        tl->setTextColor(nvgRGB(255, 255, 255));
        tl->setSingleLine(true);
        // Cap the width so a long title is cut with an ellipsis instead of
        // running across the screen into the top-right hints. Shortened to clear
        // the wider top-right cluster (Options + the Y-lock hint). Short titles
        // keep their natural width (the pill shrinks to fit).
        tl->setMaxWidth(760.0f);
        pauseTitleBox->addView(tl);
    }
    this->addView(pauseTitleBox);

    // Top-right cluster shown while paused: the Options hint, then the Y-lock
    // hint at the far right. Right-anchored so they stay together against the
    // edge, and each pill toggles its own visibility (the lock one also flashes
    // on a blocked input) without the other moving.
    auto* topRight = new brls::Box();
    topRight->setPositionType(brls::PositionType::ABSOLUTE);
    topRight->setPositionTop(48.0f);
    topRight->setPositionRight(60.0f);
    topRight->setAxis(brls::Axis::ROW);
    topRight->setAlignItems(brls::AlignItems::CENTER);
    this->addView(topRight);

    // The X button glyph + a gear: X opens the audio/subtitle options. Same
    // show/hide as the pause overlays. Nudged left of the Y-lock pill.
    optionsHint = new brls::Box();
    // Fixed height (not content-driven): keeps both pills the same thickness and
    // stops the cluster from reflowing -- and the lock pill from jumping a pixel
    // -- when Options is hidden on lock. Vertical space comes from centring.
    optionsHint->setHeight(56.0f);
    optionsHint->setPadding(4.0f, 20.0f, 0.0f, 20.0f);
    optionsHint->setMarginRight(16.0f);
    optionsHint->setCornerRadius(8.0f);
    optionsHint->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    optionsHint->setAxis(brls::Axis::ROW);
    optionsHint->setAlignItems(brls::AlignItems::CENTER);
    optionsHint->setVisibility(brls::Visibility::GONE);
    {
        auto* x = new brls::Label();
        x->setText("");  // borealis font: the X button glyph
        x->setFontSize(26.0f);
        x->setTextColor(nvgRGB(255, 255, 255));
        x->setMargins(0.0f, 12.0f, 0.0f, 0.0f);
        optionsHint->addView(x);

        // Gear from the Material Icons font bundled in romfs (settings glyph).
        auto* gear = new brls::Label();
        gear->setText("");
        gear->setFontSize(26.0f);
        gear->setTextColor(nvgRGB(255, 255, 255));
        // Nudge it down a touch: the Material gear glyph sits high in its box and
        // otherwise does not line up with the X button glyph next to it.
        gear->setMargins(5.0f, 0.0f, 0.0f, 0.0f);
        optionsHint->addView(gear);
    }
    topRight->addView(optionsHint);

    // The Y button glyph + "Lock": a hint that Y locks the controls while paused,
    // and the indicator that flashes when a blocked input is attempted (its label
    // then reads "Locked", see flashLock). Far right of the cluster.
    lockHint = new brls::Box();
    lockHint->setHeight(56.0f);  // match optionsHint exactly (see the note there)
    lockHint->setPadding(4.0f, 20.0f, 0.0f, 20.0f);
    lockHint->setCornerRadius(8.0f);
    lockHint->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    lockHint->setAxis(brls::Axis::ROW);
    lockHint->setAlignItems(brls::AlignItems::CENTER);
    lockHint->setVisibility(brls::Visibility::GONE);
    {
        auto* y = new brls::Label();
        y->setText("\xEE\x83\xA3");  // borealis font: the Y button glyph (U+E0E3)
        y->setFontSize(26.0f);
        y->setTextColor(nvgRGB(255, 255, 255));
        y->setMargins(0.0f, 12.0f, 0.0f, 0.0f);
        lockHint->addView(y);

        // Padlock glyph (Material Icons): open = a hint that Y locks; closed =
        // locked. Nudged down a touch like the gear, which sits high in its box.
        lockLabel = new brls::Label();
        lockLabel->setText(kGlyphLockOpen);
        lockLabel->setFontSize(26.0f);
        lockLabel->setTextColor(nvgRGB(255, 255, 255));
        lockLabel->setMargins(4.0f, 0.0f, 0.0f, 0.0f);
        lockHint->addView(lockLabel);
    }
    topRight->addView(lockHint);

    // The L/R readout, top centre: shown for a moment on a speed or a
    // subtitle-delay change and hidden again by updatePill. Its own absolute,
    // full-width row so centring it does not disturb the pills above -- and
    // away from the bottom, where the subtitles it can be about are drawn.
    {
        auto* row = new brls::Box();
        row->setPositionType(brls::PositionType::ABSOLUTE);
        row->setPositionTop(48.0f);
        row->setPositionLeft(0.0f);
        row->setWidthPercentage(100.0f);
        row->setAxis(brls::Axis::ROW);
        row->setJustifyContent(brls::JustifyContent::CENTER);

        hintPill = new brls::Box();
        hintPill->setHeight(56.0f);
        hintPill->setPadding(0.0f, 24.0f, 0.0f, 24.0f);
        hintPill->setCornerRadius(8.0f);
        hintPill->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
        hintPill->setAxis(brls::Axis::ROW);
        hintPill->setAlignItems(brls::AlignItems::CENTER);
        hintPill->setVisibility(brls::Visibility::GONE);
        {
            // Words, not a glyph. The pill says two different things
            // ("Speed 1.25x", "Subtitles +0.10 s") and no one icon covers both
            // -- and a caption glyph could not be drawn anyway: every Material
            // one lives in U+E000-E152, which the Nintendo Extended shared font
            // claims ahead of Material in the fallback chain.
            pillLabel = new brls::Label();
            pillLabel->setText("");
            pillLabel->setFontSize(24.0f);
            pillLabel->setTextColor(nvgRGB(255, 255, 255));
            hintPill->addView(pillLabel);
        }
        row->addView(hintPill);
        this->addView(row);
    }

    // "Next episode", bottom right, over the last kNextCardSecs of an episode
    // that has one. Above the seek bar so the two never overlap, and on the
    // right so it stays clear of the title pill on the left.
    nextCard = new brls::Box();
    nextCard->setAxis(brls::Axis::COLUMN);
    nextCard->setPositionType(brls::PositionType::ABSOLUTE);
    nextCard->setPositionBottom(132.0f);
    nextCard->setPositionRight(70.0f);
    nextCard->setPadding(14.0f, 22.0f, 16.0f, 22.0f);
    nextCard->setCornerRadius(10.0f);
    nextCard->setBackgroundColor(nvgRGBA(0, 0, 0, 170));
    nextCard->setVisibility(brls::Visibility::GONE);
    {
        // The A button glyph, then the words. Two labels rather than one
        // string so the glyph can carry its own size -- it sits small next to
        // 23pt text otherwise. Same font as the X and Y glyphs on the pills
        // above (Nintendo Extended, which owns U+E000-E152).
        auto* headRow = new brls::Box();
        headRow->setAxis(brls::Axis::ROW);
        headRow->setAlignItems(brls::AlignItems::CENTER);

        auto* glyph = new brls::Label();
        glyph->setText("\xEE\x83\xA0");  // U+E0E0, the A button
        glyph->setFontSize(26.0f);
        glyph->setTextColor(nvgRGB(255, 255, 255));
        glyph->setMarginRight(10.0f);
        headRow->addView(glyph);

        auto* head = new brls::Label();
        head->setText("Next episode");
        head->setFontSize(23.0f);
        head->setTextColor(nvgRGB(255, 255, 255));
        headRow->addView(head);

        nextCard->addView(headRow);

        nextCardSub = new brls::Label();
        nextCardSub->setText("");
        nextCardSub->setFontSize(18.0f);
        nextCardSub->setTextColor(nvgRGB(186, 186, 192));
        nextCardSub->setMarginTop(4.0f);
        nextCard->addView(nextCardSub);
    }
    // Touch works whatever the playback state, which is the way through while
    // paused (A resumes then).
    nextCard->addGestureRecognizer(
        new brls::TapGestureRecognizer(nextCard, [this]() {
            if (controlsLocked) { flashLock(); return; }
            goToNextEpisode();
        }));
    this->addView(nextCard);

    // Seek bar at the bottom while paused: elapsed | progress | total.
    seekOverlay = new brls::Box();
    seekOverlay->setAxis(brls::Axis::ROW);
    seekOverlay->setAlignItems(brls::AlignItems::CENTER);
    seekOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    seekOverlay->setPositionLeft(70.0f);
    seekOverlay->setPositionRight(70.0f);
    seekOverlay->setPositionBottom(56.0f);
    seekOverlay->setHeight(64.0f);
    seekOverlay->setPadding(0, 26, 0, 26);
    seekOverlay->setCornerRadius(10.0f);
    seekOverlay->setBackgroundColor(nvgRGBA(0, 0, 0, 165));
    seekOverlay->setVisibility(brls::Visibility::GONE);

    // Fixed width, or the bar jitters: a Label sizes to its text, the track
    // grows into whatever is left, so every "9:59" -> "10:00" widened the label
    // and shoved the track a few pixels. While scrubbing the timer changes
    // constantly, which made the whole bar shiver. Wide enough for "H:MM:SS".
    seekCur = new brls::Label();
    seekCur->setText("0:00");
    seekCur->setFontSize(22);
    seekCur->setTextColor(nvgRGB(255, 255, 255));
    seekCur->setWidth(104.0f);
    seekCur->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    seekCur->setMargins(0, 20, 0, 0);
    seekOverlay->addView(seekCur);

    seekTrack = new brls::Box();
    seekTrack->setAxis(brls::Axis::ROW);
    seekTrack->setAlignItems(brls::AlignItems::CENTER);
    seekTrack->setGrow(1.0f);
    seekTrack->setHeight(8.0f);
    seekTrack->setCornerRadius(4.0f);
    seekTrack->setBackgroundColor(nvgRGBA(255, 255, 255, 45));

    seekFill = new brls::Box();
    seekFill->setHeight(8.0f);
    seekFill->setWidth(0.0f);  // driven in pixels from the measured track width
    seekFill->setCornerRadius(4.0f);
    seekFill->setBackgroundColor(theme.getColor("brls/accent"));
    seekTrack->addView(seekFill);

    // Cursor: absolutely positioned, so it is NOT part of the row's flex flow.
    // As a normal child it sat next to the fill and competed for width, so the
    // fill got squeezed by the cursor's 6 px and could never reach 100% -- the
    // cursor visibly pushed the bar instead of riding on it. Its left offset is
    // driven from the same percentage as the fill (see updateSeekBar).
    seekCursor = new brls::Box();
    seekCursor->setPositionType(brls::PositionType::ABSOLUTE);
    seekCursor->setPositionLeft(0.0f);
    seekCursor->setDimensions(6.0f, 26.0f);
    seekCursor->setCornerRadius(3.0f);
    seekCursor->setBackgroundColor(nvgRGB(255, 255, 255));
    seekTrack->addView(seekCursor);

    seekOverlay->addView(seekTrack);

    seekTotal = new brls::Label();
    seekTotal->setText("0:00");
    seekTotal->setFontSize(22);
    seekTotal->setTextColor(nvgRGB(255, 255, 255));
    seekTotal->setWidth(104.0f);  // fixed for the same reason as seekCur
    seekTotal->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    seekTotal->setMargins(0, 0, 0, 20);
    seekOverlay->addView(seekTotal);
    this->addView(seekOverlay);

    // --- Touch controls --------------------------------------------------
    // Tap the video to show/hide the controls overlay. Taps on the children below
    // (centre play/pause button, seek bar, Options / lock hints) interrupt this
    // one -- borealis lets a child gesture win over its ancestor -- so they act on
    // their own areas instead of toggling the overlay.
    this->addGestureRecognizer(
        new brls::TapGestureRecognizer(this, [this]() {
            if (controlsLocked) { flashLock(); return; }
            // A tap on the video shows/hides the controls overlay -- it does NOT
            // play/pause (that is the centre button now).
            if (ready) setControlsVisible(!controlsShown);
        }));

    // Tap anywhere along the seek bar to jump there: map the tap's x within the
    // measured track to a fraction of the duration.
    seekOverlay->addGestureRecognizer(new brls::TapGestureRecognizer(
        [this](brls::TapGestureStatus status, brls::Sound*) {
            if (controlsLocked) { flashLock(); return; }
            if (status.state != brls::GestureState::END || !seekTrack)
                return;
            float w = seekTrack->getWidth();
            if (w > 0.0f)
                seekToFraction((status.position.x - seekTrack->getX()) / w);
        }));

    // Drag along the seek bar to scrub: the cursor/target follows the finger
    // live (updateSeekBar draws it while `seeking`), and the seek is committed on
    // release -- one seek instead of one per pixel.
    seekOverlay->addGestureRecognizer(new brls::PanGestureRecognizer(
        [this](brls::PanGestureStatus status, brls::Sound*) {
            if (controlsLocked) { flashLock(); return; }
            if (!ready || !mpv || !seekTrack || obsDur <= 0.0)
                return;
            float w = seekTrack->getWidth();
            if (w <= 0.0f)
                return;
            double frac = (status.position.x - seekTrack->getX()) / w;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            if (status.state == brls::GestureState::END)
                seekToFraction(frac);  // commit there, resume, hide the bar
            else  // START / STAY: track the finger
            {
                // Pause and capture the start position, like the stick/button
                // scrub -- without this the video kept playing under the preview
                // seeks and the two fought (jumpy playback while dragging).
                if (!seeking) beginSeek();
                seekTarget = frac * seekDur;
                setControlsVisible(true);
            }
        },
        brls::PanAxis::HORIZONTAL));

    // Tap the "Options" hint (top-right, shown while paused) for the track menu.
    if (optionsHint)
        optionsHint->addGestureRecognizer(new brls::TapGestureRecognizer(
            optionsHint, [this]() {
                if (controlsLocked) { flashLock(); return; }
                openTrackMenu();
            }));

    // Tap the Y-lock pill to toggle the lock -- like the Y button, and NOT gated,
    // so a tap on it is the way to unlock by touch too. (While locked and idle the
    // pill is hidden; a tap on the video flashes it, and a tap on it then unlocks.)
    if (lockHint)
        lockHint->addGestureRecognizer(
            new brls::TapGestureRecognizer(lockHint, [this]() { toggleLock(); }));

    // Semi-transparent network/torrent info panel, toggled with ZR. Two columns:
    // the network/worker counters alone are longer than the screen is tall, so a
    // single column pushed the video section off the bottom where it could never
    // be read.
    infoOverlay = new brls::Box();
    infoOverlay->setAxis(brls::Axis::ROW);
    infoOverlay->setPositionType(brls::PositionType::ABSOLUTE);
    infoOverlay->setPositionTop(40.0f);
    infoOverlay->setPositionLeft(40.0f);
    infoOverlay->setWidth(960.0f);
    // An absolute-positioned box with auto height can lay out to zero size and
    // never render, so pin an explicit height big enough for all the lines.
    infoOverlay->setHeight(660.0f);
    infoOverlay->setPadding(16.0f, 20.0f, 16.0f, 20.0f);
    infoOverlay->setCornerRadius(8.0f);
    infoOverlay->setBackgroundColor(nvgRGBA(0, 0, 0, 150));
    infoOverlay->setVisibility(brls::Visibility::GONE);

    auto makeColumn = [&](float w) {
        auto* l = new brls::Label();
        l->setText("");
        // Small: the WORKERS list alone is ~17 lines and ran off the bottom of
        // the screen at a larger size.
        l->setFontSize(14);
        l->setTextColor(nvgRGB(235, 235, 235));
        l->setIsWrapping(true);
        l->setWidth(w);
        infoOverlay->addView(l);
        return l;
    };
    infoLabel  = makeColumn(520.0f);  // network + workers
    infoLabel2 = makeColumn(380.0f);  // video + source
    infoLabel2->setMarginLeft(20.0f);
    this->addView(infoOverlay);
}

void MpvView::updateLoadingOverlay()
{
    if (!loadingOverlay)
        return;

    // The engine isn't open yet: for a magnet that means we're walking the
    // swarm for whoever will serve the metadata, one peer at a time. Show that
    // count -- without it the screen looks frozen for a minute and there is no
    // way to tell a slow fetch from a hung one.
    if (!tfs)
    {
        int tried = torrent_meta_peers_tried;
        int total = torrent_meta_peers_total;
        if (statusLabel && total > 0)
        {
            char b[96];
            std::snprintf(b, sizeof(b), "Metadata: peer %d / %d", tried,
                          total);
            statusLabel->setText(b);
        }
        return;
    }

    // Buffering percent (cheap; refresh every frame so the bar is smooth).
    // The bar tracks how much video mpv has buffered toward the kBufferSecs
    // target; 100% is reached exactly when playback is allowed to start.
    int pct = 0;
    if (fileLoaded && mpv)
    {
        // Async-observed values (pumpEvents): no sync mpv call on this thread.
        double secs = obsCacheSecs;
        if (secs >= 0.0)
        {
            pct = (int)(secs / kBufferSecs * 100.0);
            if (secs >= kBufferSecs)
                ready = true;  // buffered enough -> unpause and show video
        }

        // Safety net for short files / a full demuxer cache: if there's nothing
        // left to read, don't wait for the full kBufferSecs.
        if (obsCacheIdle && pct > 0)
            ready = true;
    }
    if (pct > 100)
        pct = 100;
    if (pct < 0)
        pct = 0;

    if (pct != shownPct)
    {
        shownPct = pct;
        barFill->setWidthPercentage((float)pct);
        char pb[16];
        std::snprintf(pb, sizeof(pb), "%d%%", pct);
        percentLabel->setText(pb);
    }

    // Text stats: refresh ~twice a second (each setText relayouts).
    auto now  = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - lastSample).count();
    if (dt >= 0.5)
    {
        int64_t bytes = tfs ? torrentfs_bytes_recv(tfs) : 0;
        speedBps      = (double)(bytes - lastBytes) / dt;
        if (speedBps < 0)
            speedBps = 0;
        lastBytes  = bytes;
        lastSample = now;

        int peers = tfs ? torrentfs_peer_count(tfs) : 0;
        int64_t piecesDone = 0, piecesTotal = 0, playhead = 0;
        if (tfs)
            torrentfs_stats(tfs, &piecesDone, &piecesTotal, &playhead);

        const char* status;
        if (!fileLoaded)
            status = (peers > 0) ? "Downloading header..." : "Connecting to peers...";
        else
            status = "Buffering...";
        statusLabel->setText(status);

        char sbuf[192];
        double spd = speedBps / (1024.0 * 1024.0);
        std::snprintf(sbuf, sizeof(sbuf),
                      "%d peer%s · %.1f MB/s · %lld / %lld pieces",
                      peers, peers == 1 ? "" : "s", spd, (long long)piecesDone,
                      (long long)piecesTotal);
        statsLabel->setText(sbuf);
    }
}

void MpvView::updateBufferIndicator()
{
    if (!bufferOverlay || !mpv)
        return;

    // Show the badge when the demuxer has almost nothing buffered ahead (the
    // stream is struggling); hide it once it recovers. Small hysteresis so it
    // doesn't flicker.
    double secs = obsCacheSecs;  // async-observed; no sync mpv call per frame
    bool stalling = buffering ? (secs < 1.0) : (secs < 0.25);
    if (stalling != buffering)
    {
        buffering = stalling;
        bufferOverlay->setVisibility(stalling ? brls::Visibility::VISIBLE
                                              : brls::Visibility::GONE);
    }
}

void MpvView::updateInfoOverlay()
{
    if (!infoOverlay || !infoShown)
        return;

    // Refresh ~twice a second (each setText relayouts).
    auto now  = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - infoLastSample).count();
    if (dt < 0.5)
        return;

    // Before torrentfs_open returns there is no engine to report on -- but that
    // is exactly the phase (a magnet's metadata fetch) that used to look hung,
    // so show what IS happening rather than an empty panel.
    if (!tfs)
    {
        infoLastSample = now;
        int state = torrent_meta_state;
        int tried = torrent_meta_peers_tried;
        int total = torrent_meta_peers_total;
        int conn  = torrent_meta_connected;
        int trk   = torrent_meta_trackers;

        // Elapsed in the current fetch, reset when a fresh one starts (state
        // rewinds to parse/announce). Single player at a time, so a static is
        // enough and needs no header field.
        static std::chrono::steady_clock::time_point metaStart = now;
        static int lastState = -1;
        if (state != lastState && state <= META_ANNOUNCE)
            metaStart = now;
        lastState   = state;
        int elapsed = (int)std::chrono::duration<double>(now - metaStart).count();

        char m[512];
        std::snprintf(m, sizeof(m),
                      "METADATA FETCH\n"
                      "State: %s\n"
                      "Elapsed: %d s   Trackers: %d\n"
                      "Peers: tried %d / %d   reachable %d\n"
                      "Last peer error: %s",
                      torrent_meta_state_str(state), elapsed, trk, tried, total,
                      conn,
                      torrent_meta_last_err[0] ? torrent_meta_last_err : "-");
        infoLabel->setText(m);
        if (infoLabel2) infoLabel2->setText("");
        return;
    }

    int64_t bytes = torrentfs_bytes_recv(tfs);
    infoSpeed     = (double)(bytes - infoLastBytes) / dt;
    if (infoSpeed < 0)
        infoSpeed = 0;
    infoLastBytes  = bytes;
    infoLastSample = now;

    int peers    = torrentfs_peer_count(tfs);
    int incoming = torrentfs_incoming_count(tfs);
    int64_t done = 0, total = 0, ph = 0;
    torrentfs_stats(tfs, &done, &total, &ph);
    int pct = total > 0 ? (int)(done * 100 / total) : 0;

    // Worker diagnostics. These are what tell apart "bytes arrive but never
    // become a piece": choked = peers never let us request, sha_fail = pieces
    // complete but fail verification, fetch_fail = sessions die mid-piece.
    int c[10] = { 0 };
    torrentfs_debug_counts(tfs, c);
    int live = 0, peak = 0, connecting = 0;
    torrentfs_live_peers(tfs, &live, &peak, &connecting);
    int sockFail = 0, connTimeouts = 0;
    torrentfs_fail_kinds(tfs, &sockFail, &connTimeouts);
    int claiming = 0, idleUnchoked = 0;
    torrentfs_claim_stats(tfs, &claiming, &idleUnchoked);
    int bfEmpty = 0, bfOk = 0, bfBad = 0;
    torrentfs_bitfield_stats(tfs, &bfEmpty, &bfOk, &bfBad);
    int64_t wph = 0, wlo = 0, whi = 0;
    int claimFail = 0, claimOk = 0, inflight = 0;
    torrentfs_claim_debug(tfs, &wph, &wlo, &whi, &claimFail, &claimOk, &inflight);
    int cacheWrFail = 0, cacheRdShort = 0;
    int64_t cacheTotal = 0;
    torrentfs_cache_stats(tfs, &cacheWrFail, &cacheRdShort, &cacheTotal);
    char lastErr[128] = { 0 };
    torrentfs_last_err(tfs, lastErr, sizeof(lastErr));
    int64_t plen = torrentfs_piece_len(tfs);

    // Playback / decode stats. If "Dropped" climbs during the stutters it's the
    // decoder/CPU that can't keep up; if it stays flat with a full buffer, the
    // hitch is frame pacing/judder, not dropped frames.
    double buf = 0.0, fps = 0.0;
    int64_t w = 0, h = 0, drop = 0, decDrop = 0;
    char* hwdec = nullptr;
    if (mpv)
    {
        mpv_get_property(mpv, "demuxer-cache-duration", MPV_FORMAT_DOUBLE, &buf);
        mpv_get_property(mpv, "width", MPV_FORMAT_INT64, &w);
        mpv_get_property(mpv, "height", MPV_FORMAT_INT64, &h);
        mpv_get_property(mpv, "container-fps", MPV_FORMAT_DOUBLE, &fps);
        mpv_get_property(mpv, "frame-drop-count", MPV_FORMAT_INT64, &drop);
        mpv_get_property(mpv, "decoder-frame-drop-count", MPV_FORMAT_INT64,
                        &decDrop);
        mpv_get_property(mpv, "hwdec-current", MPV_FORMAT_STRING, &hwdec);
    }
    const char* hw =
        (hwdec && hwdec[0] && std::strcmp(hwdec, "no") != 0) ? hwdec : "software";

    // Video buffered in RAM: mpv's demuxer forward cache. Read out of the
    // demuxer-cache-state node's total-bytes field.
    int64_t ramBytes = 0;
    if (mpv)
    {
        mpv_node node;
        if (mpv_get_property(mpv, "demuxer-cache-state", MPV_FORMAT_NODE, &node) >= 0)
        {
            if (node.format == MPV_FORMAT_NODE_MAP && node.u.list)
                for (int i = 0; i < node.u.list->num; i++)
                    if (std::strcmp(node.u.list->keys[i], "total-bytes") == 0 &&
                        node.u.list->values[i].format == MPV_FORMAT_INT64)
                        ramBytes = node.u.list->values[i].u.int64;
            mpv_free_node_contents(&node);
        }
    }

    // SD-card write rate: how fast verified pieces are landing in the cache.
    // Differentiates the engine's written-bytes counter -- NOT cacheTotal,
    // which is the (constant) size the cache must reach, whose delta showed a
    // permanent 0 here.
    int64_t cacheWritten = torrentfs_cache_written(tfs);
    double sdWrite = (double)(cacheWritten - infoLastCache) / dt;
    if (sdWrite < 0) sdWrite = 0;
    infoLastCache  = cacheWritten;

    // Syscall probes: calls/s (delta of the cumulative counts) plus the worst
    // single call since the last refresh. During a console-wide freeze the
    // panel cannot redraw, but the engine keeps recording -- the first refresh
    // after the freeze shows which call class was stuck, i.e. which OS service
    // (bsd or fs) the console was waiting on.
    uint32_t latN[5];
    uint64_t latUs[5];
    torrentfs_lat_stats(tfs, latN, latUs);
    unsigned latRate[5];
    unsigned long long latMs[5];
    for (int i = 0; i < 5; i++)
    {
        latRate[i]        = (unsigned)((latN[i] - infoLastLatN[i]) / (dt > 0 ? dt : 1));
        infoLastLatN[i]   = latN[i];
        latMs[i]          = latUs[i] / 1000;
    }

    // Thread heartbeats: ms since each engine thread last ran, @ the core it
    // ran on. A freeze with every syscall probe flat (poll/recv/send/sd all
    // low) is a *scheduling* stall, not an OS-service stall -- and only these
    // ages show it: the stalled thread's age balloons while survivors stay near
    // 0, and the cores say who starved whom. rd/wr legitimately idle when the
    // buffer is full (nothing to read/write), so watch net and ui.
    uint32_t hbAge[4];
    int hbCore[4];
    torrentfs_heartbeats(tfs, hbAge, hbCore);

    // Left column: everything about the swarm. Right column: what came out of
    // it -- the video, and the source it is being read from.
    // In RAM mode nothing hits the card, so the "written" rate is a RAM-store
    // rate: label it as such rather than "SD write", which read like disk I/O.
    const char* storeLabel = torrentfs_ram_active(tfs) ? "RAM store" : "SD write";
    char text[2048];
    std::snprintf(text, sizeof(text),
                  "NETWORK\n"
                  "Peers: %d (+%d incoming)\n"
                  "Speed: %.2f MB/s\n"
                  "Downloaded: %.1f MB\n"
                  "Pieces: %lld / %lld (%d%%)  piece=%lld KB\n"
                  "Buffer: %.1f s\n"
                  "\n"
                  "WORKERS\n"
                  "live now: %d   peak: %d   connecting: %d\n"
                  "downloading: %d   idle unchoked: %d\n"
                  "bitfield: %d empty   %d ok   %d bad\n"
                  "claim: ph=%lld win=[%lld,%lld)  ok=%d fail=%d\n"
                  "inflight pieces: %d\n"
                  "cache: %.2f GB  wr fail: %d  rd short: %d\n"
                  "%s: %.2f MB/s\n"
                  "sys calls/s | max ms (calm=%d):\n"
                  "poll %u|%llu  recv %u|%llu  send %u|%llu\n"
                  "sd wr %u|%llu  sd rd %u|%llu\n"
                  "hb(ms@core) net %u@%d wr %u@%d rd %u@%d ui %u@%d\n"
                  "conn ok/fail: %d / %d\n"
                  "fail: %d sock/local   %d timeout\n"
                  "unchoked: %d   choked: %d\n"
                  "piece ok: %d   fetch fail: %d   sha fail: %d\n"
                  "up: %d blocks (%d interested, %d req)\n"
                  "last err: %s",
                  peers, incoming, infoSpeed / (1024.0 * 1024.0),
                  (double)bytes / (1024.0 * 1024.0), (long long)done,
                  (long long)total, pct, (long long)(plen / 1024), buf,
                  live, peak, connecting,
                  claiming, idleUnchoked,
                  bfEmpty, bfOk, bfBad,
                  (long long)wph, (long long)wlo, (long long)whi, claimOk, claimFail,
                  inflight,
                  (double)cacheTotal / (1024.0 * 1024.0 * 1024.0), cacheWrFail,
                  cacheRdShort,
                  storeLabel, sdWrite / (1024.0 * 1024.0),
                  torrentfs_calm(tfs),
                  latRate[0], latMs[0], latRate[1], latMs[1],
                  latRate[2], latMs[2],
                  latRate[3], latMs[3], latRate[4], latMs[4],
                  hbAge[0], hbCore[0], hbAge[1], hbCore[1],
                  hbAge[2], hbCore[2], hbAge[3], hbCore[3],
                  c[0], c[1],
                  sockFail, connTimeouts,
                  c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9],
                  lastErr[0] ? lastErr : "-");
    infoLabel->setText(text);

    // HEADER: the critical head/tail pieces mpv needs before playback -- the
    // ones fetched during "Downloading header..." (the container's moov atom
    // lives at both ends). Shows each one's live status so a stalled start is
    // visible. Counts mirror the engine's CRIT_HEAD / CRIT_TAIL; wlo is the
    // file's first absolute piece, so file-relative index i maps to wlo + i.
    auto pieceStat = [&](int64_t idx) -> std::string {
        int st = -1, have = 0, req = 0, nbtot = 0;
        torrentfs_piece_debug(tfs, idx, &st, &have, &req, &nbtot);
        switch (st)
        {
            case 2: return "ok";     // DONE
            case 3: return "wr";     // WRITING
            case 1:                  // ACTIVE: blocks in hand / total
            {
                char b[16];
                std::snprintf(b, sizeof(b), "%d/%d", have, nbtot);
                return b;
            }
            case 0: return "wait";   // NEEDED
            default: return "-";
        }
    };
    int critHead = 2, critTail = 3;
    torrentfs_crit(tfs, &critHead, &critTail);
    std::string hdr;
    char hb[64];
    for (int i = 0; i < critHead && i < total; i++)
    {
        std::snprintf(hb, sizeof(hb), "%s#%d %s", i ? "  " : "head ", i,
                      pieceStat(wlo + i).c_str());
        hdr += hb;
    }
    hdr += "\n";
    for (int i = 0; i < critTail; i++)
    {
        int64_t rel = total - critTail + i;
        if (rel < critHead) continue;   // tiny file: don't repeat the head
        std::snprintf(hb, sizeof(hb), "%s#%lld %s", i ? "  " : "tail ",
                      (long long)rel, pieceStat(wlo + rel).c_str());
        hdr += hb;
    }

    // DOWNLOADING: the pieces being assembled right now (claimed + in flight),
    // sorted by index, each with blocks-in-hand / total. File-relative indices
    // (idx - wlo) so they line up with the HEADER list above.
    int64_t aidx[16];
    int ahave[16], atot[16];
    int an = torrentfs_active_pieces(tfs, aidx, ahave, atot, 16);
    for (int i = 0; i < an; i++)          // insertion sort by index (<=16)
        for (int j = i + 1; j < an; j++)
            if (aidx[j] < aidx[i])
            {
                int64_t ti = aidx[i]; aidx[i] = aidx[j]; aidx[j] = ti;
                int th = ahave[i]; ahave[i] = ahave[j]; ahave[j] = th;
                int tt = atot[i]; atot[i] = atot[j]; atot[j] = tt;
            }
    std::string dl;
    char db[48];
    for (int i = 0; i < an; i++)
    {
        std::snprintf(db, sizeof(db), "%s#%lld %d/%d", i ? "  " : "",
                      (long long)(aidx[i] - wlo), ahave[i], atot[i]);
        dl += db;
    }
    if (dl.empty()) dl = "-";

    char right[1536];
    std::snprintf(right, sizeof(right),
                  "VIDEO\n"
                  "%lldx%lld @ %.3f fps\n"
                  "Decode: %s\n"
                  "Dropped: %lld (decoder %lld)\n"
                  "RAM buffer: %.0f MB\n"
                  "\n"
                  "SOURCE\n"
                  "%s\n"
                  "%.2f GB  %lld pieces of %lld KB\n"
                  "\n"
                  "METADATA\n"
                  "Peers tried: %d / %d\n"
                  "\n"
                  "HEADER\n"
                  "%s\n"
                  "\n"
                  "DOWNLOADING\n"
                  "%s",
                  (long long)w, (long long)h, fps,
                  hw, (long long)drop, (long long)decDrop,
                  (double)ramBytes / (1024.0 * 1024.0),
                  torrentfs_name(tfs),
                  (double)torrentfs_size(tfs) / (1024.0 * 1024.0 * 1024.0),
                  (long long)total, (long long)(plen / 1024),
                  torrent_meta_peers_tried, torrent_meta_peers_total,
                  hdr.c_str(), dl.c_str());
    if (infoLabel2) infoLabel2->setText(right);

    if (hwdec)
        mpv_free(hwdec);
}

void MpvView::logStats()
{
    if (!tfs)
        return;

    auto now = std::chrono::steady_clock::now();
    if (!statsStarted)
    {
        statsStarted    = true;
        statsStart      = now;
        statsLastSample = now;
        statsLastBytes  = torrentfs_bytes_recv(tfs);
        return;
    }
    double dt = std::chrono::duration<double>(now - statsLastSample).count();
    if (dt < 2.0)
        return;

    // Piggyback on this 2 s cadence: keep the last known playback position for
    // the Stremio watch-state sync, and push it out every couple of minutes.
    if (ready && mpv)
    {
        // Async-observed (pumpEvents): this runs on the UI thread, which must
        // not block on the mpv core.
        if (obsPos > 0) lastPosSec = obsPos;
        if (obsDur > 0) lastDurSec = obsDur;
        maybePushWatchState(false);
    }

    int64_t bytes  = torrentfs_bytes_recv(tfs);
    int64_t stored = torrentfs_stored_bytes(tfs);
    double kbps    = (double)(bytes - statsLastBytes) / dt / 1024.0;
    statsLastBytes = bytes;
    statsLastSample = now;
    int elapsed = (int)std::chrono::duration<double>(now - statsStart).count();

    int64_t done = 0, total = 0, ph = 0;
    torrentfs_stats(tfs, &done, &total, &ph);
    int live = 0, peak = 0, connecting = 0;
    torrentfs_live_peers(tfs, &live, &peak, &connecting);
    int claiming = 0, idle = 0;
    torrentfs_claim_stats(tfs, &claiming, &idle);
    int bfEmpty = 0, bfOk = 0, bfBad = 0;
    torrentfs_bitfield_stats(tfs, &bfEmpty, &bfOk, &bfBad);
    int64_t wph = 0, wlo = 0, whi = 0;
    int cFail = 0, cOk = 0, infl = 0;
    torrentfs_claim_debug(tfs, &wph, &wlo, &whi, &cFail, &cOk, &infl);
    int wrFail = 0, rdShort = 0;
    int64_t cacheTotal = 0;
    torrentfs_cache_stats(tfs, &wrFail, &rdShort, &cacheTotal);
    int sockFail = 0, tmo = 0;
    torrentfs_fail_kinds(tfs, &sockFail, &tmo);
    int c[10] = { 0 };
    torrentfs_debug_counts(tfs, c);
    char lastErr[128] = { 0 };
    torrentfs_last_err(tfs, lastErr, sizeof(lastErr));

    double buf = obsCacheSecs;  // async-observed; no sync mpv call here

    // The piece the player is blocked on: when ph freezes, this says why.
    int pst = -1, phv = 0, prq = 0, ptot = 0;
    torrentfs_piece_debug(tfs, ph, &pst, &phv, &prq, &ptot);
    static const char* kSt[] = { "NEEDED", "INFLIGHT", "DONE", "VERIFYING" };
    const char* pstName = (pst >= 0 && pst <= 3) ? kSt[pst] : "?";

    // Syscall-latency peaks since the last sample: during a console freeze
    // these say which OS service stalled (bsd for poll/recv/send, fs for
    // w/rd) and for how long. Reading clears the peaks, which the ZR panel
    // also does -- with the panel open the two readers split the values, so
    // trust the log only when the panel is closed.
    uint32_t latN[5];
    uint64_t latUs[5];
    torrentfs_lat_stats(tfs, latN, latUs);
    // Calls per interval too (cumulative counts, so panel reads don't skew
    // them): distinguishes "netloop starved" (few calls) from "idle" (few
    // calls, but nothing pending) vs "busy and fast" (many calls, low max).
    uint32_t latC[5];
    for (int i = 0; i < 5; i++)
    {
        latC[i]          = latN[i] - statsLastLatN[i];
        statsLastLatN[i] = latN[i];
    }

    // Thread heartbeats: age (ms since the thread last ran) @ the core it
    // last ran on. During a freeze the stalled threads' ages balloon while
    // the survivors stay near 0 -- and the cores say who shares a starving
    // core with whom. Note: the stats thread itself must be alive to log, so
    // the first line AFTER a freeze carries the peak ages.
    uint32_t hbAge[4];
    int hbCore[4];
    torrentfs_heartbeats(tfs, hbAge, hbCore);

    // One line per sample, fixed field order, so the whole run can be read as a
    // trend (and grepped) instead of guessed at from a screenshot.
    brls::Logger::info(
        "[stats] t={}s spd={:.0f}KB/s dl={:.1f}MB stored={:.1f}MB dup={:.1f}MB "
        "pc={}/{} buf={:.1f}s | "
        "live={} peak={} conn={} dling={} idle={} | bf={}/{}/{} | "
        "claim ph={} win=[{},{}) ok={} fail={} infl={} | "
        "PH-PIECE {} have={}/{} req={} | "
        "cache={:.2f}GB wr_fail={} rd_short={} | "
        "conn_ok={} conn_fail={} sock={} tmo={} | "
        "unchoked={} pok={} ffail={} sha={} | up={} int={} req={} | "
        "lat p={}:{} r={}:{} s={}:{} w={}:{} rd={}:{} maxms:calls | "
        "hb net={}@{} wr={}@{} rd={}@{} ui={}@{} ms@core | "
        "calm={} | err={}",
        elapsed, kbps, (double)bytes / (1024.0 * 1024.0),
        (double)stored / (1024.0 * 1024.0),
        (double)(bytes - stored) / (1024.0 * 1024.0), (long long)done,
        (long long)total, buf,
        live, peak, connecting, claiming, idle,
        bfEmpty, bfOk, bfBad,
        (long long)wph, (long long)wlo, (long long)whi, cOk, cFail, infl,
        pstName, phv, ptot, prq,
        (double)cacheTotal / (1024.0 * 1024.0 * 1024.0), wrFail, rdShort,
        c[0], c[1], sockFail, tmo,
        c[2], c[4], c[5], c[6], c[7], c[8], c[9],
        (unsigned long long)(latUs[0] / 1000), latC[0],
        (unsigned long long)(latUs[1] / 1000), latC[1],
        (unsigned long long)(latUs[2] / 1000), latC[2],
        (unsigned long long)(latUs[3] / 1000), latC[3],
        (unsigned long long)(latUs[4] / 1000), latC[4],
        hbAge[0], hbCore[0], hbAge[1], hbCore[1],
        hbAge[2], hbCore[2], hbAge[3], hbCore[3],
        torrentfs_calm(tfs),
        lastErr[0] ? lastErr : "-");
}

// Reports the watched position to the Stremio API. Rate-limited to one push
// every 2 minutes unless `force` (teardown). Below ~30 s of playback nothing is
// sent: opening a stream and backing out should not rewrite the account's
// "continue watching" position.
void MpvView::maybePushWatchState(bool force)
{
    if (watch.authKey.empty() || watch.itemId.empty())
        return;

    // Finished an episode that has a next one -- reached EOF, or stopped within
    // the last 5% (people press B over the credits instead of waiting for EOF,
    // and >=95% is exactly what Continue Watching filters out). Advance the
    // library pointer to the next episode instead of recording this one at
    // ~100%, which would push the show out of Continue Watching. A ~1% offset
    // keeps it inside the window; resumeFrom() ignores anything under 60 s, so
    // opening it still starts from the beginning.
    bool finished = ended ||
                    (lastDurSec > 0.0 && lastPosSec >= lastDurSec * 0.95);
    if (force && finished && !watch.nextVideoId.empty())
    {
        double dur = lastDurSec > 0.0 ? lastDurSec : 2400.0;
        double off = dur * 0.01;
        if (off > 55.0) off = 55.0;
        stremio::pushWatchStateAsync(watch.authKey, watch.itemId,
                                     watch.nextVideoId, off, dur);
        return;
    }

    if (lastPosSec < 30.0)
        return;
    auto now = std::chrono::steady_clock::now();
    if (!force && watchPushValid &&
        std::chrono::duration<double>(now - watchPushLast).count() < 120.0)
        return;
    watchPushLast  = now;
    watchPushValid = true;
    stremio::pushWatchStateAsync(watch.authKey, watch.itemId, watch.videoId,
                                 lastPosSec, lastDurSec);
}

// Enter scrub mode: pause and pin the cursor to where playback currently is.
void MpvView::beginSeek()
{
    seeking = true;
    // Async-observed values: even this one-shot must not block the UI thread
    // if the user scrubs during an mpv hiccup.
    seekDur    = obsDur;
    seekTarget = obsPos;
    mpv_set_property_string(mpv, "pause", "yes");
    if (pauseOverlay) pauseOverlay->setVisibility(brls::Visibility::VISIBLE);
    if (seekOverlay) seekOverlay->setVisibility(brls::Visibility::VISIBLE);
    seekHeld       = 0.0;
    seekFrameValid = false;
}

// Analog scrubbing, driven every frame from the stick's X axis rather than from
// button events: only the axis carries how far it is pushed, which is what makes
// a slow nudge and a fast sweep the same gesture.
void MpvView::updateStickSeek()
{
    if (!ready || !mpv)
        return;
    // The settings panel has the cursor, but the stick is polled straight off
    // the controller rather than routed through focus -- so without this it
    // would go on scrubbing the video from under an open menu.
    if (settingsOpen)
        return;

    brls::ControllerState st {};
    brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&st);
    // Either stick scrubs -- take whichever is pushed further from centre, so the
    // right Joy-Con's stick works the same as the left one.
    float lx = st.axes[brls::ControllerAxis::LEFT_X];
    float rx = st.axes[brls::ControllerAxis::RIGHT_X];
    float x  = (lx < 0 ? -lx : lx) >= (rx < 0 ? -rx : rx) ? lx : rx;

    bool pushed = x <= -kStickDeadzone || x >= kStickDeadzone;

    // Locked: the stick is a blocked input like any other, so it flashes the pill
    // -- which it used to skip entirely, since the lock check sat above the axis
    // read. Once per push, not once per frame: every other control flashes on a
    // press, and re-arming the timer every frame would pin the pill on screen for
    // as long as the stick is held.
    if (controlsLocked)
    {
        if (pushed && !stickWasPushed) flashLock();
        stickWasPushed = pushed;
        return;
    }
    stickWasPushed = pushed;

    auto now = std::chrono::steady_clock::now();
    double dt = seekFrameValid
                    ? std::chrono::duration<double>(now - seekLastFrame).count()
                    : 0.0;
    seekLastFrame  = now;
    seekFrameValid = true;
    if (dt > 0.25) dt = 0.25;  // a hitch must not teleport the cursor

    if (!pushed)
    {
        seekHeld = 0.0;  // released: next push starts slow again
        return;
    }

    if (!seeking)
        beginSeek();

    seekHeld += dt;
    double ramp = 1.0 + (kSeekAccelMax - 1.0) *
                            (seekHeld < kSeekAccelSecs ? seekHeld / kSeekAccelSecs : 1.0);
    // Square the tilt: fine control near centre, full speed at the edge.
    double tilt = (double)x;
    double rate = tilt * (tilt < 0 ? -tilt : tilt) * kSeekBaseRate * ramp;

    seekTarget += rate * dt;
    if (seekTarget < 0.0) seekTarget = 0.0;
    if (seekDur > 0.0 && seekTarget > seekDur) seekTarget = seekDur;
}

void MpvView::updateSeekBar()
{
    if (!seekOverlay || !mpv)
        return;
    // Async-observed (pumpEvents): the seek bar refreshes every frame and
    // must never block on the mpv core.
    double pos = obsPos, dur = obsDur;

    if (seeking && seekDur > 0.0)
        dur = seekDur;

    // Two different things, so two different values -- driving both from one
    // number made the played-progress bar jump around with the stick, claiming
    // we had watched up to wherever the cursor happened to point.
    //   fill   = what has actually been played (time-pos). Paused while
    //            scrubbing, so it correctly stays put.
    //   cursor = where the scrub is aiming; equals playback when not scrubbing.
    double cursorAt = seeking ? seekTarget : pos;

    auto frac = [dur](double t) {
        if (dur <= 0.0) return 0.0f;
        float f = (float)(t / dur);
        return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    };

    // The timer follows the cursor: while scrubbing it is the target time that
    // matters, not the frozen playback clock.
    seekCur->setText(fmtTime(cursorAt));
    seekTotal->setText(fmtTime(dur));

    // Place both in pixels off the same measured width. As percentages they
    // drifted apart the further along the bar you got: yoga resolves a child's
    // width% against the parent's content box but an absolute child's left%
    // against its padding box, so the two scales diverge and the fill ran past
    // the cursor. One width, one unit, no room to disagree.
    float trackW = seekTrack ? seekTrack->getWidth() : 0.0f;
    if (trackW <= 0.0f)
        return;  // not laid out yet; next frame will have it

    float cw = seekCursor ? seekCursor->getWidth() : 0.0f;
    seekFill->setWidth(trackW * frac(pos));
    if (seekCursor)
    {
        // Keep the cursor inside the track at both ends rather than hanging off.
        float x = trackW * frac(cursorAt) - cw / 2.0f;
        if (x < 0.0f) x = 0.0f;
        if (x > trackW - cw) x = trackW - cw;
        seekCursor->setPositionLeft(x);
    }
}

void MpvView::draw(NVGcontext* vg, float x, float y, float width, float height,
                   brls::Style style, brls::FrameContext* ctx)
{
    pumpEvents();  // also feeds the engine's backlog (async property observe)
    if (tfs)
        torrentfs_hb_ui(tfs);  // render-thread heartbeat (ui age in the ZR panel)

    // A touch on the screen flips borealis into TOUCH input mode, and the first
    // gamepad press after that is consumed just to switch back -- so A took two
    // presses to pause/resume once the screen had been tapped. Nothing here uses
    // touch, so keep it in GAMEPAD mode; the next A then acts on the first press.
    if (brls::Application::getInputType() == brls::InputType::TOUCH)
        brls::Application::setInputType(brls::InputType::GAMEPAD);

    // No render context yet: the engine is still opening (a magnet spends up to
    // a minute finding a peer that will serve its metadata). There is no video
    // to composite, but the loading screen still has to be drawn -- returning
    // here left the whole view blank, which is exactly the black screen the
    // async open was supposed to fix.
    if (!renderCtx)
    {
        updateLoadingOverlay();
        // This IS the metadata-fetch / engine-opening phase -- the one most
        // likely to hang -- so the ZR panel has to be fed here too. The normal
        // update() call is past the early return below and never ran during it,
        // which left the toggled-on overlay an empty transparent box.
        updateInfoOverlay();
        brls::Box::draw(vg, x, y, width, height, style, ctx);
        return;
    }

    // nanovg is a *batched* renderer: every nvg* call in this frame (including
    // this view's opaque black background drawn by View::frame just before us,
    // and any lower view) is only submitted to GL at the outer nvgEndFrame --
    // which runs AFTER this draw(). mpv_render_context_render() below draws
    // *immediately*, so without this split the queued black background gets
    // flushed on top of the video -> black screen with working audio, and the
    // video only flashes through on the pop transition when that fill is gone.
    //
    // Flush everything queued so far now (paints the black letterbox UNDER the
    // video), render mpv, then restart the nanovg frame so the red overlay and
    // borealis' outer nvgEndFrame composite ON TOP of the video.
    nvgEndFrame(vg);

    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);

    mpv_opengl_fbo mfbo;
    mfbo.fbo              = fbo;
    mfbo.w               = (int)brls::Application::windowWidth;
    mfbo.h               = (int)brls::Application::windowHeight;
    mfbo.internal_format = 0;
    int flip             = 1;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &mfbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip },
        { MPV_RENDER_PARAM_INVALID, nullptr },
    };

    // borealis/nanovg leaves scissor/stencil/blend/colormask enabled from the
    // UI pass; mpv's render honors them and its output gets clipped away (black).
    // Reset to a clean full-screen state before handing the framebuffer to mpv.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, brls::Application::windowWidth, brls::Application::windowHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    mpv_render_context_render(renderCtx, params);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, brls::Application::windowWidth, brls::Application::windowHeight);

    // The Switch compositor honors the framebuffer's alpha channel. mpv leaves
    // the video pixels with alpha 0 -> they composite as transparent (black).
    // Force the whole framebuffer opaque by clearing only the alpha channel to 1
    // (RGB / the video is left untouched via the color mask).
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    mpv_render_context_report_swap(renderCtx);

    // Resume the nanovg frame that we flushed above, matching the parameters
    // Application::frame() opened it with, so the loading overlay (and the outer
    // nvgEndFrame) draw on top of the video instead of under it.
    double scaleFactor =
        brls::Application::getPlatform()->getVideoContext()->getScaleFactor();
    nvgBeginFrame(vg, brls::Application::windowWidth,
                  brls::Application::windowHeight, (float)scaleFactor);
    nvgScale(vg, brls::Application::windowScale, brls::Application::windowScale);

    // Loading screen: keep it fed until the first frame is up, then take it down
    // once so it stops laying out. Drawn via Box::draw (our child views) on top
    // of the freshly rendered video.
    if (!ready)
        updateLoadingOverlay();
    if (ready && !overlayHidden && loadingOverlay)
    {
        // Buffered enough: start playback and take the loading screen down.
        if (mpv)
            mpv_set_property_string(mpv, "pause", "no");
        loadingOverlay->setVisibility(brls::Visibility::GONE);
        overlayHidden = true;
    }
    if (overlayHidden)
        updateBufferIndicator();
    if (ready)
        updateStickSeek();  // must run every frame: the stick is an axis, not an event
    if (controlsShown || seeking)
        updateSeekBar();
    updateControlsAutoHide();  // hide the overlay after a few seconds of playback
    updateLockHint();  // fades the lock flash out after its moment
    updatePill();          // ... and the L/R readout after its own
    updateNextCard();      // "next episode", over the last seconds
    updateInfoOverlay();
    logStats();  // always, even with the ZR panel closed

    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

PlayerActivity::PlayerActivity(std::string source, PlayerArt art,
                               std::string title, int fileIndex,
                               WatchInfo watch)
    : source(std::move(source))
    , art(std::move(art))
    , title(std::move(title))
    , fileIndex(fileIndex)
    , watch(std::move(watch))
{
}

brls::View* PlayerActivity::createContentView()
{
    return new MpvView(source, art, title, fileIndex, watch);
}
