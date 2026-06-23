#include "../include/common.hpp"
#include "../include/utils.hpp"
#include "../include/discovery.hpp"
#include "../include/generator.hpp"

void printHelp();

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
    cout << "  --category <name>                   Override auto-detected category.\n";
    cout << "  --env <KEY=VALUE>                   Pass environment variables (repeatable).\n";
    cout << "  --force                             Overwrite existing entry.\n\n";
    cout << "Commands:\n";
    cout << "  --show <name>                       Show details of a managed entry.\n";
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
        string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
        cout << TITLE_COLOR << BOLD << "DejaTop Managed Entries:\n" << RESET;
        bool found = false;
        if (fs::exists(appsDir)) {
            for (const auto& entry : fs::directory_iterator(appsDir)) {
                string fname = entry.path().filename().string();
                if (fname.find("dejatop_") == 0) {
                    cout << "  - " << fname.substr(8, fname.length() - 16) << "\n";
                    found = true;
                }
            }
        }
        if (!found) cout << "  No entries found.\n";
        return 0;
    }
    if (arg1 == "--show" && argc >= 3) {
        string query = argv[2];
        string dFile = findDesktopFile(query);
        if (dFile.empty()) {
            cout << ERROR_COLOR << "Error: No entry found matching '" << query << "'\n" << RESET;
            return 1;
        }

        // Parse .desktop file
        ifstream in(dFile);
        if (!in.is_open()) {
            cout << ERROR_COLOR << "Error: Cannot read " << dFile << "\n" << RESET;
            return 1;
        }
        string name, exec, icon, category, path;
        {
            string line;
            while (getline(in, line)) {
                if (line.rfind("Name=",       0) == 0) name     = line.substr(5);
                if (line.rfind("Exec=",       0) == 0) exec     = line.substr(5);
                if (line.rfind("Icon=",       0) == 0) icon     = line.substr(5);
                if (line.rfind("Categories=", 0) == 0) category = line.substr(11);
                if (line.rfind("Path=",       0) == 0) path     = line.substr(5);
            }
        }
        in.close();

        // Unescape %% -> % in icon/exec for display (escapeDesktop wrote them as %%)
        auto unescapeDesktop = [](const string& s) {
            string out;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '%' && i + 1 < s.size() && s[i+1] == '%') { out += '%'; ++i; }
                else out += s[i];
            }
            return out;
        };
        string iconDisplay = unescapeDesktop(icon);
        string execDisplay = unescapeDesktop(exec);

        // Derive wrapper and log paths from Name= (same logic as creation)
        string safeName = sanitizeName(name);
        string wrapperDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
        string logDir     = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "logs").string();
        string wrapperPath = (fs::path(wrapperDir) / (safeName + "_wrapper.sh")).string();
        string logPath     = (fs::path(logDir)     / (safeName + ".log")).string();

        // Header
        cout << TITLE_COLOR << BOLD << "DejaTop Entry: " << name << RESET << "\n";
        cout << string(40, '-') << "\n";
        cout << "  Desktop:    " << dFile << "\n";
        cout << "  Name:       " << name << "\n";
        cout << "  Category:   " << category << "\n";
        cout << "  Exec:       " << execDisplay << "\n";
        if (!path.empty()) cout << "  Path:       " << path << "\n";

        // Icon with existence check
        if (!iconDisplay.empty()) {
            bool iconOk = fs::exists(iconDisplay);
            cout << "  Icon:       " << iconDisplay;
            if (iconOk) cout << "  " << SUCCESS_COLOR << "\xe2\x9c\x94" << RESET;
            else        cout << "  " << ERROR_COLOR << "\xe2\x9c\x98 (missing \xe2\x80\x94 use --replace-icon to fix)" << RESET;
            cout << "\n";
        }

        // Wrapper: present if Exec= references the wrapper dir
        if (execDisplay.find(wrapperDir) != string::npos) {
            if (fs::exists(wrapperPath)) {
                cout << "  Wrapper:    " << wrapperPath
                     << "  " << SUCCESS_COLOR << "\xe2\x9c\x94" << RESET << "\n";

                // Parse wrapper for prefix and proton path
                ifstream wf(wrapperPath);
                string wline;
                string parsedPrefix, parsedProton;
                while (getline(wf, wline)) {
                    // STEAM_COMPAT_DATA_PATH="<path>"
                    if (wline.rfind("export STEAM_COMPAT_DATA_PATH=", 0) == 0) {
                        size_t q1 = wline.find('"');
                        size_t q2 = wline.rfind('"');
                        if (q1 != string::npos && q2 != q1)
                            parsedPrefix = wline.substr(q1 + 1, q2 - q1 - 1);
                    }
                    // "<proton>/proton" run "<exe>"
                    if (wline.find("/proton\" run") != string::npos) {
                        size_t q1 = wline.find('"');
                        size_t q2 = wline.find("/proton\"");
                        if (q1 != string::npos && q2 != string::npos && q2 > q1)
                            parsedProton = wline.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
                wf.close();
                if (!parsedProton.empty()) cout << "    Proton:   " << parsedProton << "\n";
                if (!parsedPrefix.empty()) cout << "    Prefix:   " << parsedPrefix << "\n";
            } else {
                cout << "  Wrapper:    " << wrapperPath
                     << "  " << ERROR_COLOR << "\xe2\x9c\x98 (missing \xe2\x80\x94 re-run dejatop to regenerate)" << RESET << "\n";
            }
        } else {
            cout << "  Wrapper:    " << PROMPT_COLOR << "(none \xe2\x80\x94 native runner)" << RESET << "\n";
        }

        // Log
        if (fs::exists(logPath)) {
            cout << "  Log:        " << logPath;
            std::error_code ec;
            uintmax_t sz = fs::file_size(logPath, ec);
            if (!ec) {
                if (sz == 0)               cout << "  (empty)";
                else if (sz < 1024)        cout << "  (" << sz << " B)";
                else if (sz < 1024*1024)   cout << "  (" << sz / 1024 << " KB)";
                else                       cout << "  (" << sz / (1024*1024) << " MB)";
            }
            cout << "\n";
        } else {
            cout << "  Log:        " << PROMPT_COLOR << "(none)" << RESET << "\n";
        }

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
        if (iconPath.find('/') != string::npos || iconPath.find('.') != string::npos) {
            if (!fs::exists(iconPath)) {
                cout << ERROR_COLOR << "Error: Icon file '" << iconPath << "' does not exist.\n" << RESET;
                return 1;
            }
            iconPath = fs::canonical(iconPath).string();
        }
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
        string customCategory = "";
        vector<string> envVars;
        bool force = false;
        
        for (int i=2; i<argc; i++) {
            string arg = argv[i];
            if (arg == "--name" && i+1 < argc) gameName = argv[++i];
            else if (arg == "--icon" && i+1 < argc) {
                string tempIcon = argv[++i];
                if (tempIcon.find('/') != string::npos || tempIcon.find('.') != string::npos) {
                    if (!fs::exists(tempIcon)) {
                        cout << ERROR_COLOR << "Error: Icon file '" << tempIcon << "' does not exist.\n" << RESET;
                        return 1;
                    }
                    customIcon = fs::canonical(tempIcon).string();
                } else {
                    customIcon = tempIcon;
                }
            }
            else if (arg == "--runner" && i+1 < argc) customRunner = argv[++i];
            else if (arg == "--prefix" && i+1 < argc) customPrefix = argv[++i];
            else if (arg == "--category" && i+1 < argc) customCategory = argv[++i];
            else if (arg == "--env" && i+1 < argc) envVars.push_back(argv[++i]);
            else if (arg == "--force") force = true;
        }
        
        string safeName = sanitizeName(gameName);
        if (safeName.empty()) {
            cout << ERROR_COLOR << "Error: Parsed name is empty. Please provide a valid --name.\n" << RESET;
            return 1;
        }

        string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
        string desktopPath = (fs::path(appsDir) / ("dejatop_" + safeName + ".desktop")).string();
        if (fs::exists(desktopPath) && !force) {
            cout << ERROR_COLOR << "Error: Entry for '" << safeName << "' already exists. Use --force to overwrite.\n" << RESET;
            return 1;
        }

        cout << TITLE_COLOR << "Processing " << gameName << "...\n" << RESET;
        
        string icon = customIcon;
        if (icon.empty()) {
            auto icons = findIcons(execDir, gameName);
            if (!icons.empty()) icon = icons[0];
        }
        // If no icon found on disk and this is a .exe, try extracting from the binary
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
        
        string category = customCategory;
        if (category.empty()) {
            if (runner == "native") category = "Utility;Application;";
            else category = "Game;";
        }
        
        string finalExec;
        string envPrefix;
        if (!envVars.empty()) {
            envPrefix = "env ";
            for (const auto& env : envVars) {
                envPrefix += escapeBash(env) + " ";
            }
        }

        if (runner == "native") {
            fs::permissions(execPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);
            finalExec = envPrefix + "\"" + execPath + "\"";
        } else if (runner == "wine") {
            finalExec = envPrefix + "wine \"" + execPath + "\"";
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
                        prefix = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "shared_prefix").string();
                    }
                }
                
                string launchOptions = extractSteamLaunchOptions(execPath);
                string wrapperPath = createWrapper(safeName, execDir, execPath, protons[0], prefix, envVars, launchOptions);
                finalExec = wrapperPath;
            }
        }
        
        if (!generateDesktop(gameName, safeName, finalExec, execDir, icon, category)) {
            cout << ERROR_COLOR << "Failed to write desktop entry.\n" << RESET;
            return 1;
        }
        
        if (commandExists("desktop-file-validate")) {
            if (runCommand({"desktop-file-validate", desktopPath}) != 0) {
                cout << ERROR_COLOR << "Warning: desktop-file-validate found issues with the generated entry.\n" << RESET;
            }
        }
        if (commandExists("update-desktop-database")) {
            (void)runCommand({"update-desktop-database", appsDir});
        }
        
        cout << SUCCESS_COLOR << "✔ Desktop entry created successfully!\n" << RESET;
        return 0;
    }

    printHelp();
    return 1;
}

