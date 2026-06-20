#include "../include/discovery.hpp"

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

vector<string> findIcons(const string& execDir) {
    vector<string> bestIcons, goodIcons, okayIcons, badIcons;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(execDir, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                string pathStr = entry.path().string();
                string filename = entry.path().filename().string();
                string ext = entry.path().extension().string();
                
                string fLower = toLower(filename);
                string pLower = toLower(pathStr);

                if (ext == ".png" || ext == ".ico") {
                    if (pLower.find("steam-runtime") != string::npos || pLower.find("/usr/") != string::npos || pLower.find("/help/") != string::npos) {
                        badIcons.push_back(pathStr);
                    } else if (fLower.find("icon") != string::npos || fLower.find("game") != string::npos || fLower.find("logo") != string::npos) {
                        bestIcons.push_back(pathStr);
                    } else if (ext == ".ico") {
                        goodIcons.push_back(pathStr);
                    } else {
                        okayIcons.push_back(pathStr);
                    }
                }
            }
        }
    } catch (const fs::filesystem_error&) {}

    vector<string> allIcons;
    for (const auto& p : bestIcons) allIcons.push_back(p);
    for (const auto& p : goodIcons) allIcons.push_back(p);
    for (const auto& p : okayIcons) allIcons.push_back(p);
    for (const auto& p : badIcons) allIcons.push_back(p);
    return allIcons;
}

string extractExeIcon(const string& execPath, const string& safeName) {
    if (!commandExists("wrestool") || !commandExists("icotool")) return "";

    string iconsDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "icons").string();
    fs::create_directories(iconsDir);

    // Create a temporary working directory
    string tmpBase = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / (".tmp_icon_" + safeName)).string();
    fs::create_directories(tmpBase);

    // Step 1: Extract .ico resources from the .exe (resource type 14 = RT_GROUP_ICON)
    int ret = runCommand({"wrestool", "-x", "-t", "14", execPath, "-o", tmpBase});
    if (ret != 0) {
        fs::remove_all(tmpBase);
        return "";
    }

    // Step 2: Find the largest extracted .ico file (likely the highest quality)
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

    // Step 3: Convert .ico to .png using icotool, requesting the largest size available
    string outPng = (fs::path(iconsDir) / (safeName + ".png")).string();
    ret = runCommand({"icotool", "-x", "-w", "256", "-o", outPng, bestIco});
    if (ret != 0) {
        // Retry without size constraint (some icons don't have 256px)
        ret = runCommand({"icotool", "-x", "-o", outPng, bestIco});
    }

    // Cleanup temp directory
    fs::remove_all(tmpBase);

    if (ret == 0 && fs::exists(outPng)) {
        return outPng;
    }
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

