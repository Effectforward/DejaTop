#include "../include/discovery.hpp"
#include <cctype>
#include <sstream>

namespace {

vector<int> parseProtonVersionKey(const string& name) {
    vector<int> parts;
    int current = -1;

    for (char ch : name) {
        if (isdigit(static_cast<unsigned char>(ch))) {
            if (current < 0) current = 0;
            current = current * 10 + (ch - '0');
        } else if (current >= 0) {
            parts.push_back(current);
            current = -1;
        }
    }

    if (current >= 0) parts.push_back(current);
    return parts;
}

int compareProtonVersionKeys(const vector<int>& a, const vector<int>& b) {
    size_t count = std::max(a.size(), b.size());
    for (size_t i = 0; i < count; ++i) {
        int left = i < a.size() ? a[i] : -1;
        int right = i < b.size() ? b[i] : -1;
        if (left != right) return left > right ? 1 : -1;
    }
    return 0;
}

bool isCommentOrBlank(const string& line) {
    for (char ch : line) {
        if (!isspace(static_cast<unsigned char>(ch))) {
            return ch == '#';
        }
    }
    return true;
}

bool startsWithIndentKey(const string& line, const string& key) {
    size_t pos = 0;
    while (pos < line.size() && isspace(static_cast<unsigned char>(line[pos]))) ++pos;
    return line.compare(pos, key.size(), key) == 0;
}

size_t leadingIndentCount(const string& line) {
    size_t pos = 0;
    while (pos < line.size() && isspace(static_cast<unsigned char>(line[pos]))) ++pos;
    return pos;
}

string unescapeJsonString(const string& value) {
    string result;
    result.reserve(value.size());
    bool escape = false;
    for (char ch : value) {
        if (escape) {
            result += ch;
            escape = false;
        } else if (ch == '\\') {
            escape = true;
        } else {
            result += ch;
        }
    }
    return result;
}

string extractQuotedJsonValue(const string& content, const string& key) {
    size_t keyPos = content.find(key);
    if (keyPos == string::npos) return "";

    size_t colonPos = content.find(':', keyPos + key.size());
    if (colonPos == string::npos) return "";

    size_t quoteStart = content.find('"', colonPos + 1);
    if (quoteStart == string::npos) return "";

    string value;
    bool escape = false;
    for (size_t i = quoteStart + 1; i < content.size(); ++i) {
        char ch = content[i];
        if (escape) {
            value += ch;
            escape = false;
        } else if (ch == '\\') {
            escape = true;
        } else if (ch == '"') {
            return value;
        } else {
            value += ch;
        }
    }

    return "";
}

} // namespace

bool protonSortCompare(const string& a, const string& b) {
    string nameA = toLower(fs::path(a).filename().string());
    string nameB = toLower(fs::path(b).filename().string());
    
    bool aGE = nameA.find("ge-proton") != string::npos;
    bool bGE = nameB.find("ge-proton") != string::npos;
    if (aGE != bGE) return aGE;
    
    bool aExp = nameA.find("experimental") != string::npos;
    bool bExp = nameB.find("experimental") != string::npos;
    if (aExp != bExp) return aExp;
    
    vector<int> versionA = parseProtonVersionKey(nameA);
    vector<int> versionB = parseProtonVersionKey(nameB);
    int versionCompare = compareProtonVersionKeys(versionA, versionB);
    if (versionCompare != 0) return versionCompare > 0;

    return nameA > nameB;
}

vector<string> findProtonVersions() {
    vector<string> versions;
    string home = getHomeDir();
    vector<string> searchPaths = {
        (fs::path(home) / ".local" / "share" / "Steam" / "steamapps" / "common").string(),
        (fs::path(home) / ".steam" / "steam" / "steamapps" / "common").string(),
        (fs::path(home) / ".steam" / "root" / "compatibilitytools.d").string(),
        // Flatpak Steam
        (fs::path(home) / ".var" / "app" / "com.valvesoftware.Steam" / "data" / "Steam" / "steamapps" / "common").string(),
        (fs::path(home) / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam" / "compatibilitytools.d").string()
    };

    for (const auto& path : searchPaths) {
        if (fs::exists(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_directory()) {
                    string name = entry.path().filename().string();
                    if (name.find("Proton") != string::npos || name.find("proton") != string::npos) {
                        if (fs::exists(entry.path() / "proton")) {
                            try {
                                string canonicalPath = fs::canonical(entry.path()).string();
                                if (find(versions.begin(), versions.end(), canonicalPath) == versions.end()) {
                                    versions.push_back(canonicalPath);
                                }
                            } catch (const fs::filesystem_error&) {
                                string rawPath = entry.path().string();
                                if (find(versions.begin(), versions.end(), rawPath) == versions.end()) {
                                    versions.push_back(rawPath);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    sort(versions.begin(), versions.end(), protonSortCompare);
    return versions;
}

// ---------------------------------------------------------------------------
// Image header parsing (PNG + ICO)
// ---------------------------------------------------------------------------

ImageInfo getPngInfo(const string& path) {
    ImageInfo info;
    ifstream in(path, ios::binary);
    if (!in) return info;

    unsigned char sig[8];
    if (!in.read(reinterpret_cast<char*>(sig), 8)) return info;
    static const unsigned char pngSig[8] = {137,80,78,71,13,10,26,10};
    if (!std::equal(sig, sig+8, pngSig)) return info;

    unsigned char chunk[8];
    if (!in.read(reinterpret_cast<char*>(chunk), 8)) return info;
    if (chunk[4] != 'I' || chunk[5] != 'H' || chunk[6] != 'D' || chunk[7] != 'R') return info;

    unsigned char dims[8];
    if (!in.read(reinterpret_cast<char*>(dims), 8)) return info;
    info.width  = (dims[0] << 24) | (dims[1] << 16) | (dims[2] << 8) | dims[3];
    info.height = (dims[4] << 24) | (dims[5] << 16) | (dims[6] << 8) | dims[7];

    info.valid = (info.width > 0 && info.height > 0);
    return info;
}

ImageInfo getIcoInfo(const string& path) {
    ImageInfo info;
    ifstream in(path, ios::binary);
    if (!in) return info;

    unsigned char header[6];
    if (!in.read(reinterpret_cast<char*>(header), 6)) return info;
    uint16_t reserved = header[0] | (header[1] << 8);
    uint16_t type     = header[2] | (header[3] << 8);
    uint16_t count    = header[4] | (header[5] << 8);
    if (reserved != 0 || type != 1 || count == 0) return info;

    int bestArea = -1;
    for (uint16_t i = 0; i < count; ++i) {
        unsigned char entry[16];
        if (!in.read(reinterpret_cast<char*>(entry), 16)) break;
        int w = entry[0] == 0 ? 256 : entry[0];
        int h = entry[1] == 0 ? 256 : entry[1];
        int bpp = entry[6] | (entry[7] << 8);
        int area = w * h;
        if (area > bestArea) {
            bestArea = area;
            info.width = w;
            info.height = h;
            info.bitsPerPixel = bpp;
            info.valid = true;
        }
    }
    return info;
}

// ---------------------------------------------------------------------------
// Icon scoring
// ---------------------------------------------------------------------------

int getIconScore(const fs::path& path, const string& gameName, const fs::path& execDir) {
    int score = 0;
    const string pathLower = toLower(path.string());
    const string fLower    = toLower(path.filename().string());
    const string gLower    = toLower(gameName);
    const string extLower  = toLower(path.extension().string());

    // 3.1 Format base score
    if      (extLower == ".svg")  score += 100;
    else if (extLower == ".png")  score += 80;
    else if (extLower == ".ico")  score += 40;
    else if (extLower == ".xpm")  score += 20;
    else if (extLower == ".jpg" || extLower == ".jpeg") score += 10;

    // 3.2 Dimension / bit-depth score
    ImageInfo info;
    if (extLower == ".png") info = getPngInfo(path.string());
    else if (extLower == ".ico") info = getIcoInfo(path.string());

    if (info.valid) {
        if (info.width == info.height) score += 30;
        else score -= 20;

        if      (info.width >= 512) score += 40;
        else if (info.width >= 256) score += 30;
        else if (info.width >= 128) score += 20;
        else if (info.width >= 64)  score += 10;
        else if (info.width > 0 && info.width < 32) score -= 10;

        if (info.bitsPerPixel > 0) {
            if (info.bitsPerPixel <= 1) score -= 40;
            else if (info.bitsPerPixel <= 4) score -= 20;
        }
    }

    // 3.3 Filename match against gameName
    string fBase = fLower.substr(0, fLower.find_last_of('.'));
    if (!gLower.empty() && fBase == gLower) score += 100;
    else if (!gLower.empty() && fLower.find(gLower) != string::npos) score += 50;

    // 3.4 Generic icon/logo filename convention
    if (fLower == "icon.png" || fLower == "icon.svg" || fLower == "logo.png" || fLower == "logo.svg") score += 60;
    else if (fLower.find("icon") != string::npos || fLower.find("logo") != string::npos) score += 20;

    // 3.5 Icon-theme directory convention
    if (pathLower.find("/icons/") != string::npos ||
        pathLower.find("/pixmaps/") != string::npos ||
        pathLower.find("/hicolor/") != string::npos) score += 40;

    // 3.5b Filename-convention fallback for low-quality variants
    static const vector<string> qualityPenalties = {"1bpp","_bw","_mono","_lowres","_small"};
    for (const auto& m : qualityPenalties) if (fLower.find(m) != string::npos) score -= 30;

    // 3.6 Ancestor path match — bounded at execDir (BUG-34 fix)
    if (!gLower.empty()) {
        fs::path p = path.parent_path();
        fs::path stopAt = execDir;
        while (!p.empty() && p != stopAt && p.has_filename()) {
            if (toLower(p.filename().string()) == gLower) {
                score += 50;
                break;
            }
            p = p.parent_path();
        }
    }

    // 3.7 Negative signal categories
    static const vector<string> uiMarkers = {"button","ui","hud","menu","cursor","banner","header","hero","bg","background"};
    static const vector<string> assetMarkers = {"weapon","item","skill","sprite","texture","atlas","char","portrait"};
    static const vector<string> systemMarkers = {"crash","unins","setup","redist","steam_api"};

    for (const auto& m : uiMarkers)     if (pathLower.find(m) != string::npos) score -= 50;
    for (const auto& m : assetMarkers)  if (pathLower.find(m) != string::npos) score -= 50;
    for (const auto& m : systemMarkers) if (pathLower.find(m) != string::npos) score -= 100;

    return score;
}

// ---------------------------------------------------------------------------
// Candidate collection (hard exclusion, no shell)
// ---------------------------------------------------------------------------

static bool isExcludedPath(const string& pathLower) {
    return pathLower.find("steam-runtime") != string::npos ||
           pathLower.find("/usr/") != string::npos ||
           pathLower.find("/help/") != string::npos;
}

static bool hasIconExtension(const fs::path& p) {
    static const vector<string> exts = {".desktop",".svg",".png",".ico",".xpm",".jpg",".jpeg"};
    string ext = toLower(p.extension().string());
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

static vector<fs::path> collectCandidates(const fs::path& execDir) {
    vector<fs::path> files;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(execDir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) continue;
        if (!it->is_regular_file(ec)) continue;
        if (!hasIconExtension(it->path())) continue;
        if (isExcludedPath(toLower(it->path().string()))) continue;
        files.push_back(it->path());
    }
    return files;
}

// ---------------------------------------------------------------------------
// findIcons — the full pipeline
// ---------------------------------------------------------------------------

constexpr int WINNING_SCORE_THRESHOLD = 50;

vector<string> findIcons(const string& execDir, const string& gameName) {
    vector<fs::path> files = collectCandidates(execDir);

    // Stage 2: .desktop shortcut check
    for (const auto& f : files) {
        if (toLower(f.extension().string()) != ".desktop") continue;
        ifstream in(f);
        string line, targetIcon;
        while (std::getline(in, line)) {
            if (line.rfind("Icon=", 0) == 0) { targetIcon = line.substr(5); break; }
        }
        if (targetIcon.empty()) continue;
        if (fs::exists(targetIcon)) return {targetIcon};
        for (const auto& sf : files) {
            string sname = sf.filename().string();
            if (sname == targetIcon || sname == targetIcon + ".png" || sname == targetIcon + ".svg")
                return {sf.string()};
        }
    }

    // Stage 3+4: score and stable-sort
    vector<std::pair<fs::path,int>> scored;
    for (const auto& f : files) {
        if (toLower(f.extension().string()) == ".desktop") continue;
        scored.push_back({f, getIconScore(f, gameName, execDir)});
    }

    std::stable_sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Stage 5: low-confidence warning
    if (!scored.empty() && scored[0].second < WINNING_SCORE_THRESHOLD) {
        cout << PROMPT_COLOR << "No strong game-specific icon match found.\n" << RESET;
        cout << PROMPT_COLOR << "Using best available candidate: " << scored[0].first.string() << "\n" << RESET;
        cout << PROMPT_COLOR << "Override with --icon <path> if this isn't right.\n" << RESET;
    }

    vector<string> ranked;
    for (const auto& s : scored) ranked.push_back(s.first.string());
    return ranked;
}

string extractExeIcon(const string& execPath, const string& safeName) {
    if (!commandExists("wrestool") || !commandExists("icotool")) return "";

    string iconsDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "icons").string();
    fs::create_directories(iconsDir);

    string templatePath = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / ".tmp_icon_XXXXXX").string();
    char* tmpDir = mkdtemp(templatePath.data());
    if (!tmpDir) return "";
    string tmpBase = tmpDir;

    int ret = runCommand({"wrestool", "-x", "-t", "14", execPath, "-o", tmpBase});
    if (ret != 0) {
        fs::remove_all(tmpBase);
        return "";
    }

    string bestIco;
    uintmax_t bestSize = 0;
    try {
        for (const auto& entry : fs::directory_iterator(tmpBase)) {
            if (entry.is_regular_file() && toLower(entry.path().extension().string()) == ".ico") {
                uintmax_t sz = entry.file_size();
                if (sz > bestSize) {
                    bestSize = sz;
                    bestIco = entry.path().string();
                }
            }
        }
    } catch (...) {}

    if (bestIco.empty()) {
        fs::remove_all(tmpBase);
        return "";
    }

    ret = runCommand({"icotool", "-x", "-o", tmpBase, bestIco});
    string bestPng;
    uintmax_t bestPngSize = 0;
    if (ret == 0) {
        try {
            for (const auto& entry : fs::directory_iterator(tmpBase)) {
                if (entry.is_regular_file() && toLower(entry.path().extension().string()) == ".png") {
                    uintmax_t sz = entry.file_size();
                    if (sz > bestPngSize) {
                        bestPngSize = sz;
                        bestPng = entry.path().string();
                    }
                }
            }
        } catch (...) {}
    }

    string outPng = (fs::path(iconsDir) / (safeName + ".png")).string();
    if (!bestPng.empty()) {
        fs::copy_file(bestPng, outPng, fs::copy_options::overwrite_existing);
        fs::remove_all(tmpBase);
        return outPng;
    }

    fs::remove_all(tmpBase);
    return "";
}

string extractHeroicPrefix(const string& execPath) {
    string heroicDir = (fs::path(getHomeDir()) / ".config" / "heroic" / "GamesConfig").string();
    if (!fs::exists(heroicDir)) return "";
    
    for (const auto& entry : fs::directory_iterator(heroicDir)) {
        if (entry.path().extension() == ".json") {
            ifstream in(entry.path());
            string content((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
            if (content.find(execPath) != string::npos) {
                string prefix = extractQuotedJsonValue(content, "\"winePrefix\"");
                if (!prefix.empty()) {
                    return unescapeJsonString(prefix);
                }
            }
        }
    }
    return "";
}

string extractLutrisPrefix(const string& execPath) {
    string lutrisDir = (fs::path(getHomeDir()) / ".config" / "lutris" / "games").string();
    if (!fs::exists(lutrisDir)) return "";
    
    for (const auto& entry : fs::directory_iterator(lutrisDir)) {
        if (entry.path().extension() == ".yml") {
            ifstream in(entry.path());
            string line;
            bool foundExe = false;
            string foundPrefix = "";
            size_t targetIndent = 0;
            bool inTargetBlock = false;
            while (getline(in, line)) {
                if (isCommentOrBlank(line)) continue;
                size_t indent = leadingIndentCount(line);
                if (foundExe && inTargetBlock && indent < targetIndent) {
                    break;
                }
                if (line.find(execPath) != string::npos) {
                    foundExe = true;
                    inTargetBlock = true;
                    targetIndent = indent;
                }
                if (inTargetBlock && startsWithIndentKey(line, "prefix:")) {
                    size_t ppos = line.find(':');
                    if (ppos != string::npos) {
                        foundPrefix = line.substr(ppos + 1);
                        while (!foundPrefix.empty() && isspace(static_cast<unsigned char>(foundPrefix.front()))) {
                            foundPrefix.erase(foundPrefix.begin());
                        }
                    }
                }
            }
            if (foundExe && !foundPrefix.empty()) {
                if (!foundPrefix.empty() && foundPrefix.back() == '\r') foundPrefix.pop_back();
                return foundPrefix;
            }
        }
    }
    return "";
}

string extractSteamPrefix(const string& execPath) {
    string home = getHomeDir();
    vector<string> userdataDirs = {
        (fs::path(home) / ".local" / "share" / "Steam" / "userdata").string(),
        (fs::path(home) / ".var" / "app" / "com.valvesoftware.Steam" / "data" / "Steam" / "userdata").string()
    };
    
    for (const auto& userdataDir : userdataDirs) {
    if (!fs::exists(userdataDir)) continue;
    
    for (const auto& userEntry : fs::directory_iterator(userdataDir)) {
        if (userEntry.is_directory()) {
            string vdfPath = (userEntry.path() / "config" / "shortcuts.vdf").string();
            if (fs::exists(vdfPath)) {
                ifstream in(vdfPath, ios::binary);
                string data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
                
                size_t exePos = data.find(execPath);
                if (exePos != string::npos) {
                    size_t appidStrPos = data.rfind("appid\0", exePos, 6);
                    if (appidStrPos != string::npos) {
                        appidStrPos += 6;
                        if (appidStrPos + 4 <= data.length()) {
                            uint32_t appid;
                            std::memcpy(&appid, data.data() + appidStrPos, sizeof(appid));
                            // Check both native and Flatpak compatdata
                            vector<string> compatdataDirs = {
                                (fs::path(getHomeDir()) / ".local" / "share" / "Steam" / "steamapps" / "compatdata" / to_string(appid)).string(),
                                (fs::path(getHomeDir()) / ".var" / "app" / "com.valvesoftware.Steam" / "data" / "Steam" / "steamapps" / "compatdata" / to_string(appid)).string()
                            };
                            for (const auto& pd : compatdataDirs) {
                                if (fs::exists(pd)) return pd;
                            }
                        }
                    }
                }
            }
        }
    }
    } // end userdataDirs loop
    return "";
}

string autoDiscoverPrefix(const string& execPath) {
    string p = extractSteamPrefix(execPath);
    if (!p.empty()) {
        cout << PROMPT_COLOR << "Discovered existing Steam prefix: " << p << RESET << "\n";
        return p;
    }
    p = extractHeroicPrefix(execPath);
    if (!p.empty()) {
        cout << PROMPT_COLOR << "Discovered existing Heroic prefix: " << p << RESET << "\n";
        return p;
    }
    p = extractLutrisPrefix(execPath);
    if (!p.empty()) {
        cout << PROMPT_COLOR << "Discovered existing Lutris prefix: " << p << RESET << "\n";
        return p;
    }
    return "";
}

string findDesktopFile(const string& query) {
    string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
    string target = toLower(query);
    if (fs::exists(appsDir)) {
        for (const auto& entry : fs::directory_iterator(appsDir)) {
            string fname = entry.path().filename().string();
            if (fname.find("dejatop_") == 0) {
                if (toLower(fname).find(target) != string::npos) {
                    return entry.path().string();
                }
            }
        }
    }
    return "";
}

string extractSteamLaunchOptions(const string& execPath) {
    string home = getHomeDir();
    vector<string> userdataDirs = {
        (fs::path(home) / ".local" / "share" / "Steam" / "userdata").string(),
        (fs::path(home) / ".var" / "app" / "com.valvesoftware.Steam" / "data" / "Steam" / "userdata").string()
    };

    for (const auto& userdataDir : userdataDirs) {
        if (!fs::exists(userdataDir)) continue;

        for (const auto& userEntry : fs::directory_iterator(userdataDir)) {
            if (!userEntry.is_directory()) continue;

            string vdfPath = (userEntry.path() / "config" / "shortcuts.vdf").string();
            if (!fs::exists(vdfPath)) continue;

            ifstream in(vdfPath, ios::binary);
            string data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());

            size_t exePos = data.find(execPath);
            if (exePos == string::npos) continue;

            size_t optionsPos = data.find("LaunchOptions", exePos);
            if (optionsPos == string::npos) continue;

            size_t valueStart = data.find('\0', optionsPos);
            if (valueStart == string::npos || valueStart + 1 >= data.size()) continue;
            ++valueStart;

            size_t valueEnd = valueStart;
            while (valueEnd < data.size() && data[valueEnd] != '\0') ++valueEnd;
            if (valueEnd > valueStart) {
                return data.substr(valueStart, valueEnd - valueStart);
            }
        }
    }

    return "";
}

