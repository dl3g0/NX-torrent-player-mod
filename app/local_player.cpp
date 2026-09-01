#include "local_player.hpp"

#include <EGL/egl.h>
#include <glad/glad.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <switch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fmt/core.h>

#include "appdata.hpp"
#include "config.hpp"
#include "i18n.hpp"
#include "sys.hpp"
#include "theme.hpp"

namespace
{

void* getProcAddress(void* ctx, const char* name)
{
    (void)ctx;
    return (void*)eglGetProcAddress(name);
}

constexpr float  kStickDeadzone = 0.20f;
constexpr double kSeekBaseRate  = 20.0;
constexpr double kSeekAccelSecs =  3.0;
constexpr double kSeekAccelMax  =  6.0;

const char* const kAudioBoostFilter =
    "dynaudnorm=f=150:g=15:m=10:p=0.95:r=0.9:b=1";

bool audioBoostWanted()
{
    if (!config::get().audioBoost)
        return false;
#if defined(__SWITCH__)
    return appletGetOperationMode() == AppletOperationMode_Handheld;
#else
    return true;
#endif
}

class PlayPauseButton : public brls::Box
{
  public:
    const bool* paused = nullptr;

    PlayPauseButton()
    {
        this->setFocusable(false);
        this->setCornerRadius(70.0f);
        this->setDimensions(140.0f, 140.0f);
        this->setBackgroundColor(nvgRGBA(0, 0, 0, 90));
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override
    {
        brls::Box::draw(vg, x, y, width, height, style, ctx);

        nvgSave(vg);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 235));

        float cx = x + width * 0.5f;
        float cy = y + height * 0.5f;

        if (paused && *paused)
        {
            const float r = 26.0f;
            const float nudgeX = 4.0f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - r * 0.6f + nudgeX, cy - r);
            nvgLineTo(vg, cx + r * 1.1f + nudgeX, cy);
            nvgLineTo(vg, cx - r * 0.6f + nudgeX, cy + r);
            nvgClosePath(vg);
            nvgFill(vg);
        }
        else
        {
            const float bw = 13.0f, bh = 52.0f, gap = 16.0f;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx - gap / 2 - bw, cy - bh / 2, bw, bh, 3.0f);
            nvgRoundedRect(vg, cx + gap / 2, cy - bh / 2, bw, bh, 3.0f);
            nvgFill(vg);
        }
        nvgRestore(vg);
    }
};

const char* const kGlyphLockOpen   = "\xEE\xA0\x97";  // Material Icons lock_open
const char* const kGlyphLockClosed = "\xEE\xA0\x96";  // Material Icons lock
const std::vector<std::string> kSpeedLabels = { "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x" };
const std::vector<double> kSpeeds = { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
constexpr float kPanelW = 560.0f;

std::string fmtTime(double s)
{
    if (s < 0 || s != s) s = 0;
    int t = (int)s, h = t / 3600, m = (t % 3600) / 60, sec = t % 60;
    char buf[16];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, sec);
    else
        std::snprintf(buf, sizeof(buf), "%d:%02d", m, sec);
    return buf;
}

std::string subDelayText(double d)
{
    if (d == 0.0) return tr("None");
    char b[32];
    std::snprintf(b, sizeof(b), "%+.2f s", d);
    return b;
}

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

class LocalSettingsActivity : public brls::Activity
{
  public:
    LocalSettingsActivity(brls::View* content, std::function<void()> onGone)
        : content(content), onGone(std::move(onGone))
    {
    }
    ~LocalSettingsActivity() override
    {
        if (onGone) onGone();
    }
    bool isTranslucent() override { return true; }
    brls::View* createContentView() override { return content; }

  private:
    brls::View* content;
    std::function<void()> onGone;
};

class LocalTrackCell : public brls::SelectorCell
{
  public:
    LocalTrackCell()
    {
        title->setShrink(0.0f);
        detail->setSingleLine(true);
        detail->setShrink(1.0f);
        detail->setMaxWidth(500.0f);
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

} // namespace

LocalMpvView::LocalMpvView(const std::string& filePath, const std::string& title)
    : filePath(filePath)
{
    this->setGrow(1.0f);
    this->setBackgroundColor(nvgRGB(0, 0, 0));
    this->setFocusable(true);
    this->setHideHighlight(true);

    if (!title.empty())
        displayTitle = title;
    else
    {
        size_t slash = filePath.find_last_of("/\\");
        displayTitle = (slash != std::string::npos) ? filePath.substr(slash + 1) : filePath;
    }

    sys::preventSleep(true);
    sys::setCpuBoost(true);

    buildLoadingOverlay(displayTitle);
    registerPlayerActions();

    auto liveFlag = this->alive;
    brls::sync([this, liveFlag]() {
        if (!*liveFlag) return;
        if (!startMpv() && statusLabel)
            statusLabel->setText(tr("Player initialisation failed"));
    });
}

LocalMpvView::~LocalMpvView()
{
    brls::Logger::info("[local_player] ~LocalMpvView teardown enter");
    sys::setCpuBoost(false);
    sys::preventSleep(false);
    appletSetMediaPlaybackState(false);

    *alive = false;

    if (renderCtx)
    {
        mpv_render_context_free(renderCtx);
        renderCtx = nullptr;
    }
    if (mpv)
    {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
    brls::Logger::info("[local_player] ~LocalMpvView teardown complete");
}

bool LocalMpvView::startMpv()
{
    mpv = mpv_create();
    if (!mpv)
    {
        brls::Logger::error("mpv_create failed in local player");
        return false;
    }

    mpv_set_option_string(mpv, "vo", "libmpv");

    bool hwDec = config::get().hwDecode;
    mpv_set_option_string(mpv, "hwdec", hwDec ? "auto" : "no");
    if (hwDec)
        mpv_set_option_string(mpv, "hwdec-extra-frames", "32");

#if defined(__SWITCH__)
    mpv_set_option_string(mpv, "config", "yes");
    mpv_set_option_string(mpv, "config-dir", APPDATA_DIR);
#else
    mpv_set_option_string(mpv, "config", "no");
#endif
    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "audio-channels", "stereo");
    mpv_set_option_string(mpv, "audio-normalize-downmix", "no");
    mpv_set_option_string(mpv, "audio-stream-silence", "yes");
    mpv_set_option_string(mpv, "audio-wait-open", "0");
    mpv_set_option_string(mpv, "audio-pitch-correction", "yes");
    mpv_set_option_string(mpv, "audio-fallback-to-null", "yes");

    boostApplied = audioBoostWanted();
    mpv_set_option_string(mpv, "af", boostApplied ? kAudioBoostFilter : "");

    std::string alang = config::mpvLangList(config::get().audioLang);
    if (!alang.empty()) mpv_set_option_string(mpv, "alang", alang.c_str());
    bool subOn = config::get().subtitles;
    std::string slang = config::mpvLangList(config::get().subLang);
    if (!slang.empty()) mpv_set_option_string(mpv, "slang", slang.c_str());
    mpv_set_option_string(mpv, "sid", "auto");
    mpv_set_option_string(mpv, "sub-visibility", subOn ? "yes" : "no");
    mpv_set_option_string(mpv, "sub-auto", "auto");

    mpv_set_option_string(mpv, "sub-ass", "yes");
    mpv_set_option_string(mpv, "sub-font-provider", "none");
    mpv_set_option_string(mpv, "sub-font", "sans-serif");
    mpv_set_option_string(mpv, "sub-font-size", "46");
    mpv_set_option_string(mpv, "sub-bold", "yes");
    mpv_set_option_string(mpv, "sub-color", "#FFFFFF");
    mpv_set_option_string(mpv, "sub-border-color", "#000000");
    mpv_set_option_string(mpv, "sub-border-size", "2.6");
    mpv_set_option_string(mpv, "sub-shadow-color", "#000000");
    mpv_set_option_string(mpv, "sub-shadow-offset", "1.4");
    mpv_set_option_string(mpv, "sub-blur", "0.35");
    mpv_set_option_string(mpv, "sub-spacing", "0.4");
    mpv_set_option_string(mpv, "sub-margin-y", "50");

    mpv_set_option_string(mpv, "cache", "yes");
    mpv_set_option_string(mpv, "cache-pause", "no");
    mpv_set_option_string(mpv, "pause", "yes");
    mpv_set_option_string(mpv, "hr-seek", "default");
    mpv_set_option_string(mpv, "hr-seek-framedrop", "yes");
    mpv_set_option_string(mpv, "framedrop", "vo");
    mpv_set_option_string(mpv, "opengl-glfinish", "yes");
    mpv_set_option_string(mpv, "vd-lavc-dr", "no");

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

    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "core-idle", MPV_FORMAT_FLAG);

    const char* cmd[] = { "loadfile", filePath.c_str(), nullptr };
    mpv_command(mpv, cmd);
    brls::Logger::info("[local_player] loadfile issued: {}", filePath);
    return true;
}

void LocalMpvView::buildLoadingOverlay(const std::string& title)
{
    brls::Theme theme = brls::Application::getTheme();

    loadingOverlay = new brls::Box();
    loadingOverlay->setAxis(brls::Axis::COLUMN);
    loadingOverlay->setGrow(1.0f);
    loadingOverlay->setBackgroundColor(theme.getColor("brls/background"));

    auto* column = new brls::Box();
    column->setAxis(brls::Axis::COLUMN);
    column->setJustifyContent(brls::JustifyContent::CENTER);
    column->setAlignItems(brls::AlignItems::CENTER);
    column->setGrow(1.0f);
    column->setPadding(0, 60, 0, 60);
    loadingOverlay->addView(column);

    auto* logo = new brls::Image();
    logo->setImageFromRes("video-icon.png");
    logo->setDimensions(148.0f, 148.0f);
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
    statusLabel->setText(tr("Opening file..."));
    statusLabel->setFontSize(21);
    statusLabel->setTextColor(theme.getColor("brls/text"));
    statusLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    column->addView(statusLabel);

    this->addView(loadingOverlay);

    // Centre play/pause button
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
        btn->paused = &userPaused;
        btn->setDimensions(140.0f, 140.0f);
        btn->setCornerRadius(70.0f);
        btn->setBackgroundColor(nvgRGBA(0, 0, 0, 90));
        btn->addGestureRecognizer(
            new brls::TapGestureRecognizer(btn, [this]() {
                if (controlsLocked) { flashLock(); return; }
                onPlayPause();
            }));
        pauseOverlay->addView(btn);
    }
    this->addView(pauseOverlay);

    // Title, top-left pill
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
        tl->setText(title);
        tl->setFontSize(26.0f);
        tl->setTextColor(nvgRGB(255, 255, 255));
        tl->setSingleLine(true);
        tl->setMaxWidth(760.0f);
        pauseTitleBox->addView(tl);
    }
    this->addView(pauseTitleBox);

    // Top-right cluster: Options + Lock
    auto* topRight = new brls::Box();
    topRight->setPositionType(brls::PositionType::ABSOLUTE);
    topRight->setPositionTop(48.0f);
    topRight->setPositionRight(60.0f);
    topRight->setAxis(brls::Axis::ROW);
    topRight->setAlignItems(brls::AlignItems::CENTER);
    this->addView(topRight);

    optionsHint = new brls::Box();
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
        x->setText("");  // U+E0E2 (X button glyph)
        x->setFontSize(26.0f);
        x->setTextColor(nvgRGB(255, 255, 255));
        x->setMargins(0.0f, 12.0f, 0.0f, 0.0f);
        optionsHint->addView(x);

        auto* gear = new brls::Label();
        gear->setText(""); // Material settings gear
        gear->setFontSize(26.0f);
        gear->setTextColor(nvgRGB(255, 255, 255));
        gear->setMargins(5.0f, 0.0f, 0.0f, 0.0f);
        optionsHint->addView(gear);
    }
    topRight->addView(optionsHint);

    lockHint = new brls::Box();
    lockHint->setHeight(56.0f);
    lockHint->setPadding(4.0f, 20.0f, 0.0f, 20.0f);
    lockHint->setCornerRadius(8.0f);
    lockHint->setBackgroundColor(nvgRGBA(0, 0, 0, 140));
    lockHint->setAxis(brls::Axis::ROW);
    lockHint->setAlignItems(brls::AlignItems::CENTER);
    lockHint->setVisibility(brls::Visibility::GONE);
    {
        auto* y = new brls::Label();
        y->setText("\xEE\x83\xA3");  // U+E0E3 (Y button glyph)
        y->setFontSize(26.0f);
        y->setTextColor(nvgRGB(255, 255, 255));
        y->setMargins(0.0f, 12.0f, 0.0f, 0.0f);
        lockHint->addView(y);

        lockLabel = new brls::Label();
        lockLabel->setText(kGlyphLockOpen);
        lockLabel->setFontSize(26.0f);
        lockLabel->setTextColor(nvgRGB(255, 255, 255));
        lockLabel->setMargins(4.0f, 0.0f, 0.0f, 0.0f);
        lockHint->addView(lockLabel);
    }
    topRight->addView(lockHint);

    // Pill top-center
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
            pillLabel = new brls::Label();
            pillLabel->setText("");
            pillLabel->setFontSize(24.0f);
            pillLabel->setTextColor(nvgRGB(255, 255, 255));
            hintPill->addView(pillLabel);
        }
        row->addView(hintPill);
        this->addView(row);
    }

    // Seek bar overlay bottom
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
    seekFill->setWidth(0.0f);
    seekFill->setCornerRadius(4.0f);
    seekFill->setBackgroundColor(theme.getColor("brls/accent"));
    seekTrack->addView(seekFill);

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
    seekTotal->setWidth(104.0f);
    seekTotal->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    seekTotal->setMargins(0, 0, 0, 20);
    seekOverlay->addView(seekTotal);
    this->addView(seekOverlay);

    // --- Touch gestures ---
    this->addGestureRecognizer(
        new brls::TapGestureRecognizer(this, [this]() {
            if (controlsLocked) { flashLock(); return; }
            if (ready) setControlsVisible(!controlsShown);
        }));

    seekOverlay->addGestureRecognizer(new brls::TapGestureRecognizer(
        [this](brls::TapGestureStatus status, brls::Sound*) {
            if (controlsLocked) { flashLock(); return; }
            if (status.state != brls::GestureState::END || !seekTrack)
                return;
            float w = seekTrack->getWidth();
            if (w > 0.0f)
                seekToFraction((status.position.x - seekTrack->getX()) / w);
        }));

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
                seekToFraction(frac);
            else
            {
                if (!seeking) beginSeek();
                seekTarget = frac * seekDur;
                setControlsVisible(true);
            }
        },
        brls::PanAxis::HORIZONTAL));

    if (optionsHint)
        optionsHint->addGestureRecognizer(new brls::TapGestureRecognizer(
            optionsHint, [this]() {
                if (controlsLocked) { flashLock(); return; }
                openTrackMenu();
            }));

    if (lockHint)
        lockHint->addGestureRecognizer(
            new brls::TapGestureRecognizer(lockHint, [this]() { toggleLock(); }));
}

void LocalMpvView::registerPlayerActions()
{
    this->registerAction(
        tr("Lock"), brls::BUTTON_Y,
        [this](brls::View*) {
            toggleLock();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    this->registerAction(
        tr("Back"), brls::BUTTON_B,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            if (seeking) { cancelSeek(); return true; }
            brls::Application::popActivity();
            return true;
        },
        false, false, brls::SOUND_BACK);

    this->registerAction(
        tr("Pause"), brls::BUTTON_A,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            onPlayPause();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    auto scrub = [this](double delta) {
        if (controlsLocked) { flashLock(); return true; }
        if (!ready || !mpv) return true;
        char v[32];
        std::snprintf(v, sizeof(v), "%.1f", delta);
        const char* cmd[] = { "seek", v, "relative", nullptr };
        mpv_command_async(mpv, 0, cmd);

        std::string text = delta > 0 ? ("+ " + std::to_string((int)delta) + "s")
                                     : ("- " + std::to_string((int)(-delta)) + "s");
        flashPill(text);
        return true;
    };

    this->registerAction(
        tr("Seek -"), brls::BUTTON_LEFT,
        [scrub](brls::View*) { return scrub(-10.0); },
        false, true, brls::SOUND_NONE);
    this->registerAction(
        tr("Seek +"), brls::BUTTON_RIGHT,
        [scrub](brls::View*) { return scrub(10.0); },
        false, true, brls::SOUND_NONE);

    auto speedStep = [this](int dir) {
        if (controlsLocked) { flashLock(); return true; }
        if (!ready || !mpv) return true;
        nudgeSpeed(dir);
        return true;
    };
    this->registerAction(
        tr("Slower"), brls::BUTTON_LB,
        [speedStep](brls::View*) { return speedStep(-1); },
        false, false, brls::SOUND_NONE);
    this->registerAction(
        tr("Faster"), brls::BUTTON_RB,
        [speedStep](brls::View*) { return speedStep(1); },
        false, false, brls::SOUND_NONE);

    this->registerAction(
        tr("Options"), brls::BUTTON_X,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            if (!ready || !mpv) return true;
            openTrackMenu();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    this->registerAction(
        tr("Info"), brls::BUTTON_RT,
        [this](brls::View*) {
            if (controlsLocked) { flashLock(); return true; }
            infoShown = !infoShown;
            if (infoOverlay) infoOverlay->setVisibility(infoShown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
            return true;
        },
        false, false, brls::SOUND_CLICK);
}

void LocalMpvView::onPlayPause()
{
    if (!ready || !mpv) return;

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

void LocalMpvView::setControlsVisible(bool show)
{
    controlsShown = show;
    brls::Visibility v = show ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (pauseOverlay) pauseOverlay->setVisibility(v);
    if (pauseTitleBox) pauseTitleBox->setVisibility(v);
    if (optionsHint) optionsHint->setVisibility(v);
    if (seekOverlay) seekOverlay->setVisibility(v);

    if (show && !userPaused)
        controlsHideAt = std::chrono::steady_clock::now() + std::chrono::seconds(4);
}

void LocalMpvView::updateControlsAutoHide()
{
    if (!controlsShown || userPaused || seeking) return;
    if (std::chrono::steady_clock::now() >= controlsHideAt)
        setControlsVisible(false);
}

void LocalMpvView::beginSeek()
{
    seeking = true;
    seekDur = obsDur;
    seekTarget = obsPos;
    mpv_set_property_string(mpv, "pause", "yes");
    if (pauseOverlay) pauseOverlay->setVisibility(brls::Visibility::VISIBLE);
    if (seekOverlay) seekOverlay->setVisibility(brls::Visibility::VISIBLE);
    seekHeld = 0.0;
    seekFrameValid = false;
}

void LocalMpvView::cancelSeek()
{
    if (!seeking) return;
    seeking = false;
    if (mpv)
        mpv_set_property_string(mpv, "pause", userPaused ? "yes" : "no");
    setControlsVisible(userPaused);
}

void LocalMpvView::updateStickSeek()
{
    if (!ready || !mpv || settingsOpen) return;

    brls::ControllerState st {};
    brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&st);
    float lx = st.axes[brls::ControllerAxis::LEFT_X];
    float rx = st.axes[brls::ControllerAxis::RIGHT_X];
    float x  = (std::abs(lx) >= std::abs(rx)) ? lx : rx;

    bool pushed = std::abs(x) >= kStickDeadzone;

    if (controlsLocked)
    {
        if (pushed && !stickWasPushed) flashLock();
        stickWasPushed = pushed;
        return;
    }
    stickWasPushed = pushed;

    auto now = std::chrono::steady_clock::now();
    double dt = seekFrameValid ? std::chrono::duration<double>(now - seekLastFrame).count() : 0.0;
    seekLastFrame  = now;
    seekFrameValid = true;
    if (dt > 0.25) dt = 0.25;

    if (!pushed)
    {
        seekHeld = 0.0;
        return;
    }

    seekHeld += dt;
    double ramp  = 1.0 + (kSeekAccelMax - 1.0) *
                   (seekHeld < kSeekAccelSecs ? seekHeld / kSeekAccelSecs : 1.0);
    double tilt  = (double)x;
    double delta = tilt * std::abs(tilt) * kSeekBaseRate * ramp * dt;

    if (std::abs(delta) < 0.05) return;   // too small to bother

    char v[32];
    std::snprintf(v, sizeof(v), "%.2f", delta);
    const char* cmd[] = { "seek", v, "relative+keyframes", nullptr };
    mpv_command_async(mpv, 0, cmd);

    setControlsVisible(true);
}

void LocalMpvView::updateSeekBar()
{
    if (!seekOverlay || !mpv) return;
    double pos = obsPos;
    double dur  = obsDur;
    // During a touch-drag seek show the scrub target, not the actual position
    if (seeking && seekDur > 0.0) { pos = seekTarget; dur = seekDur; }

    if (seekCur)
    {
        int p = (int)std::max(0.0, pos);
        seekCur->setText(fmt::format("{}:{:02d}", p / 60, p % 60));
    }
    if (seekTotal && dur > 0.0)
    {
        int d = (int)dur;
        seekTotal->setText(fmt::format("{}:{:02d}", d / 60, d % 60));
    }

    if (seekFill && seekTrack && dur > 0.0)
    {
        float trackW = seekTrack->getWidth();
        if (trackW > 0.0f)
        {
            float frac = (float)(pos / dur);
            if (frac > 1.0f) frac = 1.0f;
            if (frac < 0.0f) frac = 0.0f;
            seekFill->setWidth(trackW * frac);
            if (seekCursor)
                seekCursor->setPositionLeft(trackW * frac - 3.0f);
        }
    }
}

void LocalMpvView::toggleLock()
{
    controlsLocked = !controlsLocked;
    flashLock();
}

void LocalMpvView::flashLock()
{
    if (!lockHint || !lockLabel) return;
    lockLabel->setText(controlsLocked ? tr("Locked") : tr("Unlocked"));
    lockHint->setVisibility(brls::Visibility::VISIBLE);
    lockFlashActive = true;
    lockFlashUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(1400);
}

void LocalMpvView::updateLockHint()
{
    if (!lockFlashActive || !lockHint) return;
    if (std::chrono::steady_clock::now() >= lockFlashUntil)
    {
        lockFlashActive = false;
        lockHint->setVisibility(controlsShown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

void LocalMpvView::seekToFraction(double frac)
{
    if (!ready || !mpv || obsDur <= 0.0) return;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    double target = frac * obsDur;
    char t[32];
    std::snprintf(t, sizeof(t), "%.3f", target);
    const char* cmd[] = { "seek", t, "absolute", nullptr };
    mpv_command_async(mpv, 0, cmd);
    obsPos     = target;
    seekTarget = target;
    seeking    = false;
    userPaused = false;
    mpv_set_property_string(mpv, "pause", "no");
    setControlsVisible(false);
}

void LocalMpvView::setSubDelay(double seconds)
{
    constexpr double kMax = 10.0;
    if (seconds < -kMax) seconds = -kMax;
    if (seconds > kMax) seconds = kMax;
    subDelay = std::round(seconds * 100.0) / 100.0;

    if (mpv)
    {
        char v[24];
        std::snprintf(v, sizeof(v), "%.2f", subDelay);
        mpv_set_property_string(mpv, "sub-delay", v);
    }

    if (subDelaySink)
        subDelaySink(subDelay);
    else
        flashPill(tr("Subtitles  ") + subDelayText(subDelay));
}

void LocalMpvView::nudgeSpeed(int dir)
{
    int idx = 2; // 1.0x
    for (int i = 0; i < 6; i++)
    {
        if (std::abs(kSpeeds[i] - playSpeed) < 0.05)
        {
            idx = i;
            break;
        }
    }
    idx += dir;
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    playSpeed = kSpeeds[idx];
    if (mpv)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", playSpeed);
        mpv_set_property_string(mpv, "speed", buf);
    }
    flashPill(fmt::format("Speed {:.2f}x", playSpeed));
}

void LocalMpvView::flashPill(const std::string& text)
{
    if (!hintPill || !pillLabel) return;
    pillLabel->setText(text);
    hintPill->setVisibility(brls::Visibility::VISIBLE);
    pillFlashActive = true;
    pillFlashUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
}

void LocalMpvView::updatePill()
{
    if (!pillFlashActive || !hintPill) return;
    if (std::chrono::steady_clock::now() >= pillFlashUntil)
    {
        pillFlashActive = false;
        hintPill->setVisibility(brls::Visibility::GONE);
    }
}

void LocalMpvView::openTrackMenu()
{
    if (!ready || !mpv) return;

    setControlsVisible(false);
    settingsOpen = true;

    auto label = [](const std::string& lang, const std::string& title, int64_t id) {
        std::string s = lang;
        for (auto& c : s) c = (char)std::toupper((unsigned char)c);
        if (!title.empty()) s += (s.empty() ? "" : " - ") + title;
        if (s.empty()) s = tr("Track ") + std::to_string(id);
        return s;
    };

    std::vector<std::string> aLabels, sLabels{ tr("Off") };
    std::vector<std::string> aLangs, sLangs{ "" };
    std::vector<int64_t> aIds, sIds{ -1 };
    int aCur = 0, sCur = 0;

    mpv_node node;
    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) >= 0)
    {
        if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list)
        {
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
                    aLangs.push_back(!lang.empty() ? lang : title);
                    aIds.push_back(id);
                }
                else if (type == "sub")
                {
                    if (sel) sCur = (int)sIds.size();
                    sLabels.push_back(label(lang, title, id));
                    sLangs.push_back(!lang.empty() ? lang : title);
                    sIds.push_back(id);
                }
            }
        }
        mpv_free_node_contents(&node);

        int subVis = 0;
        if (mpv_get_property(mpv, "sub-visibility", MPV_FORMAT_FLAG, &subVis) < 0 || !subVis)
            sCur = 0;
    }

    auto* root = new brls::Box();
    root->setAxis(brls::Axis::ROW);
    root->setJustifyContent(brls::JustifyContent::FLEX_END);
    root->setGrow(1.0f);

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
    panel->setBackgroundColor(theme::isLight() ? nvgRGBA(246, 246, 250, 242)
                                               : nvgRGBA(18, 18, 22, 238));
    root->addView(panel);

    panel->registerAction(
        tr("Close"), brls::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity();
            return true;
        },
        false, false, brls::SOUND_BACK);

    auto* title = new brls::Label();
    title->setText(tr("Playback"));
    title->setFontSize(28.0f);
    title->setTextColor(theme::text());
    title->setMarginBottom(4.0f);
    panel->addView(title);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    scroll->setContentView(content);
    panel->addView(scroll);

    auto nearest = [](const std::vector<double>& v, double x) {
        int best = 0;
        double bd = 1e18;
        for (size_t i = 0; i < v.size(); i++)
        {
            double d = x > v[i] ? x - v[i] : v[i] - x;
            if (d < bd) { bd = d; best = (int)i; }
        }
        return best;
    };
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

    content->addView(panelSection(tr("AUDIO"), 8.0f));

    if (!aLabels.empty())
    {
        auto* a = new LocalTrackCell();
        a->init(tr("Track"), aLabels, aCur, [this, aIds, aLangs](int sel) {
            char v[24];
            std::snprintf(v, sizeof(v), "%lld", (long long)aIds[sel]);
            const char* cmd[] = { "set", "aid", v, nullptr };
            mpv_command_async(mpv, 0, cmd);
            if (sel >= 0 && sel < (int)aLangs.size())
            {
                std::string code = config::langCodeFor(aLangs[sel]);
                if (!code.empty() && code != "auto")
                {
                    config::get().audioLang = code;
                    config::save();
                }
            }
        });
        content->addView(a);
    }

    {
        playSpeed = getScale("speed");
        auto* sp  = new brls::SelectorCell();
        sp->init(tr("Speed"), kSpeedLabels, nearest(kSpeeds, playSpeed),
                 [this, setScale](int sel) {
                     playSpeed = kSpeeds[(size_t)sel];
                     setScale("speed", playSpeed);
                 });
        content->addView(sp);
    }

    content->addView(panelSection(tr("SUBTITLES"), 18.0f));

    if (sLabels.size() > 1)
    {
        auto* s = new LocalTrackCell();
        s->init(tr("Subtitles"), sLabels, sCur, [this, sIds, sLangs](int sel) {
            int64_t sid = sIds[(size_t)sel];
            if (sid <= 0)
            {
                int64_t noId = 0;
                mpv_set_property(mpv, "sid", MPV_FORMAT_INT64, &noId);
                int visible = 0;
                mpv_set_property(mpv, "sub-visibility", MPV_FORMAT_FLAG, &visible);
                config::get().subtitles = false;
                config::save();
            }
            else
            {
                mpv_set_property(mpv, "sid", MPV_FORMAT_INT64, &sid);
                int visible = 1;
                mpv_set_property(mpv, "sub-visibility", MPV_FORMAT_FLAG, &visible);
                if (sel >= 0 && sel < (int)sLangs.size())
                {
                    std::string code = config::langCodeFor(sLangs[sel]);
                    if (!code.empty() && code != "auto")
                        config::get().subLang = code;
                }
                config::get().subtitles = true;
                config::save();
            }
        });
        content->addView(s);

        const std::vector<double> scales = { 0.75, 0.9, 1.0, 1.15, 1.3, 1.5 };
        auto* z = new brls::SelectorCell();
        z->init(tr("Subtitle size"),
                { "75%", "90%", "100%", "115%", "130%", "150%" },
                nearest(scales, getScale("sub-scale")),
                [scales, setScale](int sel) { setScale("sub-scale", scales[sel]); });
        content->addView(z);

        auto* sd = new brls::DetailCell();
        sd->setText(tr("Subtitle delay"));
        sd->setDetailText(subDelayText(subDelay));
        sd->setDetailTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
        sd->setFocusable(false);
        sd->setLineBottom(0.0f);
        content->addView(sd);

        subDelaySink = [sd](double d) { sd->setDetailText(subDelayText(d)); };

        auto* hint = new brls::Label();
        hint->setText(tr("L / R shift the subtitles by 0.1 s. Later is positive. "
                      "The video keeps playing behind this."));
        hint->setFontSize(15.0f);
        hint->setTextColor(theme::textMuted());
        hint->setLineHeight(1.35f);
        hint->setMargins(6.0f, 12.0f, 4.0f, 12.0f);
        content->addView(hint);

        auto nudge = [this](double delta) {
            setSubDelay(subDelay + delta);
            return true;
        };
        panel->registerAction(
            tr("Subtitles earlier"), brls::BUTTON_LB,
            [nudge](brls::View*) { return nudge(-0.1); }, false, true,
            brls::SOUND_NONE);
        panel->registerAction(
            tr("Subtitles later"), brls::BUTTON_RB,
            [nudge](brls::View*) { return nudge(0.1); }, false, true,
            brls::SOUND_NONE);
    }
    else
    {
        auto* s = new brls::SelectorCell();
        s->init(tr("Subtitles"), { tr("None") }, 0, [](int) {});
        content->addView(s);
    }

    auto liveFlag = this->alive;
    brls::Application::pushActivity(
        new LocalSettingsActivity(root, [this, liveFlag]() {
            if (!*liveFlag) return;
            settingsOpen = false;
            subDelaySink = nullptr;
        }));
}

void LocalMpvView::updateInfoOverlay()
{
    // Media info
}

void LocalMpvView::pumpEvents()
{
    if (!mpv) return;

    while (mpv_event* ev = mpv_wait_event(mpv, 0))
    {
        if (ev->event_id == MPV_EVENT_NONE) break;

        switch (ev->event_id)
        {
            case MPV_EVENT_FILE_LOADED:
            case MPV_EVENT_PLAYBACK_RESTART:
                fileLoaded = true;
                ready = true;
                break;
            case MPV_EVENT_END_FILE:
            {
                auto* ef = (mpv_event_end_file*)ev->data;
                if (ef && ef->reason == MPV_END_FILE_REASON_EOF && !ended)
                {
                    ended = true;
                    brls::sync([]() { brls::Application::popActivity(); });
                }
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
            {
                auto* p = (mpv_event_property*)ev->data;
                if (p && p->data)
                {
                    if (p->format == MPV_FORMAT_DOUBLE)
                    {
                        double v = *(double*)p->data;
                        if (std::strcmp(p->name, "time-pos") == 0) obsPos = v;
                        else if (std::strcmp(p->name, "duration") == 0) obsDur = v;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void LocalMpvView::draw(NVGcontext* vg, float x, float y, float width, float height,
                        brls::Style style, brls::FrameContext* ctx)
{
    pumpEvents();

    if (brls::Application::getInputType() == brls::InputType::TOUCH)
        brls::Application::setInputType(brls::InputType::GAMEPAD);

    if (!renderCtx)
    {
        brls::Box::draw(vg, x, y, width, height, style, ctx);
        return;
    }

    nvgEndFrame(vg);

    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);

    mpv_opengl_fbo mfbo;
    mfbo.fbo = fbo;
    mfbo.w = (int)brls::Application::windowWidth;
    mfbo.h = (int)brls::Application::windowHeight;
    mfbo.internal_format = 0;
    int flip = 1;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &mfbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip },
        { MPV_RENDER_PARAM_INVALID, nullptr },
    };

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

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    mpv_render_context_report_swap(renderCtx);

    double scaleFactor = brls::Application::getPlatform()->getVideoContext()->getScaleFactor();
    nvgBeginFrame(vg, brls::Application::windowWidth, brls::Application::windowHeight, (float)scaleFactor);
    nvgScale(vg, brls::Application::windowScale, brls::Application::windowScale);

    if (ready && !overlayHidden && loadingOverlay)
    {
        if (mpv) mpv_set_property_string(mpv, "pause", "no");
        loadingOverlay->setVisibility(brls::Visibility::GONE);
        overlayHidden = true;
        sys::setCpuBoost(false);
    }

    if (ready)
        updateStickSeek();
    if (controlsShown)
        updateSeekBar();
    updateControlsAutoHide();
    updateLockHint();
    updatePill();

    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

LocalPlayerActivity::LocalPlayerActivity(std::string filePath, std::string title)
    : filePath(std::move(filePath))
    , title(std::move(title))
{
}

brls::View* LocalPlayerActivity::createContentView()
{
    return new LocalMpvView(filePath, title);
}
