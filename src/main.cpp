#include "../include/common.hpp"
#include "../include/utils.hpp"
#include "../include/discovery.hpp"
#include "../include/generator.hpp"

// ── helpers ──────────────────────────────────────────────────────────────────

// Extract safe name from a .desktop path: "…/dejatop_Portal2.desktop" → "Portal2"
static string safeNameFromPath(const string& dFile) {
    string fname = fs::path(dFile).filename().string();
    if (fname.size() < 17) return ""; // "dejatop_" (8) + at least 1 char + ".desktop" (8)
    return fname.substr(8, fname.size() - 16);
}

// Read a single key from a .desktop file: readKey("…", "Name") → "Portal 2"
static string readKey(const string& dFile, const string& key) {
    ifstream in(dFile);
    if (!in.is_open()) return "";
    string line;
    while (getline(in, line))
        if (line.rfind(key + "=", 0) == 0) return line.substr(key.size() + 1);
    return "";
}

// Run update-desktop-database on the user applications dir
static void refreshDB() {
    string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
    if (commandExists("update-desktop-database"))
        (void)runCommand({"update-desktop-database", appsDir});
}

// ── printHelp ─────────────────────────────────────────────────────────────────

void printHelp() {
    cout << TITLE_COLOR << BOLD << "DejaTop v" << VERSION << RESET << "\n";
    cout << "Seamless Desktop Entry Creator for Linux.\n\n";
    cout << "Usage:\n";
    cout << "  dejatop <path/to/executable> [options]\n\n";
    cout << "Options:\n";
    cout << "  --name <name>                       Override auto-detected name.\n";
    cout << "  --icon <path>                       Override auto-detected icon.\n";
    cout << "  --runner <native|wine|proton>       Override auto-detected runner.\n";
    cout << "  --prefix <path>                     Override Wine/Proton prefix path.\n";
    cout << "  --category <name>                   Set desktop category (default: Game; for\n";
    cout << "                                      Wine/Proton and known game paths,\n";
    cout << "                                      Utility;Application; otherwise).\n";
    cout << "  --env <KEY=VALUE>                   Pass environment variables (repeatable).\n";
    cout << "  --force                             Overwrite existing entry.\n\n";
    cout << "Commands:\n";
    cout << "  --show <name>                       Show details of a managed entry.\n";
    cout << "  --rename <name> <new_name>          Rename a managed entry.\n";
    cout << "  --replace-icon <name> <icon_path>   Swap the icon of a managed entry.\n";
    cout << "  --delete <name>                     Delete a managed entry.\n";
    cout << "  --list                              List managed entries.\n";
    cout << "  --version                           Show version.\n";
    cout << "  --help                              Show this message.\n";
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc <= 1) { printHelp(); return 0; }

    string arg1 = argv[1];

    if (arg1 == "--help" || arg1 == "-h") { printHelp(); return 0; }
    if (arg1 == "--version" || arg1 == "-v") {
        cout << "DejaTop v" << VERSION << "\n";
        return 0;
    }

    // ── --get-latest-proton ───────────────────────────────────────────────────
    if (arg1 == "--get-latest-proton") {
        auto protons = findProtonVersions();
        if (!protons.empty()) {
            cout << protons[0] << "\n";
            return 0;
        }
        return 1;
    }

    auto joinArgs = [&](int startIdx, int endIdx) {
        string res;
        for (int i = startIdx; i < endIdx; ++i) {
            res += argv[i];
            if (i < endIdx - 1) res += " ";
        }
        return res;
    };

    // ── --list ────────────────────────────────────────────────────────────────
    if (arg1 == "--list") {
        string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
        cout << TITLE_COLOR << BOLD << "DejaTop Managed Entries:\n" << RESET;
        bool found = false;
        if (fs::exists(appsDir)) {
            for (const auto& entry : fs::directory_iterator(appsDir)) {
                string fname = entry.path().filename().string();
                if (fname.find("dejatop_") != 0) continue;
                // BP-13: show Name= from file, not sanitized filename
                string displayName = readKey(entry.path().string(), "Name");
                string safeId      = safeNameFromPath(entry.path().string());
                if (!displayName.empty() && displayName != safeId)
                    cout << "  - " << displayName << "  [" << safeId << "]\n";
                else
                    cout << "  - " << safeId << "\n";
                found = true;
            }
        }
        if (!found) cout << "  No entries found.\n";
        return 0;
    }

    // ── --show ────────────────────────────────────────────────────────────────
    if (arg1 == "--show" && argc >= 3) {
        string query = joinArgs(2, argc);
        string dFile = findDesktopFile(query);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << query << "'\n" << RESET;
            return 1;
        }

        ifstream in(dFile);
        if (!in.is_open()) {
            cout << ERROR_COLOR << "Error: Cannot read " << dFile << "\n" << RESET;
            return 1;
        }
        string name, exec, icon, category, path;
        { string line;
          while (getline(in, line)) {
            if (line.rfind("Name=",       0) == 0) name     = line.substr(5);
            if (line.rfind("Exec=",       0) == 0) exec     = line.substr(5);
            if (line.rfind("Icon=",       0) == 0) icon     = line.substr(5);
            if (line.rfind("Categories=", 0) == 0) category = line.substr(11);
            if (line.rfind("Path=",       0) == 0) path     = line.substr(5);
          }
        }
        in.close();

        // Unescape %% -> % for display
        auto unescape = [](const string& s) {
            string out;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '%' && i+1 < s.size() && s[i+1] == '%') { out += '%'; ++i; }
                else out += s[i];
            }
            return out;
        };

        string safeName   = safeNameFromPath(dFile);
        string wrapperDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
        string wrapperPath = (fs::path(wrapperDir) / (safeName + "_wrapper.sh")).string();
        string logPath     = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "logs" / (safeName + ".log")).string();
        string iconDisplay = unescape(icon);
        string execDisplay = unescape(exec);

        cout << TITLE_COLOR << BOLD << "DejaTop Entry: " << name << RESET << "\n";
        cout << string(40, '-') << "\n";
        cout << "  Desktop:    " << dFile << "\n";
        cout << "  Name:       " << name << "\n";
        cout << "  Category:   " << category << "\n";
        cout << "  Exec:       " << execDisplay << "\n";
        if (!path.empty()) cout << "  Path:       " << path << "\n";

        if (!iconDisplay.empty()) {
            cout << "  Icon:       " << iconDisplay;
            if (fs::exists(iconDisplay)) cout << "  " << SUCCESS_COLOR << "\xe2\x9c\x94" << RESET;
            else cout << "  " << ERROR_COLOR << "\xe2\x9c\x98 (missing \xe2\x80\x94 use --replace-icon to fix)" << RESET;
            cout << "\n";
        }

        if (execDisplay.find(wrapperDir) != string::npos) {
            if (fs::exists(wrapperPath)) {
                cout << "  Wrapper:    " << wrapperPath << "  " << SUCCESS_COLOR << "\xe2\x9c\x94" << RESET << "\n";
                ifstream wf(wrapperPath);
                if (wf.is_open()) {
                    string wline, parsedProton, parsedPrefix;
                    while (getline(wf, wline)) {
                        if (wline.rfind("export STEAM_COMPAT_DATA_PATH=", 0) == 0) {
                            size_t q1 = wline.find('"'), q2 = wline.rfind('"');
                            if (q1 != string::npos && q2 != q1)
                                parsedPrefix = wline.substr(q1+1, q2-q1-1);
                        }
                        if (wline.find("/proton\" run") != string::npos) {
                            size_t q1 = wline.find('"'), q2 = wline.find("/proton\"");
                            if (q1 != string::npos && q2 != string::npos && q2 > q1)
                                parsedProton = wline.substr(q1+1, q2-q1-1);
                        }
                    }
                    if (!parsedProton.empty()) cout << "    Proton:   " << parsedProton << "\n";
                    if (!parsedPrefix.empty()) cout << "    Prefix:   " << parsedPrefix << "\n";
                }
            } else {
                cout << "  Wrapper:    " << wrapperPath
                     << "  " << ERROR_COLOR << "\xe2\x9c\x98 (missing \xe2\x80\x94 re-run dejatop to regenerate)" << RESET << "\n";
            }
        } else {
            cout << "  Wrapper:    " << PROMPT_COLOR << "(none \xe2\x80\x94 native runner)" << RESET << "\n";
        }

        if (fs::exists(logPath)) {
            cout << "  Log:        " << logPath;
            std::error_code ec;
            uintmax_t sz = fs::file_size(logPath, ec);
            if (!ec) {
                if (sz == 0)               cout << "  (empty)";
                else if (sz < 1024)        cout << "  (" << sz << " B)";
                else if (sz < 1024*1024)   cout << "  (" << sz/1024 << " KB)";
                else                       cout << "  (" << sz/(1024*1024) << " MB)";
            }
            cout << "\n";
        } else {
            cout << "  Log:        " << PROMPT_COLOR << "(none)" << RESET << "\n";
        }
        return 0;
    }

    // ── --rename ──────────────────────────────────────────────────────────────
    if (arg1 == "--rename" && argc >= 4) {
        string newName = argv[3];
        if (newName.empty()) { cout << ERROR_COLOR << "Error: new name cannot be empty.\n" << RESET; return 1; }

        string dFile = findDesktopFile(argv[2]);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << argv[2] << "'\n" << RESET;
            return 1;
        }

        string oldSafe = safeNameFromPath(dFile);
        string newSafe = sanitizeName(newName);
        if (newSafe.empty()) {
            cout << ERROR_COLOR << "Error: '" << newName << "' produces an empty safe name.\n" << RESET;
            return 1;
        }

        string appsDir    = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
        string wrapperDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
        string logDir     = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "logs").string();
        string newDesktop = (fs::path(appsDir)    / ("dejatop_" + newSafe + ".desktop")).string();
        string oldWrapper = (fs::path(wrapperDir) / (oldSafe + "_wrapper.sh")).string();
        string newWrapper = (fs::path(wrapperDir) / (newSafe + "_wrapper.sh")).string();
        string oldLog     = (fs::path(logDir)     / (oldSafe + ".log")).string();
        string newLog     = (fs::path(logDir)     / (newSafe + ".log")).string();

        // Collision check (only when file would need to be renamed)
        if (oldSafe != newSafe && fs::exists(newDesktop)) {
            cout << ERROR_COLOR << "Error: An entry already exists with safe name '" << newSafe << "'.\n" << RESET;
            return 1;
        }

        // Patch Name= (and Exec= wrapper reference if safe name changes)
        { ifstream in(dFile);
          if (!in.is_open()) {
              cout << ERROR_COLOR << "Error: Cannot read " << dFile << "\n" << RESET;
              return 1;
          }
          string content, line;
          while (getline(in, line)) {
            if (line.rfind("Name=", 0) == 0)
                content += "Name=" + newName + "\n";
            else if (oldSafe != newSafe && line.rfind("Exec=", 0) == 0 &&
                     line.find(oldSafe + "_wrapper.sh") != string::npos) {
                size_t pos;
                while ((pos = line.find(oldSafe + "_wrapper.sh")) != string::npos)
                    line.replace(pos, oldSafe.size() + 11, newSafe + "_wrapper.sh");
                content += line + "\n";
            } else {
                content += line + "\n";
            }
          }
          in.close();
          ofstream out(dFile);
          if (!out.is_open()) {
              cout << ERROR_COLOR << "Error: Cannot write to " << dFile << "\n" << RESET;
              return 1;
          }
          out << content;
        }

        if (oldSafe != newSafe) {
            fs::rename(dFile, newDesktop);

            if (fs::exists(oldWrapper)) {
                // Patch log path and echo label inside wrapper, then rename
                ifstream wf(oldWrapper);
                if (!wf.is_open()) {
                    cout << ERROR_COLOR << "Error: Cannot read " << oldWrapper << "\n" << RESET;
                    return 1;
                }
                string content((istreambuf_iterator<char>(wf)), istreambuf_iterator<char>());
                wf.close();
                auto replaceAll = [](string& s, const string& from, const string& to) {
                    size_t pos = 0;
                    while ((pos = s.find(from, pos)) != string::npos) {
                        s.replace(pos, from.size(), to); pos += to.size();
                    }
                };
                replaceAll(content, oldSafe + ".log", newSafe + ".log");
                replaceAll(content, "Wrapper for " + oldSafe, "Wrapper for " + newSafe);
                ofstream out(newWrapper);
                if (!out.is_open()) {
                    cout << ERROR_COLOR << "Error: Cannot write to " << newWrapper << "\n" << RESET;
                    return 1;
                }
                out << content;
                fs::remove(oldWrapper);
            }
            if (fs::exists(oldLog)) fs::rename(oldLog, newLog);
        }

        cout << SUCCESS_COLOR << "Renamed '" << argv[2] << "' -> '" << newName << "'\n" << RESET;
        refreshDB();
        return 0;
    }



    // ── --delete ──────────────────────────────────────────────────────────────
    if (arg1 == "--delete" && argc >= 3) {
        string query = joinArgs(2, argc);
        string dFile = findDesktopFile(query);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << query << "'\n" << RESET;
            return 1;
        }
        string safeName    = safeNameFromPath(dFile);
        string wrapperPath = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers" / (safeName + "_wrapper.sh")).string();

        // BUG-03 fix: remove wrapper alongside the .desktop file
        fs::remove(dFile);
        cout << SUCCESS_COLOR << "Deleted: " << dFile << RESET << "\n";
        if (fs::exists(wrapperPath)) {
            fs::remove(wrapperPath);
            cout << SUCCESS_COLOR << "Deleted: " << wrapperPath << RESET << "\n";
        }
        refreshDB();
        return 0;
    }

    // ── --replace-icon ────────────────────────────────────────────────────────
    if (arg1 == "--replace-icon" && argc >= 4) {
        string iconPath = argv[3];
        if (iconPath.find('/') != string::npos || iconPath.find('.') != string::npos) {
            if (!fs::exists(iconPath)) {
                cout << ERROR_COLOR << "Error: Icon file '" << iconPath << "' does not exist.\n" << RESET;
                return 1;
            }
            iconPath = fs::canonical(iconPath).string();
        }
        string dFile = findDesktopFile(argv[2]);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << argv[2] << "'\n" << RESET;
            return 1;
        }
        ifstream in(dFile);
        if (!in.is_open()) {
            cout << ERROR_COLOR << "Error: Cannot read " << dFile << "\n" << RESET;
            return 1;
        }
        string content, line;
        bool found = false;
        while (getline(in, line)) {
            if (line.rfind("Icon=", 0) == 0) { line = "Icon=" + iconPath; found = true; }
            content += line + "\n";
        }
        in.close();
        if (!found) content += "Icon=" + iconPath + "\n";
        ofstream out(dFile);
        if (!out.is_open()) {
            cout << ERROR_COLOR << "Error: Cannot write to " << dFile << "\n" << RESET;
            return 1;
        }
        out << content;
        cout << SUCCESS_COLOR << "Icon updated: " << dFile << RESET << "\n";
        return 0;
    }

    // ── One-Shot Creation ─────────────────────────────────────────────────────
    if (arg1[0] != '-') {
        string execPath = arg1;
        if (!fs::exists(execPath)) {
            cout << ERROR_COLOR << "Error: File '" << execPath << "' does not exist.\n" << RESET;
            return 1;
        }
        execPath = fs::canonical(execPath).string();
        fs::path p(execPath);
        string execDir  = p.parent_path().string();
        string execExt  = toLower(p.extension().string());
        string gameName = p.stem().string();

        string customIcon, customRunner, customPrefix, customCategory;
        vector<string> envVars;
        bool force = false;

        for (int i = 2; i < argc; i++) {
            string arg = argv[i];
            if      (arg == "--name"     && i+1 < argc) { gameName     = argv[++i]; }
            else if (arg == "--runner"   && i+1 < argc) { customRunner   = argv[++i]; }
            else if (arg == "--prefix"   && i+1 < argc) { customPrefix   = argv[++i]; }
            else if (arg == "--category" && i+1 < argc) { customCategory = argv[++i]; }
            else if (arg == "--env"      && i+1 < argc) { envVars.push_back(argv[++i]); }
            else if (arg == "--force")                   { force = true; }
            else if (arg == "--icon" && i+1 < argc) {
                string t = argv[++i];
                if (t.find('/') != string::npos || t.find('.') != string::npos) {
                    if (!fs::exists(t)) {
                        cout << ERROR_COLOR << "Error: Icon file '" << t << "' does not exist.\n" << RESET;
                        return 1;
                    }
                    customIcon = fs::canonical(t).string();
                } else {
                    customIcon = t;
                }
            }
            else {
                // BUG-27 fix: error on unrecognized flags instead of silently ignoring
                cout << ERROR_COLOR << "Error: Unknown option '" << arg << "'. Run --help for usage.\n" << RESET;
                return 1;
            }
        }

        string safeName = sanitizeName(gameName);
        if (safeName.empty()) {
            cout << ERROR_COLOR << "Error: Name produces an empty identifier. Provide a valid --name.\n" << RESET;
            return 1;
        }

        string appsDir     = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
        string desktopPath = (fs::path(appsDir) / ("dejatop_" + safeName + ".desktop")).string();
        if (fs::exists(desktopPath) && !force) {
            cout << ERROR_COLOR << "Error: Entry '" << safeName << "' already exists. Use --force to overwrite.\n" << RESET;
            return 1;
        }

        cout << TITLE_COLOR << "Processing " << gameName << "...\n" << RESET;

        string icon = customIcon;
        if (icon.empty()) {
            auto icons = findIcons(execDir, gameName);
            if (!icons.empty()) icon = icons[0];
        }
        if (icon.empty() && execExt == ".exe") {
            string extracted = extractExeIcon(execPath, safeName);
            if (!extracted.empty()) {
                icon = extracted;
                cout << PROMPT_COLOR << "Extracted icon from .exe: " << icon << RESET << "\n";
            }
        }

        string runner = customRunner;
        if (runner.empty()) {
            if (execExt == ".sh" || execExt == ".appimage" || execExt.empty()) runner = "native";
            else runner = "proton";
        }

        // Category auto-detection: Game; for Wine/Proton and known game install paths
        string category = customCategory;
        if (category.empty()) {
            if (runner != "native") {
                category = "Game;";
            } else {
                string edl = toLower(execDir);
                bool looksLikeGame = edl.find("gog")     != string::npos ||
                                     edl.find("/games/") != string::npos ||
                                     edl.find("heroic")  != string::npos ||
                                     edl.find("lutris")  != string::npos ||
                                     edl.find("bottles") != string::npos;
                category = looksLikeGame ? "Game;" : "Utility;Application;";
            }
        }

        // Build env prefix for native/wine Exec= field
        // Values with spaces are double-quoted per freedesktop Exec= spec
        string envPrefix;
        if (!envVars.empty()) {
            envPrefix = "env ";
            for (const auto& env : envVars) {
                if (env.find(' ') != string::npos) {
                    string quoted = "\"";
                    for (char c : env) { if (c == '"' || c == '\\') quoted += '\\'; quoted += c; }
                    envPrefix += quoted + "\" ";
                } else {
                    envPrefix += env + " ";
                }
            }
        }

        string launchOptions = extractSteamLaunchOptions(execPath);
        auto applyLaunchOpts = [&](const string& baseCmd) -> string {
            if (launchOptions.empty()) return baseCmd;
            size_t pos = launchOptions.find("%command%");
            if (pos != string::npos)
                return launchOptions.substr(0, pos) + baseCmd + launchOptions.substr(pos + 9);
            return launchOptions + " " + baseCmd;
        };

        string finalExec;
        if (runner == "native") {
            fs::permissions(execPath,
                fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                fs::perm_options::add);
            finalExec = envPrefix + applyLaunchOpts("\"" + execPath + "\"");
        } else if (runner == "wine") {
            // BUG-11 fix: honour --prefix via WINEPREFIX
            if (!customPrefix.empty())
                finalExec = envPrefix + applyLaunchOpts(
                    "env WINEPREFIX=\"" + customPrefix + "\" wine \"" + execPath + "\"");
            else
                finalExec = envPrefix + applyLaunchOpts("wine \"" + execPath + "\"");
        } else {
            auto protons = findProtonVersions();
            if (protons.empty()) {
                cout << ERROR_COLOR << "Error: Proton not installed. Install via Steam or GE-Proton.\n" << RESET;
                return 1;
            }
            string prefix = customPrefix;
            if (prefix.empty()) {
                prefix = autoDiscoverPrefix(execPath);
                if (prefix.empty()) {
                    cout << "\nNo existing prefix found across Steam, Heroic, or Lutris.\n";
                    cout << "Create and use the default DejaTop shared prefix? [Y/n]: ";
                    string resp;
                    if (getline(cin, resp)) {
                        if (!resp.empty() && toLower(resp)[0] == 'n') {
                            cout << ERROR_COLOR << "Aborted.\n" << RESET; return 1;
                        }
                    }
                    prefix = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "shared_prefix").string();
                }
            }
            string wrapperPath = createWrapper(safeName, execDir, execPath, protons[0], prefix, envVars, launchOptions);
            finalExec = wrapperPath;
        }

        if (!generateDesktop(gameName, safeName, finalExec, execDir, icon, category)) {
            cout << ERROR_COLOR << "Failed to write desktop entry.\n" << RESET; return 1;
        }

        if (commandExists("desktop-file-validate"))
            if (runCommand({"desktop-file-validate", desktopPath}) != 0)
                cout << ERROR_COLOR << "Warning: desktop-file-validate found issues.\n" << RESET;

        refreshDB();
        cout << SUCCESS_COLOR << "\xe2\x9c\x94 Desktop entry created successfully!\n" << RESET;
        return 0;
    }

    printHelp();
    return 1;
}
