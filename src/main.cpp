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

