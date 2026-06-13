#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <stdint.h>

namespace fs = std::filesystem;
using namespace std;

// --- ANSI Escape Codes ---
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define TITLE_COLOR "\033[38;2;203;166;247m"
#define PROMPT_COLOR "\033[38;2;166;227;161m"
#define SUCCESS_COLOR "\033[38;2;166;227;161m"
#define ERROR_COLOR "\033[38;2;243;139;168m"

const string VERSION = "1.0.0-beta";

string getHomeDir() {
    const char* home = getenv("HOME");
    return home ? string(home) : "";
}

string sanitizeName(const string& name) {
    string safe = "";
    for (char c : name) {
        if (isalnum(c) || c == '_' || c == '-') safe += c;
    }
    return safe;
}

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Custom sorting to prioritize GE-Proton, then Experimental, then newest version
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
        home + "/.local/share/Steam/steamapps/common",
        home + "/.steam/steam/steamapps/common",
        home + "/.steam/root/compatibilitytools.d",
        // Flatpak Steam
        home + "/.var/app/com.valvesoftware.Steam/data/Steam/steamapps/common",
        home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam/compatibilitytools.d"
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
    } catch (...) {}

    vector<string> allIcons;
    for (const auto& p : bestIcons) allIcons.push_back(p);
    for (const auto& p : goodIcons) allIcons.push_back(p);
    for (const auto& p : okayIcons) allIcons.push_back(p);
    for (const auto& p : badIcons) allIcons.push_back(p);
    return allIcons;
}

string extractHeroicPrefix(const string& execPath) {
    string heroicDir = getHomeDir() + "/.config/heroic/GamesConfig";
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
    string lutrisDir = getHomeDir() + "/.config/lutris/games";
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
        home + "/.local/share/Steam/userdata",
        home + "/.var/app/com.valvesoftware.Steam/data/Steam/userdata"
    };
    
    for (const auto& userdataDir : userdataDirs) {
    if (!fs::exists(userdataDir)) continue;
    
    for (const auto& userEntry : fs::directory_iterator(userdataDir)) {
        if (userEntry.is_directory()) {
            string vdfPath = userEntry.path().string() + "/config/shortcuts.vdf";
            if (fs::exists(vdfPath)) {
                ifstream in(vdfPath, ios::binary);
                string data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
                
                size_t exePos = data.find(execPath);
                if (exePos != string::npos) {
                    size_t appidStrPos = data.rfind("appid\0", exePos);
                    if (appidStrPos != string::npos) {
                        appidStrPos += 6;
                        if (appidStrPos + 4 <= data.length()) {
                            uint32_t appid = *reinterpret_cast<const uint32_t*>(data.data() + appidStrPos);
                            // Check both native and Flatpak compatdata
                            vector<string> compatdataDirs = {
                                getHomeDir() + "/.local/share/Steam/steamapps/compatdata/" + to_string(appid),
                                getHomeDir() + "/.var/app/com.valvesoftware.Steam/data/Steam/steamapps/compatdata/" + to_string(appid)
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

string createWrapper(const string& safeName, const string& execDir, const string& execPath, const string& selProton, const string& customPrefix) {
    string wrapperDir = getHomeDir() + "/.local/share/DejaTop/wrappers";
    fs::create_directories(wrapperDir);
    string prefixDir = customPrefix.empty() ? getHomeDir() + "/.local/share/DejaTop/shared_prefix" : customPrefix;
    if (customPrefix.empty()) {
        fs::create_directories(prefixDir);
    }
    string wrapperPath = wrapperDir + "/" + safeName + "_wrapper.sh";

    ofstream ofs(wrapperPath);
    ofs << "#!/usr/bin/env bash\n";
    ofs << "export STEAM_COMPAT_DATA_PATH=\"" << prefixDir << "\"\n";
    ofs << "export STEAM_COMPAT_CLIENT_INSTALL_PATH=\"" << getHomeDir() << "/.local/share/Steam\"\n";
    ofs << "cd \"" << execDir << "\"\n";
    ofs << "\"" << selProton << "/proton\" run \"" << execPath << "\"\n";
    ofs.close();

    fs::permissions(wrapperPath, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
    return wrapperPath;
}

void generateDesktop(const string& gameName, const string& safeName, const string& finalExec, const string& execDir, const string& iconPath) {
    string appsDir = getHomeDir() + "/.local/share/applications";
    fs::create_directories(appsDir);
    string desktopPath = appsDir + "/dejatop_" + safeName + ".desktop";

    ofstream dofs(desktopPath);
    dofs << "[Desktop Entry]\n";
    dofs << "Version=1.0\n";
    dofs << "Type=Application\n";
    dofs << "Name=" << gameName << "\n";
    dofs << "Comment=Created by DejaTop\n";
    dofs << "Exec=" << finalExec << "\n";
    dofs << "Path=" << execDir << "\n"; // Removed double quotes to fix spaces issue
    if (!iconPath.empty()) {
        dofs << "Icon=" << iconPath << "\n";
    }
    dofs << "Terminal=false\n";
    dofs << "Categories=Game;Utility;Application;\n";
    dofs.close();
}

string findDesktopFile(const string& query) {
    string appsDir = getHomeDir() + "/.local/share/applications";
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

void printHelp() {
    cout << TITLE_COLOR << BOLD << "DejaTop v" << VERSION << RESET << "\n";
    cout << "Seamless Desktop Entry Creator for Linux.\n\n";
    cout << "Usage:\n";
    cout << "  dejatop <path/to/executable> [options]\n\n";
    cout << "Options:\n";
    cout << "  --name <name>                       Override auto-detected name.\n";
    cout << "  --icon <path>                       Override auto-detected icon.\n";
    cout << "  --runner <native|wine|proton>       Override auto-detected runner.\n";
    cout << "  --prefix <path>                     Override Wine/Proton prefix path.\n\n";
    cout << "Commands:\n";
    cout << "  --replace-icon <name> <icon_path>   Case-insensitively swap an icon.\n";
    cout << "  --delete <name>                     Delete an entry and its wrapper.\n";
    cout << "  --list                              List managed entries.\n";
    cout << "  --version                           Show version.\n";
    cout << "  --help                              Show this message.\n";
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printHelp();
        return 0;
    }

    string arg1 = argv[1];
    if (arg1 == "--help" || arg1 == "-h") {
        printHelp();
        return 0;
    }
    if (arg1 == "--version" || arg1 == "-v") {
        cout << "DejaTop v" << VERSION << "\n";
        return 0;
    }
    if (arg1 == "--list") {
        string appsDir = getHomeDir() + "/.local/share/applications";
        cout << TITLE_COLOR << BOLD << "DejaTop Managed Entries:\n" << RESET;
        bool found = false;
        if (fs::exists(appsDir)) {
            for (const auto& entry : fs::directory_iterator(appsDir)) {
                string fname = entry.path().filename().string();
                if (fname.find("dejatop_") == 0 && fname != "dejatop.desktop") {
                    cout << "  - " << fname.substr(8, fname.length() - 16) << "\n";
                    found = true;
                }
            }
        }
        if (!found) cout << "  No entries found.\n";
        return 0;
    }
    if (arg1 == "--delete" && argc >= 3) {
        string query = argv[2];
        string dFile = findDesktopFile(query);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << query << "'\n" << RESET;
            return 1;
        }
        fs::remove(dFile);
        cout << SUCCESS_COLOR << "Deleted: " << dFile << RESET << "\n";
        return 0;
    }
    if (arg1 == "--replace-icon" && argc >= 4) {
        string query = argv[2];
        string iconPath = argv[3];
        string dFile = findDesktopFile(query);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << query << "'\n" << RESET;
            return 1;
        }
        
        ifstream in(dFile);
        string content, line;
        bool iconFound = false;
        while (getline(in, line)) {
            if (line.find("Icon=") == 0) {
                line = "Icon=" + iconPath;
                iconFound = true;
            }
            content += line + "\n";
        }
        in.close();
        
        if (!iconFound) {
            content += "Icon=" + iconPath + "\n";
        }
        
        ofstream out(dFile);
        out << content;
        out.close();
        
        cout << SUCCESS_COLOR << "Successfully replaced icon for " << dFile << RESET << "\n";
        return 0;
    }
    
    // One-Shot Magic Creation
    if (arg1[0] != '-') {
        string execPath = arg1;
        if (!fs::exists(execPath)) {
            cout << ERROR_COLOR << "Error: File '" << execPath << "' does not exist.\n" << RESET;
            return 1;
        }
        execPath = fs::canonical(execPath).string();
        fs::path p(execPath);
        string execDir = p.parent_path().string();
        string execExt = toLower(p.extension().string());
        
        string gameName = p.stem().string();
        string customIcon = "";
        string customRunner = "";
        string customPrefix = "";
        
        for (int i=2; i<argc; i++) {
            string arg = argv[i];
            if (arg == "--name" && i+1 < argc) gameName = argv[++i];
            else if (arg == "--icon" && i+1 < argc) customIcon = argv[++i];
            else if (arg == "--runner" && i+1 < argc) customRunner = argv[++i];
            else if (arg == "--prefix" && i+1 < argc) customPrefix = argv[++i];
        }
        
        string safeName = sanitizeName(gameName);
        cout << TITLE_COLOR << "Processing " << gameName << "...\n" << RESET;
        
        string icon = customIcon;
        if (icon.empty()) {
            auto icons = findIcons(execDir);
            if (!icons.empty()) icon = icons[0];
        }
        
        string runner = customRunner;
        if (runner.empty()) {
            if (execExt == ".sh" || execExt == ".appimage" || execExt.empty()) runner = "native";
            else runner = "proton";
        }
        
        string finalExec;
        if (runner == "native") {
            finalExec = "\"" + execPath + "\"";
        } else if (runner == "wine") {
            finalExec = "wine \"" + execPath + "\"";
        } else {
            auto protons = findProtonVersions();
            if (protons.empty()) {
                cout << ERROR_COLOR << "Error: Proton not installed. Please install Proton via Steam or GE-Proton.\n" << RESET;
                return 1;
            } else {
                string prefix = customPrefix;
                if (prefix.empty()) {
                    prefix = autoDiscoverPrefix(execPath);
                    if (prefix.empty()) {
                        cout << "\nNo existing prefix was found for this game across Steam, Heroic, or Lutris.\n";
                        cout << "Would you like to create and use the default DejaTop shared prefix? [Y/n]: ";
                        string resp;
                        if (getline(cin, resp)) {
                            if (!resp.empty() && toLower(resp)[0] == 'n') {
                                cout << ERROR_COLOR << "Aborted.\n" << RESET;
                                return 1;
                            }
                        }
                        prefix = getHomeDir() + "/.local/share/DejaTop/shared_prefix";
                    }
                }
                
                string wrapperPath = createWrapper(safeName, execDir, execPath, protons[0], prefix);
                finalExec = wrapperPath;
            }
        }
        
        generateDesktop(gameName, safeName, finalExec, execDir, icon);
        cout << SUCCESS_COLOR << "✔ Desktop entry created successfully!\n" << RESET;
        return 0;
    }

    printHelp();
    return 1;
}
