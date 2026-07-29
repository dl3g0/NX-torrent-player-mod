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

} // namespace

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
