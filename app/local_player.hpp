#pragma once

#include <borealis.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct mpv_handle;
struct mpv_render_context;

class LocalMpvView : public brls::Box
{
  public:
    explicit LocalMpvView(const std::string& filePath, const std::string& title = "");
    ~LocalMpvView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    void pumpEvents();
    bool startMpv();
    void registerPlayerActions();
    void buildLoadingOverlay(const std::string& title);

    std::string filePath;
    std::string displayTitle;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    mpv_handle* mpv = nullptr;
    mpv_render_context* renderCtx = nullptr;

    // Loading overlay
    brls::Box* loadingOverlay = nullptr;
    brls::Label* statusLabel  = nullptr;

    // Pause icon + seek bar + overlays
    brls::Box* pauseOverlay   = nullptr;
    brls::Box* pauseTitleBox  = nullptr;
    brls::Box* optionsHint    = nullptr;
    brls::Box* lockHint       = nullptr;
    brls::Label* lockLabel    = nullptr;
    brls::Box* seekOverlay    = nullptr;
    brls::Box* seekFill       = nullptr;
    brls::Label* seekCur      = nullptr;
    brls::Label* seekTotal    = nullptr;
    brls::Box* seekCursor     = nullptr;
    brls::Box* seekTrack      = nullptr;

    bool fileLoaded           = false;
    bool ready                = false;
    bool overlayHidden        = false;
    bool userPaused           = false;
    bool controlsShown        = false;
    bool controlsLocked       = false;
    bool lockFlashActive      = false;
    std::chrono::steady_clock::time_point lockFlashUntil;
    std::chrono::steady_clock::time_point controlsHideAt;

    // Scrubbing & seeking
    bool seeking              = false;
    double seekTarget         = 0.0;
    double seekDur            = 0.0;
    double seekHeld           = 0.0;
    bool stickWasPushed       = false;
    std::chrono::steady_clock::time_point seekLastFrame;
    bool seekFrameValid       = false;

    // Observed mpv properties
    double obsPos             = 0.0;
    double obsDur             = 0.0;

    // Touch & button actions
    void onPlayPause();
    void setControlsVisible(bool show);
    void seekToFraction(double frac);
    void beginSeek();
    void cancelSeek();
    void updateStickSeek();
    void updateSeekBar();
    void updateControlsAutoHide();
    void toggleLock();
    void flashLock();
    void updateLockHint();
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

    // Speed & subtitle delay pills
    double playSpeed          = 1.0;
    void nudgeSpeed(int dir);
    double subDelay           = 0.0;
    void setSubDelay(double seconds);
    std::function<void(double)> subDelaySink;
    void flashPill(const std::string& text);
    void updatePill();
    brls::Box* hintPill       = nullptr;
    brls::Label* pillLabel    = nullptr;
    bool pillFlashActive      = false;
    std::chrono::steady_clock::time_point pillFlashUntil;

    // Track selector menu (X button)
    void openTrackMenu();
    bool settingsOpen         = false;

    // Info overlay (ZR button)
    brls::Box* infoOverlay    = nullptr;
    brls::Label* infoLabel    = nullptr;
    brls::Label* infoLabel2   = nullptr;
    bool infoShown            = false;
    std::chrono::steady_clock::time_point infoLastSample;
    void updateInfoOverlay();

    bool boostApplied         = false;
    bool ended                = false;
};

class LocalPlayerActivity : public brls::Activity
{
  public:
    explicit LocalPlayerActivity(std::string filePath, std::string title = "");
    brls::View* createContentView() override;

  private:
    std::string filePath;
    std::string title;
};
