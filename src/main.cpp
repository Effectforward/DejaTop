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
                        prefix = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "shared_prefix").string();
                    }
                }
                
                string wrapperPath = createWrapper(safeName, execDir, execPath, protons[0], prefix);
                finalExec = wrapperPath;
            }
        }
        
        if (!generateDesktop(gameName, safeName, finalExec, execDir, icon)) {
            cout << ERROR_COLOR << "Failed to write desktop entry.\n" << RESET;
            return 1;
        }
        cout << SUCCESS_COLOR << "✔ Desktop entry created successfully!\n" << RESET;
        return 0;
    }

    printHelp();
    return 1;
}

