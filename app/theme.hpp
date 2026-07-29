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

// ---- light / dark ---------------------------------------------------------
//
// config::themeVariant is "dark", "light" or "system". Read the console's own
// preference ONCE at startup, before anything calls setThemeVariant: on Switch
// that setter overwrites the very field getThemeVariant() reads back (it caches
// the ColorSetId at construction), so afterwards the console's choice is gone.
void captureSystemVariant();

// Resolves config + the console into the variant for this session, latches it,
// and pushes it into the platform. Startup only, and it is the ONLY reader of
// config::themeVariant: borealis states the variant is not expected to change
// while running, and views that copied a theme colour when they were built would
// keep it. Latching is what makes that true -- while the palette below read the
// config live, everything drawn per-frame switched the instant the setting
// changed while everything else kept its old colours: a half-converted UI.
void applyVariant();

// The variant in force *this session* -- not what config currently says.
bool isLight();

// ---- palette -------------------------------------------------------------
//
// The surfaces the app paints itself, as semantic roles rather than literals:
// borealis' own chrome follows the variant on its own, everything we draw does
// not. Each returns the dark or light value for the variant in force.
//
// In the light variant the background field is derived from the accent (a light
// tint of it) instead of being hand-picked per scheme, unlike the dark values in
// the table above: it is near *black* that the perceptual step from a hue varies
// enough to need tuning by hand, while light tints of the same strength read
// consistently across hues.
NVGcolor accent();      // never varies -- it carries white text in both
NVGcolor gradTop();     // Stremio tab gradient, start
NVGcolor gradBottom();  // ... and end (black when dark, white when light)
NVGcolor bgInner();
NVGcolor bgOuter();

NVGcolor text();        // strongest app-drawn text
NVGcolor textBody();    // descriptions, meta lines
NVGcolor textDim();     // secondary labels
NVGcolor textMuted();   // captions, empty states, Options hints
NVGcolor textFaint();   // the faintest tier (a disabled search result)
NVGcolor textWarn();    // an unavailable / error line

NVGcolor surface();        // a card sitting on the field
NVGcolor surfaceSunken();  // a thumbnail well, one step below a card

// A neutral veil at alpha `a`: white over dark, black over light. Progress-bar
// tracks and input wells, which must read as "the surface, slightly lifted".
NVGcolor scrim(int a);

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

// Draws the Stremio mark -- the rotated rounded square with a play glyph in it --
// into a `size` x `size` box at (x, y), in the current accent instead of the
// artwork's fixed blue-violet gradient. That is the whole reason it is drawn
// rather than loaded from romfs:/stremio-icon.png.
void drawStremioMark(NVGcontext* vg, float x, float y, float size);

// A Box / Label that is always the current accent, by re-reading it as it draws
// rather than copying it once at construction. Both setters are plain field
// assignments, so this costs nothing per frame and needs no hook.
class AccentBox : public brls::Box
{
  public:
    void draw(NVGcontext* vg, float x, float y, float w, float h,
              brls::Style style, brls::FrameContext* ctx) override
    {
        this->setBackgroundColor(accent());
        brls::Box::draw(vg, x, y, w, h, style, ctx);
    }
};

class AccentLabel : public brls::Label
{
  public:
    void draw(NVGcontext* vg, float x, float y, float w, float h,
              brls::Style style, brls::FrameContext* ctx) override
    {
        this->setTextColor(accent());
        brls::Label::draw(vg, x, y, w, h, style, ctx);
    }
};

} // namespace theme
