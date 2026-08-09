#pragma once

#include <string>
#include <vector>

// The app's own translation layer, keyed on the English source string.
//
// Not borealis' i18n: that one picks its locale from the console (getLocale())
// with no way to override it, and it keys on opaque names in JSON files under
// romfs. Keying on English instead means the source stays readable, a string
// with no entry in the table degrades to English rather than to a key, and
// nothing has to be kept in sync across two files.
//
// The language is latched once at startup (see load()) because the strings are
// read when a view is built, and half the UI -- the header, the tab bar, the
// browser -- is built once and never rebuilt. Options says so.
namespace i18n
{

// Reads the language out of config and builds the table. Call once, after
// config::load() and before any view exists.
void load();

// The active language, as stored in config: "en" or "fr".
const std::string& lang();

// The offered languages: ids and labels, index-matched. Labels are endonyms
// ("Francais"), which is what a language picker should show whatever the UI is
// currently in.
const std::vector<std::string>& langIds();
const std::vector<std::string>& langLabels();

// The translation of `en`, or `en` itself when there is none (always the case
// in English -- the table is not even built then). The returned pointer is
// either the literal passed in or a string owned by the table, so it outlives
// the call either way.
const char* tr(const char* en);

} // namespace i18n

// Deliberately unqualified: it appears a few hundred times across the UI and
// `i18n::tr(...)` around every label would drown the code it wraps.
using i18n::tr;
