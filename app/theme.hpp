#pragma once

#include <borealis.hpp>

#include <functional>
#include <string>
#include <vector>

// The app's colour scheme: an accent, plus the two backgrounds that have to
// move with it -- the Stremio tab's gradient and the drill-down screens' radial
// field. They are stored per scheme rather than derived from the accent because
// a background is a much darker, much less saturated colour than the accent it
// sits under, and the ratio that looks right is not the same at every hue.
//
// The choice lives in config as an id ("purple"), not an index, so reordering
// the table cannot silently repaint everyone's app.
namespace theme
{

struct Scheme
{
    const char* id;     // what config stores
    const char* label;  // what Options shows
    NVGcolor accent;    // buttons, focus highlight, values, progress fills
    NVGcolor gradTop;   // Stremio tab background, fading down to black
    NVGcolor bgInner;   // drill-down screens: the lit centre of the field
    NVGcolor bgOuter;   // ... and its far edge
};

// Every scheme offered, default first. Ids and labels are index-matched with
// schemeIds()/schemeLabels(), which exist for the Options selector.
const std::vector<Scheme>& schemes();
const std::vector<std::string>& schemeIds();
const std::vector<std::string>& schemeLabels();

// The scheme config::accent names, or the default if it names one we do not
// have -- so a hand-edited config.json cannot leave the app unpainted.
const Scheme& current();

// Writes the current accent into both borealis theme variants, then runs the
// repaint hook. Call once at startup and again whenever the choice changes; the
// backgrounds need no equivalent, they read current() as they draw.
void applyAccent();

// Called at the end of applyAccent(), for the views that cannot re-read the
// accent themselves -- borealis Buttons take their colours from the theme when a
// style is applied and keep them. The hook must *restyle*, never rebuild: it can
// run while Options is on top of the browser, and freeing a view that is either
// focused or sitting on Application::focusStack strands a dangling pointer that
// popActivity then dereferences.
void setRepaintHook(std::function<void()> fn);

// A Box / Label that is always the current accent, by re-reading it as it draws
// rather than copying it once at construction. Both setters are plain field
// assignments, so this costs nothing per frame and needs no hook.
class AccentBox : public brls::Box
{
  public:
    void draw(NVGcontext* vg, float x, float y, float w, float h,
              brls::Style style, brls::FrameContext* ctx) override
    {
        this->setBackgroundColor(current().accent);
        brls::Box::draw(vg, x, y, w, h, style, ctx);
    }
};

class AccentLabel : public brls::Label
{
  public:
    void draw(NVGcontext* vg, float x, float y, float w, float h,
              brls::Style style, brls::FrameContext* ctx) override
    {
        this->setTextColor(current().accent);
        brls::Label::draw(vg, x, y, w, h, style, ctx);
    }
};

} // namespace theme
