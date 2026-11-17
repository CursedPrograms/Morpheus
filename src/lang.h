// Morpheus - language pack: 12 languages loaded from lang/<code>.txt at startup.
// Every UI string goes through L("English text") which returns the translation or falls back to the English key.
#pragma once
#include <string>

namespace Lang {
// Load a language pack by its two-letter code (e.g. "ja", "zh").  Empty = use English.
void load(const std::string& code);
// Return the current language code ("" = English).
const std::string& current();
// Translate key; returns key itself if no translation is found.
const char* get(const char* key);
}

// Convenience macro: L("New Composition") expands to Lang::get("New Composition").
#define L(s) Lang::get(s)
