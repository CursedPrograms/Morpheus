// Morpheus - language pack loader.
// Reads lang/<code>.txt (UTF-8, "Key = Value" lines, # comments) and stores key->value pairs.
// Falls back to the English key when a translation is missing.
#include "lang.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lang {

static std::string s_code;
static std::unordered_map<std::string, std::string> s_map;

void load(const std::string& code) {
    s_map.clear();
    s_code = code;
    if (code.empty()) return;

    // Locate the lang/ folder next to the executable.
    char* base = SDL_GetBasePath();
    const std::string dir = (base ? std::string(base) : std::string("./")) + "lang/";
    if (base) SDL_free(base);

    const std::string path = dir + code + ".txt";
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;

    // Parse "Key = Value" lines; skip comments and blank lines.
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        const char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;
        const char* eq = strchr(p, '=');
        if (!eq) continue;
        std::string key(p, eq);
        // Trim trailing space from key.
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        const char* v = eq + 1;
        while (*v == ' ' || *v == '\t') ++v;
        // Trim trailing newline/space from value.
        std::string val(v);
        while (!val.empty() && (val.back() == '\n' || val.back() == '\r' || val.back() == ' ')) val.pop_back();
        s_map[key] = val;
    }
    fclose(f);
}

const std::string& current() { return s_code; }

const char* get(const char* key) {
    if (s_map.empty()) return key;
    auto it = s_map.find(key);
    return it != s_map.end() ? it->second.c_str() : key;
}

}  // namespace Lang
