/*
    Copyright 2019 p-sam
    Copyright 2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <switch.h>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/switch/switch_font.hpp>

namespace brls
{

void SwitchFontLoader::loadFonts()
{
    PlFontData font;
    Result rc;
    NVGcontext* vg = brls::Application::getNVGContext();

    brls::Logger::info("switch system locale: {}", brls::Application::getPlatform()->getLocale());

    // Standard (system Latin/kana/basic). Loaded as FONT_REGULAR and used as a
    // fallback of the primary font below.
    rc = plGetSharedFontByType(&font, PlSharedFontType_Standard);
    bool haveStd = R_SUCCEEDED(rc) &&
                   Application::loadFontFromMemory(FONT_REGULAR, font.address, font.size, false);
    if (!haveStd)
        Logger::error("switch: could not load Standard shared font: {:#x}", rc);

    // LOCAL PATCH (see VENDORED.md): make the app's bundled Space Grotesk the
    // PRIMARY font. getDefaultFont() is FONT_CHINESE_SIMPLIFIED on Switch, so it
    // is loaded into THAT slot -- all UI text renders in it -- and the system
    // fonts (Standard here, then ext/traditional/Korean/symbols below) are added
    // as fallbacks for the glyphs it lacks. Loading it merely as FONT_REGULAR did
    // not work: the system Chinese font that used to be the primary carries Latin
    // glyphs, so the fallback to Space Grotesk was never reached for Latin text.
    bool sg = Application::loadFontFromFile(FONT_CHINESE_SIMPLIFIED,
                                            "romfs:/SpaceGrotesk-Medium.ttf");
    if (sg)
    {
        brls::Logger::info("switch: loaded Space Grotesk as the primary font");
        if (haveStd)
            nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED),
                                 Application::getFont(FONT_REGULAR));
        // System Simplified Chinese as a fallback (own slot; primary name is taken).
        rc = plGetSharedFontByType(&font, PlSharedFontType_ChineseSimplified);
        if (R_SUCCEEDED(rc) &&
            Application::loadFontFromMemory("sys_chinese", font.address, font.size, false))
            nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED),
                                 Application::getFont("sys_chinese"));
    }
    else
    {
        // Space Grotesk missing: stock behaviour -- system Simplified Chinese is
        // the primary, Standard its Latin fallback.
        Logger::error("switch: Space Grotesk not found, falling back to system fonts");
        rc = plGetSharedFontByType(&font, PlSharedFontType_ChineseSimplified);
        if (R_SUCCEEDED(rc) &&
            Application::loadFontFromMemory(FONT_CHINESE_SIMPLIFIED, font.address, font.size, false))
            nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED),
                                 Application::getFont(FONT_REGULAR));
        else
            Logger::error("switch: could not load Chinese Simplified shared font: {:#x}", rc);
    }

    // Simplified Chinese ext
    rc = plGetSharedFontByType(&font, PlSharedFontType_ExtChineseSimplified);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_CHINESE_SIMPLIFIED_EXT, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_CHINESE_SIMPLIFIED_EXT));
    else
        Logger::error("switch: could not load Chinese Simplified Extended shared font: {:#x}", rc);

    // Traditional Chinese
    rc = plGetSharedFontByType(&font, PlSharedFontType_ChineseTraditional);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_CHINESE_TRADITIONAL, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_CHINESE_TRADITIONAL));
    else
        Logger::error("switch: could not load Chinese Traditional shared font: {:#x}", rc);

    // Korean
    rc = plGetSharedFontByType(&font, PlSharedFontType_KO);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_KOREAN_REGULAR, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_KOREAN_REGULAR));
    else
        Logger::error("switch: could not load Korean shared font: {:#x}", rc);

    // Extended (symbols)
    rc = plGetSharedFontByType(&font, PlSharedFontType_NintendoExt);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_SWITCH_ICONS, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_SWITCH_ICONS));
    else
        Logger::error("switch: could not load Extented shared font: {:#x}", rc);

    // Material icons
    if (this->loadMaterialFromResources())
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_MATERIAL_ICONS));
    else
        Logger::error("switch: could not load Material icons font from resources");

    // Load Emoji
    if (!USER_EMOJI_PATH.empty())
    {
        if (access(USER_EMOJI_PATH.c_str(), F_OK) != -1)
        {
            brls::Logger::info("Load emoji font: {}", USER_EMOJI_PATH);
            this->loadFontFromFile("emoji", USER_EMOJI_PATH);
            nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont("emoji"));
        }
        else
        {
            brls::Logger::warning("Cannot find custom emoji, (Searched at: {})", USER_EMOJI_PATH);
        }
    }
}

} // namespace brls
