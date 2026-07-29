#pragma once

#include <borealis.hpp>

// The options screen, opened with X from the browser. Every change is written
// to config.json on the spot -- there is no "save" button to forget.
class SettingsActivity : public brls::Activity
{
  public:
    brls::View* createContentView() override;
};

// Resizes borealis' logical space to the UI size configured for the mode the
// console is currently in (docked or handheld), and relayouts. Call it once the
// window exists, on every window-size change, and after the setting is edited.
void applyUiScale();
