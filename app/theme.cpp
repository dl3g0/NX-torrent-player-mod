#include "theme.hpp"

#include "config.hpp"

namespace theme
{
namespace
{

// Every accent is dark enough to carry white text: it is a *fill* under the
// active tab's label and under the primary button's, not just a tint. That is
// why the greens and oranges here are deeper than a palette picked on its own
// would be -- a bright one looks better in a swatch and is unreadable in place.
const std::vector<Scheme> kSchemes = {
    { "purple", "Purple", nvgRGB(0x60, 0x3c, 0xf3), nvgRGB(0x1a, 0x17, 0x3e),
      nvgRGB(0x25, 0x25, 0x4e), nvgRGB(0x07, 0x07, 0x14) },
    { "blue", "Blue", nvgRGB(0x2e, 0x7b, 0xf5), nvgRGB(0x0f, 0x1e, 0x3c),
      nvgRGB(0x17, 0x29, 0x4f), nvgRGB(0x05, 0x08, 0x10) },
    { "teal", "Teal", nvgRGB(0x0e, 0x9a, 0xa7), nvgRGB(0x0c, 0x2a, 0x30),
      nvgRGB(0x17, 0x42, 0x4a), nvgRGB(0x04, 0x0b, 0x0c) },
    { "green", "Green", nvgRGB(0x21, 0x92, 0x4f), nvgRGB(0x0e, 0x2e, 0x1e),
      nvgRGB(0x1c, 0x48, 0x2f), nvgRGB(0x04, 0x0c, 0x08) },
    { "orange", "Orange", nvgRGB(0xd9, 0x64, 0x0f), nvgRGB(0x38, 0x20, 0x10),
      nvgRGB(0x4a, 0x2d, 0x19), nvgRGB(0x0e, 0x08, 0x04) },
    { "red", "Red", nvgRGB(0xe2, 0x3b, 0x4e), nvgRGB(0x3a, 0x14, 0x1a),
      nvgRGB(0x4e, 0x1e, 0x26), nvgRGB(0x10, 0x05, 0x07) },
    { "pink", "Pink", nvgRGB(0xd9, 0x41, 0x8f), nvgRGB(0x38, 0x14, 0x2b),
      nvgRGB(0x4a, 0x1d, 0x39), nvgRGB(0x0f, 0x05, 0x0b) },
};

std::function<void()> repaintHook;

// The console's own preference, captured before we overwrite it. DARK unless
// captureSystemVariant() says otherwise -- which matches what the app did when
// the variant was hardcoded.
brls::ThemeVariant systemVariant = brls::ThemeVariant::DARK;

// The variant this session runs in, decided once by applyVariant(). Defaults to
// dark, which is what the app did when the variant was hardcoded -- so the
// palette is still sane if anything reads it before startup gets that far.
bool lightLatched = false;

// `t` of `c`, the rest white. Used to tint the light variant's backgrounds off
// the accent.
NVGcolor overWhite(NVGcolor c, float t)
{
    return nvgRGBf(1.0f + (c.r - 1.0f) * t, 1.0f + (c.g - 1.0f) * t,
                   1.0f + (c.b - 1.0f) * t);
}

// Picks between a dark-variant and a light-variant colour.
inline NVGcolor pick(NVGcolor dark, NVGcolor light)
{
    return isLight() ? light : dark;
}

} // namespace

void captureSystemVariant()
{
    systemVariant = brls::Application::getPlatform()->getThemeVariant();
    brls::Logger::info("[theme] console variant: {}",
                       systemVariant == brls::ThemeVariant::LIGHT ? "light"
                                                                  : "dark");
}

void applyVariant()
{
    const std::string& v = config::get().themeVariant;
    lightLatched         = v == "light"  ? true
                           : v == "dark" ? false
                                         : systemVariant == brls::ThemeVariant::LIGHT;

    brls::Application::getPlatform()->setThemeVariant(
        lightLatched ? brls::ThemeVariant::LIGHT : brls::ThemeVariant::DARK);
    brls::Logger::info("[theme] variant in force: {} (setting: {})",
                       lightLatched ? "light" : "dark", v);
}

bool isLight() { return lightLatched; }

NVGcolor accent() { return current().accent; }

NVGcolor gradTop()
{
    return pick(current().gradTop, overWhite(current().accent, 0.18f));
}

NVGcolor gradBottom()
{
    return isLight() ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0x00, 0x00, 0x00);
}

NVGcolor bgInner()
{
    return pick(current().bgInner, overWhite(current().accent, 0.14f));
}

NVGcolor bgOuter()
{
    return pick(current().bgOuter, overWhite(current().accent, 0.04f));
}

// The text ramp, mirrored: each light tier keeps roughly the contrast against a
// near-white background that its dark counterpart has against a near-black one.
NVGcolor text() { return pick(nvgRGB(255, 255, 255), nvgRGB(24, 24, 28)); }
NVGcolor textBody() { return pick(nvgRGB(202, 202, 208), nvgRGB(58, 58, 66)); }
NVGcolor textDim() { return pick(nvgRGB(190, 190, 195), nvgRGB(84, 84, 92)); }
NVGcolor textMuted() { return pick(nvgRGB(150, 150, 155), nvgRGB(116, 116, 124)); }
NVGcolor textFaint() { return pick(nvgRGB(110, 110, 115), nvgRGB(150, 150, 158)); }
NVGcolor textWarn() { return pick(nvgRGB(205, 140, 140), nvgRGB(168, 54, 54)); }

NVGcolor surface() { return pick(nvgRGB(0x2c, 0x2c, 0x31), nvgRGB(0xe9, 0xe9, 0xee)); }
NVGcolor surfaceSunken()
{
    return pick(nvgRGB(0x1a, 0x1a, 0x20), nvgRGB(0xdc, 0xdc, 0xe2));
}

NVGcolor scrim(int a)
{
    return isLight() ? nvgRGBA(0, 0, 0, a) : nvgRGBA(255, 255, 255, a);
}

const std::vector<Scheme>& schemes() { return kSchemes; }

void setRepaintHook(std::function<void()> fn) { repaintHook = std::move(fn); }

const std::vector<std::string>& schemeIds()
{
    static std::vector<std::string> v = [] {
        std::vector<std::string> o;
        for (const auto& s : kSchemes) o.push_back(s.id);
        return o;
    }();
    return v;
}

const std::vector<std::string>& schemeLabels()
{
    static std::vector<std::string> v = [] {
        std::vector<std::string> o;
        for (const auto& s : kSchemes) o.push_back(s.label);
        return o;
    }();
    return v;
}

const Scheme& current()
{
    const std::string& id = config::get().accent;
    for (const auto& s : kSchemes)
        if (id == s.id) return s;
    return kSchemes[0];
}

void drawStremioMark(NVGcontext* vg, float x, float y, float size)
{
    // Geometry lifted straight from assets/stremio.svg, whose canvas is 124.926
    // units square: a 88.336 rounded square (r=6) rotated 45 degrees, centred --
    // its diagonal is 88.336 * sqrt(2) = the full canvas, so the diamond's points
    // touch the edges. Everything below is in those units, scaled by k.
    const float k  = size / 124.926f;
    const float cx = x + size * 0.5f, cy = y + size * 0.5f;
    const float side = 88.336f * k;

    nvgSave(vg);
    nvgTranslate(vg, cx, cy);
    nvgRotate(vg, NVG_PI * 0.25f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -side * 0.5f, -side * 0.5f, side, side, 6.0f * k);
    nvgFillColor(vg, accent());
    nvgFill(vg);
    nvgRestore(vg);

    // The play glyph, as a plain triangle: the SVG rounds its three corners by 1
    // unit out of 125, i.e. well under half a pixel at any size the header uses.
    // The vertices are the midpoints of those corner arcs.
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + 53.954f * k, y + 40.996f * k);
    nvgLineTo(vg, x + 83.389f * k, y + 62.464f * k);
    nvgLineTo(vg, x + 53.954f * k, y + 83.931f * k);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgFill(vg);
}

void applyAccent()
{
    // The focus highlight too: borealis' default glow is cyan, which clashes
    // with every accent here. color2 is the border stroke, color1 the pulsating
    // glow; both to the accent gives a clean solid focus ring.
    //
    // Set on BOTH variants: getTheme() returns the light OR dark table depending
    // on the console's system theme, and drawing may well use the one we did not
    // touch -- which would keep borealis' default cyan. (That was the bug: only
    // the startup variant was patched.)
    NVGcolor accent = current().accent;
    NVGcolor white  = nvgRGB(0xff, 0xff, 0xff);
    for (brls::Theme* t : { &brls::Theme::getLightTheme(),
                            &brls::Theme::getDarkTheme() })
    {
        t->addColor("brls/accent", accent);
        t->addColor("brls/highlight/color1", accent);
        t->addColor("brls/highlight/color2", accent);
        // The selected tab is BUTTONSTYLE_PRIMARY: its fill is this color, and
        // its label goes white so it stays readable on it (the dark theme's
        // default primary text is near-black, made for a light fill).
        t->addColor("brls/button/primary_enabled_background", accent);
        t->addColor("brls/button/primary_enabled_text", white);
    }

    if (repaintHook) repaintHook();
}

} // namespace theme
