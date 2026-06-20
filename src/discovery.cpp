#include "../include/discovery.hpp"
#include <sstream>

bool protonSortCompare(const string& a, const string& b) {
    string nameA = toLower(fs::path(a).filename().string());
    string nameB = toLower(fs::path(b).filename().string());
    
    bool aGE = nameA.find("ge-proton") != string::npos;
    bool bGE = nameB.find("ge-proton") != string::npos;
    if (aGE != bGE) return aGE;
    
    bool aExp = nameA.find("experimental") != string::npos;
    bool bExp = nameB.find("experimental") != string::npos;
    if (aExp != bExp) return aExp;
    
    return nameA > nameB; // Descending for normal versions (e.g. Proton 9.0 > Proton 8.0)
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
                            versions.push_back(entry.path().string());
                        }
                    }
                }
            }
        }
    }
    sort(versions.begin(), versions.end(), protonSortCompare);
    return versions;
}

void getPngDimensions(const string& path, int& w, int& h) {
    w = 0; h = 0;
    ifstream in(path, ios::binary);
    if (!in) return;
    char sig[8];
    if (!in.read(sig, 8)) return;
    if (static_cast<unsigned char>(sig[0]) != 137 || sig[1] != 80 || sig[2] != 78 || sig[3] != 71 ||
        sig[4] != 13 || sig[5] != 10 || sig[6] != 26 || sig[7] != 10) return;
    char chunk[8];
    if (!in.read(chunk, 8)) return;
    if (chunk[4] != 'I' || chunk[5] != 'H' || chunk[6] != 'D' || chunk[7] != 'R') return;
    char dims[8];
    if (!in.read(dims, 8)) return;
    w = (static_cast<unsigned char>(dims[0]) << 24) | (static_cast<unsigned char>(dims[1]) << 16) |
        (static_cast<unsigned char>(dims[2]) << 8) | static_cast<unsigned char>(dims[3]);
    h = (static_cast<unsigned char>(dims[4]) << 24) | (static_cast<unsigned char>(dims[5]) << 16) |
        (static_cast<unsigned char>(dims[6]) << 8) | static_cast<unsigned char>(dims[7]);
}

int getIconScore(const string& path, const string& gameName) {
    int score = 0;
    string pathLower = toLower(path);
    string fLower = toLower(fs::path(path).filename().string());
    string gLower = toLower(gameName);
    string extLower = toLower(fs::path(path).extension().string());

    if (extLower == ".svg") score += 100;
    else if (extLower == ".png") score += 80;
    else if (extLower == ".ico") score += 40;
    else if (extLower == ".xpm") score += 20;

    if (extLower == ".png") {
        int w = 0, h = 0;
        getPngDimensions(path, w, h);
        if (w > 0 && h > 0) {
            if (w == h) score += 30;
            else score -= 20;
            if (w >= 512 && h >= 512) score += 40;
            else if (w >= 256 && h >= 256) score += 30;
            else if (w >= 128 && h >= 128) score += 20;
            else if (w >= 64 && h >= 64) score += 10;
        }
    }

    string fBase = fLower.substr(0, fLower.find_last_of('.'));
    if (fBase == gLower && !gLower.empty()) score += 100;
    else if (!gLower.empty() && fLower.find(gLower) != string::npos) score += 50;

    if (fLower == "icon.png" || fLower == "logo.svg" || fLower == "icon.svg" || fLower == "logo.png") score += 60;
    else if (fLower.find("icon") != string::npos || fLower.find("logo") != string::npos) score += 20;

    if (pathLower.find("/icons/") != string::npos || pathLower.find("/pixmaps/") != string::npos || pathLower.find("/hicolor/") != string::npos) score += 40;

    string parentDir = toLower(fs::path(path).parent_path().filename().string());
    if (parentDir == gLower && !gLower.empty()) score += 20;

    vector<string> uiPenalties = {"button", "ui", "hud", "menu", "cursor", "banner", "header", "hero", "bg", "background"};
    for (const auto& p : uiPenalties) if (pathLower.find(p) != string::npos) score -= 50;
    vector<string> gamePenalties = {"weapon", "item", "skill", "sprite", "texture", "atlas", "char", "portrait"};
    for (const auto& p : gamePenalties) if (pathLower.find(p) != string::npos) score -= 50;
    vector<string> sysPenalties = {"crash", "unins", "setup", "redist", "steam_api"};
    for (const auto& p : sysPenalties) if (pathLower.find(p) != string::npos) score -= 100;

    return score;
}

vector<string> findIcons(const string& execDir) {
    string cmd = "find \"" + execDir + "\" -type f \\( -iname \"*.desktop\" -o -iname \"*.svg\" -o -iname \"*.png\" -o -iname \"*.ico\" -o -iname \"*.xpm\" \\)";
    string out = runCommandOutput(cmd);
    vector<string> files;
    std::stringstream ss(out);
    string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) files.push_back(line);
    }

    for (const auto& f : files) {
        if (toLower(fs::path(f).extension().string()) == ".desktop") {
            ifstream in(f);
            string dline, targetIcon = "";
            while (std::getline(in, dline)) {
                if (dline.find("Icon=") == 0) {
                    targetIcon = dline.substr(5);
                    break;
                }
            }
            if (!targetIcon.empty()) {
                if (fs::exists(targetIcon)) return {targetIcon};
                for (const auto& sf : files) {
                    string sname = fs::path(sf).filename().string();
                    if (sname == targetIcon || sname == targetIcon + ".png" || sname == targetIcon + ".svg") return {sf};
                }
            }
        }
    }

    string gameName = fs::path(execDir).filename().string();
    vector<std::pair<string, int>> scored;
    for (const auto& f : files) {
        if (toLower(fs::path(f).extension().string()) != ".desktop") {
            scored.push_back({f, getIconScore(f, gameName)});
        }
    }
    sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    vector<string> ret;
    for (const auto& s : scored) ret.push_back(s.first);
    return ret;
}

string extractExeIcon(const string& execPath, const string& safeName) {
    if (!commandExists("wrestool") || !commandExists("icotool")) return "";

    string iconsDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "icons").string();
    fs::create_directories(iconsDir);

    string tmpBase = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / (".tmp_icon_" + safeName)).string();
    fs::create_directories(tmpBase);

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
                size_t pos = content.find("\"winePrefix\": \"");
                if (pos != string::npos) {
                    pos += 15;
                    size_t end = content.find("\"", pos);
                    if (end != string::npos) {
                        return content.substr(pos, end - pos);
                    }
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
            while (getline(in, line)) {
                if (line.find(execPath) != string::npos) foundExe = true;
                size_t ppos = line.find("prefix: ");
                if (ppos != string::npos) foundPrefix = line.substr(ppos + 8);
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

