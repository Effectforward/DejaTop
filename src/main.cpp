#include "../include/common.hpp"
#include "../include/utils.hpp"
#include "../include/discovery.hpp"
#include "../include/generator.hpp"

// ── helpers ──────────────────────────────────────────────────────────────────

namespace {

constexpr int EXIT_USAGE = 2;

struct ManagedEntry {
    string path;
    string id;
    string name;
};

enum class MatchKind { One, None, Ambiguous };

struct MatchResult {
    MatchKind kind = MatchKind::None;
    ManagedEntry entry;
    vector<ManagedEntry> candidates;
};

enum class Cmd { Create, List, Show, Rename, ReplaceIcon, Delete, GetLatestProton, Help, Version };

struct Cli {
    Cmd cmd = Cmd::Create;
    bool cmdSet = false;
    vector<string> positional;
    string name;
    string runner;
    string prefix;
    string category;
    string icon;
    vector<string> env;
    bool nameSet = false;
    bool runnerSet = false;
    bool force = false;
};

struct ParseOutcome {
    Cli cli;
    int error = 0;
    string message;
};

string applicationsDir() {
    return (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
}

string joinFrom(const vector<string>& args, size_t start) {
    string out;
    for (size_t i = start; i < args.size(); ++i) {
        if (i > start) out += " ";
        out += args[i];
    }
    return out;
}

// Extract safe name from a .desktop path: "…/dejatop_Portal2.desktop" → "Portal2"
string safeNameFromPath(const string& dFile) {
    string fname = fs::path(dFile).filename().string();
    if (fname.size() < 17) return "";
    return fname.substr(8, fname.size() - 16);
}

string readKey(const string& dFile, const string& key) {
    ifstream in(dFile);
    if (!in.is_open()) return "";
    string line;
    while (getline(in, line))
        if (line.rfind(key + "=", 0) == 0) return line.substr(key.size() + 1);
    return "";
}

string unescapeDesktopValue(const string& s) {
    string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '\\') { out += '\\'; ++i; continue; }
            if (next == 'n')  { out += '\n'; ++i; continue; }
            if (next == 't')  { out += '\t'; ++i; continue; }
            if (next == 'r')  { out += '\r'; ++i; continue; }
            if (next == 's')  { out += ' ';  ++i; continue; }
        }
        out += s[i];
    }
    return out;
}

// An entry is managed only if it carries DejaTop's marker. The filename prefix
// alone is not ownership (F02).
bool isManagedEntry(const string& path) {
    return readKey(path, "Comment") == "Created by DejaTop";
}

vector<ManagedEntry> listManagedEntries() {
    vector<ManagedEntry> entries;
    const string appsDir = applicationsDir();
    if (!fs::exists(appsDir)) return entries;

    for (const auto& de : fs::directory_iterator(appsDir)) {
        const string path = de.path().string();
        const string fname = de.path().filename().string();
        if (fname.rfind("dejatop_", 0) != 0 || fname.size() < 17) continue;
        if (!de.is_regular_file()) continue;
        if (!isManagedEntry(path)) continue;

        ManagedEntry e;
        e.path = path;
        e.id = safeNameFromPath(path);
        e.name = unescapeDesktopValue(readKey(path, "Name"));
        entries.push_back(e);
    }

    sort(entries.begin(), entries.end(), [](const ManagedEntry& a, const ManagedEntry& b) {
        const string la = toLower(a.id), lb = toLower(b.id);
        return la == lb ? a.id < b.id : la < lb;
    });
    return entries;
}

// Exact id, then exact display name (case-insensitive), then a unique substring.
MatchResult lookupEntry(const string& query) {
    MatchResult result;
    const string q = toLower(query);
    const vector<ManagedEntry> all = listManagedEntries();

    for (const auto& e : all)
        if (toLower(e.id) == q) { result.kind = MatchKind::One; result.entry = e; return result; }
    for (const auto& e : all)
        if (toLower(e.name) == q) { result.kind = MatchKind::One; result.entry = e; return result; }

    for (const auto& e : all)
        if (toLower(e.name).find(q) != string::npos || toLower(e.id).find(q) != string::npos)
            result.candidates.push_back(e);

    if (result.candidates.size() == 1) {
        result.kind = MatchKind::One;
        result.entry = result.candidates.front();
    } else if (result.candidates.empty()) {
        result.kind = MatchKind::None;
    } else {
        result.kind = MatchKind::Ambiguous;
    }
    return result;
}

int usageError(const string& message) {
    cerr << ERROR_COLOR << "Error: " << message << RESET << "\n";
    return EXIT_USAGE;
}

int lookupFailure(const string& query, const MatchResult& match) {
    if (match.kind == MatchKind::None) {
        cerr << ERROR_COLOR << "Error: No entry found matching '" << query << "'\n" << RESET;
        return 1;
    }
    cerr << ERROR_COLOR << "Error: '" << query << "' matches more than one entry:\n" << RESET;
    for (const auto& e : match.candidates)
        cerr << "  - " << stripNewlines(e.name) << "  [" << e.id << "]\n";
    return EXIT_USAGE;
}

// Run update-desktop-database on the user applications dir.
void refreshDB() {
    string appsDir = applicationsDir();
    if (commandExists("update-desktop-database"))
        (void)runCommand({"update-desktop-database", appsDir});
}

bool resolveIcon(string& icon) {
    if (icon.find('/') == string::npos && icon.find('.') == string::npos) return true;

    std::error_code ec;
    if (!fs::exists(icon, ec)) return false;
    const fs::path resolved = fs::canonical(icon, ec);
    if (ec) return false;
    icon = resolved.string();
    return true;
}

// Add owner-execute only. The old code added group/other bits too (F15).
void ensureExecutable(const string& path) {
    std::error_code ec;
    const fs::file_status st = fs::status(path, ec);
    if (ec) return;
    if ((st.permissions() & fs::perms::owner_exec) != fs::perms::none) return;

    fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add, ec);
    if (!ec)
        cout << PROMPT_COLOR << "Made executable: " << path << RESET << "\n";
    else
        cerr << ERROR_COLOR << "Warning: could not make '" << path << "' executable.\n" << RESET;
}

ParseOutcome parseArgs(int argc, char* argv[]) {
    ParseOutcome out;
    Cli& cli = out.cli;
    bool endOfOptions = false;

    auto fail = [&](const string& message) {
        out.error = EXIT_USAGE;
        out.message = message;
        return out;
    };
    auto setCommand = [&](Cmd cmd) -> bool {
        if (cli.cmdSet && cli.cmd != cmd) {
            out.error = EXIT_USAGE;
            out.message = "Only one command may be used at a time";
            return false;
        }
        cli.cmd = cmd;
        cli.cmdSet = true;
        return true;
    };

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (!endOfOptions && arg == "--") { endOfOptions = true; continue; }
        if (endOfOptions || arg.empty() || arg[0] != '-') {
            cli.positional.push_back(arg);
            continue;
        }

        // --option=value
        string opt = arg;
        string inlineValue;
        bool hasInline = false;
        if (arg.rfind("--", 0) == 0) {
            const size_t eq = arg.find('=');
            if (eq != string::npos) {
                opt = arg.substr(0, eq);
                inlineValue = arg.substr(eq + 1);
                hasInline = true;
            }
        }

        if (opt == "--help" || opt == "-h") {
            if (hasInline) return fail(opt + " does not take a value");
            if (!setCommand(Cmd::Help)) return out;
            continue;
        }
        if (opt == "--version" || opt == "-v") {
            if (hasInline) return fail(opt + " does not take a value");
            if (!setCommand(Cmd::Version)) return out;
            continue;
        }
        if (opt == "--list") {
            if (hasInline) return fail("--list does not take a value");
            if (!setCommand(Cmd::List)) return out;
            continue;
        }
        if (opt == "--show") {
            if (hasInline) return fail("--show does not take a value");
            if (!setCommand(Cmd::Show)) return out;
            continue;
        }
        if (opt == "--rename") {
            if (hasInline) return fail("--rename does not take a value");
            if (!setCommand(Cmd::Rename)) return out;
            continue;
        }
        if (opt == "--replace-icon") {
            if (hasInline) return fail("--replace-icon does not take a value");
            if (!setCommand(Cmd::ReplaceIcon)) return out;
            continue;
        }
        if (opt == "--delete") {
            if (hasInline) return fail("--delete does not take a value");
            if (!setCommand(Cmd::Delete)) return out;
            continue;
        }
        if (opt == "--get-latest-proton") {
            if (hasInline) return fail("--get-latest-proton does not take a value");
            if (!setCommand(Cmd::GetLatestProton)) return out;
            continue;
        }
        if (opt == "--force") {
            if (hasInline) return fail("--force does not take a value");
            cli.force = true;
            continue;
        }

        auto takeValue = [&](string& destination) -> bool {
            if (hasInline) { destination = inlineValue; return true; }
            if (i + 1 >= argc) return false;
            const string next = argv[i + 1];
            if (next.rfind("--", 0) == 0) return false;
            destination = argv[++i];
            return true;
        };

        if (opt == "--name") {
            if (!takeValue(cli.name)) return fail("--name requires a value");
            cli.nameSet = true;
            continue;
        }
        if (opt == "--runner") {
            if (!takeValue(cli.runner)) return fail("--runner requires a value");
            cli.runnerSet = true;
            continue;
        }
        if (opt == "--prefix") {
            if (!takeValue(cli.prefix)) return fail("--prefix requires a value");
            continue;
        }
        if (opt == "--category") {
            if (!takeValue(cli.category)) return fail("--category requires a value");
            continue;
        }
        if (opt == "--env") {
            string value;
            if (!takeValue(value)) return fail("--env requires a value");
            cli.env.push_back(value);
            continue;
        }
        if (opt == "--icon") {
            if (!takeValue(cli.icon)) return fail("--icon requires a value");
            continue;
        }

        return fail("Unknown option '" + arg + "'");
    }

    if (cli.runnerSet && cli.runner != "native" && cli.runner != "wine" && cli.runner != "proton")
        return fail("Invalid runner '" + cli.runner + "' (expected native, wine, or proton)");

    // Validate --env before any command touches the filesystem.
    for (const auto& env : cli.env) {
        const size_t eq = env.find('=');
        if (eq == string::npos)
            return fail("--env requires KEY=VALUE (missing '=')");
        const string key = env.substr(0, eq);
        if (!validEnvKey(key))
            return fail("Invalid --env key '" + key + "' (expected [A-Za-z_][A-Za-z0-9_]*)");
    }

    return out;
}

} // namespace

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

// ── commands ──────────────────────────────────────────────────────────────────

namespace {

int commandList() {
    cout << TITLE_COLOR << BOLD << "DejaTop Managed Entries:\n" << RESET;
    const vector<ManagedEntry> entries = listManagedEntries();
    if (entries.empty()) {
        cout << "  No entries found.\n";
        return 0;
    }
    for (const auto& e : entries) {
        const string display = stripNewlines(e.name);
        if (!display.empty() && display != e.id)
            cout << "  - " << display << "  [" << e.id << "]\n";
        else
            cout << "  - " << e.id << "\n";
    }
    return 0;
}

int commandShow(const string& query) {
    if (query.empty()) return usageError("Entry name cannot be empty");
    const MatchResult match = lookupEntry(query);
    if (match.kind != MatchKind::One) return lookupFailure(query, match);
    const ManagedEntry& e = match.entry;

    ifstream in(e.path);
    if (!in.is_open()) {
        cerr << ERROR_COLOR << "Error: Cannot read " << e.path << "\n" << RESET;
        return 1;
    }
    string exec, icon, category, path;
    { string line;
      while (getline(in, line)) {
        if (line.rfind("Exec=",       0) == 0) exec     = line.substr(5);
        if (line.rfind("Icon=",       0) == 0) icon     = line.substr(5);
        if (line.rfind("Categories=", 0) == 0) category = line.substr(11);
        if (line.rfind("Path=",       0) == 0) path     = line.substr(5);
      }
    }

    auto unescape = [](const string& s) {
        string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%' && i + 1 < s.size() && s[i + 1] == '%') { out += '%'; ++i; }
            else out += s[i];
        }
        return out;
    };

    const string wrapperDir  = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
    const string wrapperPath = (fs::path(wrapperDir) / (e.id + "_wrapper.sh")).string();
    const string logPath     = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "logs" / (e.id + ".log")).string();
    const string iconDisplay = unescape(icon);
    const string execDisplay = unescape(exec);

    cout << TITLE_COLOR << BOLD << "DejaTop Entry: " << e.name << RESET << "\n";
    cout << string(40, '-') << "\n";
    cout << "  Desktop:    " << e.path << "\n";
    cout << "  Name:       " << e.name << "\n";
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
                            parsedPrefix = wline.substr(q1 + 1, q2 - q1 - 1);
                    }
                    if (wline.find("/proton\" run") != string::npos) {
                        size_t q1 = wline.find('"'), q2 = wline.find("/proton\"");
                        if (q1 != string::npos && q2 != string::npos && q2 > q1)
                            parsedProton = wline.substr(q1 + 1, q2 - q1 - 1);
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
            else if (sz < 1024 * 1024) cout << "  (" << sz / 1024 << " KB)";
            else                       cout << "  (" << sz / (1024 * 1024) << " MB)";
        }
        cout << "\n";
    } else {
        cout << "  Log:        " << PROMPT_COLOR << "(none)" << RESET << "\n";
    }
    return 0;
}

int commandRename(const string& query, const string& newName) {
    if (query.empty()) return usageError("Entry name cannot be empty");
    if (newName.empty()) {
        cerr << ERROR_COLOR << "Error: new name cannot be empty.\n" << RESET;
        return 1;
    }

    const MatchResult match = lookupEntry(query);
    if (match.kind != MatchKind::One) return lookupFailure(query, match);
    const ManagedEntry& e = match.entry;

    const string newSafe = makeSafeId(newName);
    if (newSafe.empty()) {
        cerr << ERROR_COLOR << "Error: '" << newName << "' produces an empty safe name.\n" << RESET;
        return 1;
    }

    const string appsDir    = applicationsDir();
    const string wrapperDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
    const string logDir     = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "logs").string();
    const string newDesktop = (fs::path(appsDir)    / ("dejatop_" + newSafe + ".desktop")).string();
    const string oldWrapper = (fs::path(wrapperDir) / (e.id + "_wrapper.sh")).string();
    const string newWrapper = (fs::path(wrapperDir) / (newSafe + "_wrapper.sh")).string();
    const string oldLog     = (fs::path(logDir)     / (e.id + ".log")).string();
    const string newLog     = (fs::path(logDir)     / (newSafe + ".log")).string();

    if (e.id != newSafe && fs::exists(newDesktop)) {
        cerr << ERROR_COLOR << "Error: An entry already exists with safe name '" << newSafe << "'.\n" << RESET;
        return 1;
    }

    { ifstream in(e.path);
      if (!in.is_open()) {
          cerr << ERROR_COLOR << "Error: Cannot read " << e.path << "\n" << RESET;
          return 1;
      }
      string content, line;
      while (getline(in, line)) {
        if (line.rfind("Name=", 0) == 0)
            content += "Name=" + escapeDesktopValue(newName) + "\n";
        else if (e.id != newSafe && line.rfind("Exec=", 0) == 0 &&
                 line.find(e.id + "_wrapper.sh") != string::npos) {
            size_t pos;
            while ((pos = line.find(e.id + "_wrapper.sh")) != string::npos)
                line.replace(pos, e.id.size() + 11, newSafe + "_wrapper.sh");
            content += line + "\n";
        } else {
            content += line + "\n";
        }
      }
      in.close();
      ofstream out(e.path);
      if (!out.is_open()) {
          cerr << ERROR_COLOR << "Error: Cannot write to " << e.path << "\n" << RESET;
          return 1;
      }
      out << content;
    }

    if (e.id != newSafe) {
        fs::rename(e.path, newDesktop);

        if (fs::exists(oldWrapper)) {
            ifstream wf(oldWrapper);
            if (!wf.is_open()) {
                cerr << ERROR_COLOR << "Error: Cannot read " << oldWrapper << "\n" << RESET;
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
            replaceAll(content, e.id + ".log", newSafe + ".log");
            replaceAll(content, "Wrapper for " + e.id, "Wrapper for " + newSafe);
            ofstream out(newWrapper);
            if (!out.is_open()) {
                cerr << ERROR_COLOR << "Error: Cannot write to " << newWrapper << "\n" << RESET;
                return 1;
            }
            out << content;
            fs::remove(oldWrapper);
        }
        if (fs::exists(oldLog)) fs::rename(oldLog, newLog);
    }

    cout << SUCCESS_COLOR << "Renamed '" << query << "' -> '" << newName << "'\n" << RESET;
    refreshDB();
    return 0;
}

int commandDelete(const string& query) {
    if (query.empty()) return usageError("Entry name cannot be empty");
    const MatchResult match = lookupEntry(query);
    if (match.kind != MatchKind::One) return lookupFailure(query, match);
    const ManagedEntry& e = match.entry;
    const string wrapperPath = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers" / (e.id + "_wrapper.sh")).string();

    std::error_code ec;
    fs::remove(e.path, ec);
    if (ec) {
        cerr << ERROR_COLOR << "Error: Cannot delete " << e.path << ": " << ec.message() << "\n" << RESET;
        return 1;
    }
    cout << SUCCESS_COLOR << "Deleted: " << e.path << RESET << "\n";
    if (fs::exists(wrapperPath)) {
        fs::remove(wrapperPath, ec);
        if (!ec) cout << SUCCESS_COLOR << "Deleted: " << wrapperPath << RESET << "\n";
    }
    refreshDB();
    return 0;
}

int commandReplaceIcon(const string& query, string iconPath) {
    if (query.empty()) return usageError("Entry name cannot be empty");
    const MatchResult match = lookupEntry(query);
    if (match.kind != MatchKind::One) return lookupFailure(query, match);

    if (!resolveIcon(iconPath)) {
        cerr << ERROR_COLOR << "Error: Icon file '" << iconPath << "' does not exist.\n" << RESET;
        return 1;
    }

    const string& dFile = match.entry.path;
    ifstream in(dFile);
    if (!in.is_open()) {
        cerr << ERROR_COLOR << "Error: Cannot read " << dFile << "\n" << RESET;
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
        cerr << ERROR_COLOR << "Error: Cannot write to " << dFile << "\n" << RESET;
        return 1;
    }
    out << content;
    cout << SUCCESS_COLOR << "Icon updated: " << dFile << RESET << "\n";
    return 0;
}

int commandCreate(const Cli& cli, const string& target) {
    std::error_code ec;
    if (!fs::exists(target, ec)) {
        cerr << ERROR_COLOR << "Error: File '" << target << "' does not exist.\n" << RESET;
        return 1;
    }
    if (!fs::is_regular_file(target, ec)) {
        cerr << ERROR_COLOR << "Error: '" << target << "' is not a regular file.\n" << RESET;
        return 1;
    }

    const fs::path resolved = fs::canonical(target, ec);
    if (ec) {
        cerr << ERROR_COLOR << "Error: Cannot resolve '" << target << "': " << ec.message() << "\n" << RESET;
        return 1;
    }
    const string execPath = resolved.string();
    const fs::path p(execPath);
    const string execDir = p.parent_path().string();
    const string execExt = toLower(p.extension().string());

    const string gameName = cli.nameSet ? cli.name : p.stem().string();
    const string safeName = makeSafeId(gameName);
    if (safeName.empty()) {
        cerr << ERROR_COLOR << "Error: Name produces an empty identifier. Provide a valid --name.\n" << RESET;
        return 1;
    }

    const string appsDir = applicationsDir();
    const string desktopPath = (fs::path(appsDir) / ("dejatop_" + safeName + ".desktop")).string();
    if (fs::exists(desktopPath) && !cli.force) {
        cerr << ERROR_COLOR << "Error: Entry '" << safeName << "' already exists. Use --force to overwrite.\n" << RESET;
        return 1;
    }

    cout << TITLE_COLOR << "Processing " << gameName << "...\n" << RESET;

    string icon = cli.icon;
    if (!icon.empty() && !resolveIcon(icon)) {
        cerr << ERROR_COLOR << "Error: Icon file '" << icon << "' does not exist.\n" << RESET;
        return 1;
    }
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

    string runner = cli.runner;
    if (runner.empty()) {
        if (execExt == ".sh" || execExt == ".appimage" || execExt.empty()) runner = "native";
        else runner = "proton";
    }

    string category = cli.category;
    if (category.empty()) {
        if (runner != "native") {
            category = "Game;";
        } else {
            const string edl = toLower(execDir);
            const bool looksLikeGame = edl.find("gog")     != string::npos ||
                                       edl.find("/games/") != string::npos ||
                                       edl.find("heroic")  != string::npos ||
                                       edl.find("lutris")  != string::npos ||
                                       edl.find("bottles") != string::npos;
            category = looksLikeGame ? "Game;" : "Utility;Application;";
        }
    }

    string envPrefix;
    if (!cli.env.empty()) {
        envPrefix = "env ";
        for (const auto& env : cli.env) {
            if (env.find(' ') != string::npos) {
                string quoted = "\"";
                for (char c : env) { if (c == '"' || c == '\\') quoted += '\\'; quoted += c; }
                envPrefix += quoted + "\" ";
            } else {
                envPrefix += env + " ";
            }
        }
    }

    const string launchOptions = extractSteamLaunchOptions(execPath);
    auto applyLaunchOpts = [&](const string& baseCmd) -> string {
        if (launchOptions.empty()) return baseCmd;
        size_t pos = launchOptions.find("%command%");
        if (pos != string::npos)
            return launchOptions.substr(0, pos) + baseCmd + launchOptions.substr(pos + 9);
        return launchOptions + " " + baseCmd;
    };

    string finalExec;
    if (runner == "native") {
        ensureExecutable(execPath);
        finalExec = envPrefix + applyLaunchOpts("\"" + execPath + "\"");
    } else if (runner == "wine") {
        if (!cli.prefix.empty())
            finalExec = envPrefix + applyLaunchOpts(
                "env WINEPREFIX=\"" + cli.prefix + "\" wine \"" + execPath + "\"");
        else
            finalExec = envPrefix + applyLaunchOpts("wine \"" + execPath + "\"");
    } else {
        auto protons = findProtonVersions();
        if (protons.empty()) {
            cerr << ERROR_COLOR << "Error: Proton not installed. Install via Steam or GE-Proton.\n" << RESET;
            return 1;
        }
        string prefix = cli.prefix;
        if (prefix.empty()) {
            prefix = autoDiscoverPrefix(execPath);
            if (prefix.empty()) {
                cout << "\nNo existing prefix found across Steam, Heroic, or Lutris.\n";
                cout << "Create and use the default DejaTop shared prefix? [Y/n]: ";
                string resp;
                if (getline(cin, resp)) {
                    if (!resp.empty() && toLower(resp)[0] == 'n') {
                        cerr << ERROR_COLOR << "Aborted.\n" << RESET;
                        return 1;
                    }
                }
                prefix = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "shared_prefix").string();
            }
        }
        string wrapperPath = createWrapper(safeName, execDir, execPath, prefix, cli.env, launchOptions);
        finalExec = wrapperPath;
    }

    if (!generateDesktop(gameName, safeName, finalExec, execDir, icon, category)) {
        cerr << ERROR_COLOR << "Failed to write desktop entry.\n" << RESET;
        return 1;
    }

    if (commandExists("desktop-file-validate"))
        if (runCommand({"desktop-file-validate", desktopPath}) != 0)
            cerr << ERROR_COLOR << "Warning: desktop-file-validate found issues.\n" << RESET;

    refreshDB();
    cout << SUCCESS_COLOR << "\xe2\x9c\x94 Desktop entry created successfully!\n" << RESET;
    return 0;
}

int run(int argc, char* argv[]) {
    if (argc <= 1) { printHelp(); return 0; }

    ParseOutcome parsed = parseArgs(argc, argv);
    if (parsed.error != 0) return usageError(parsed.message);

    const Cli& cli = parsed.cli;
    switch (cli.cmd) {
        case Cmd::Help:
            printHelp();
            return 0;
        case Cmd::Version:
            cout << "DejaTop v" << VERSION << "\n";
            return 0;
        case Cmd::GetLatestProton: {
            auto protons = findProtonVersions();
            if (!protons.empty()) { cout << protons[0] << "\n"; return 0; }
            return 1;
        }
        case Cmd::List:
            return commandList();
        case Cmd::Show:
            if (cli.positional.empty()) { printHelp(); return 1; }
            return commandShow(joinFrom(cli.positional, 0));
        case Cmd::Rename:
            if (cli.positional.size() < 2)
                return usageError("--rename requires <name> <new_name>");
            return commandRename(cli.positional[0], joinFrom(cli.positional, 1));
        case Cmd::ReplaceIcon:
            if (cli.positional.size() < 2)
                return usageError("--replace-icon requires <name> <icon_path>");
            return commandReplaceIcon(cli.positional[0], joinFrom(cli.positional, 1));
        case Cmd::Delete:
            if (cli.positional.empty()) { printHelp(); return 1; }
            return commandDelete(joinFrom(cli.positional, 0));
        case Cmd::Create:
            if (cli.positional.empty()) { printHelp(); return 0; }
            if (cli.positional.size() > 1)
                return usageError("Unexpected argument '" + cli.positional[1] + "'");
            return commandCreate(cli, cli.positional[0]);
    }
    return 1;
}

} // namespace

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    initOutput();
    try {
        return run(argc, argv);
    } catch (const fs::filesystem_error& e) {
        cerr << ERROR_COLOR << "Error: " << e.what() << RESET << "\n";
        return 1;
    } catch (const std::exception& e) {
        cerr << ERROR_COLOR << "Error: " << e.what() << RESET << "\n";
        return 1;
    } catch (...) {
        cerr << ERROR_COLOR << "Error: unexpected failure.\n" << RESET;
        return 1;
    }
}
