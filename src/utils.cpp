#include "../include/utils.hpp"

const char *RESET = "\033[0m";
const char *BOLD = "\033[1m";
const char *TITLE_COLOR = "\033[38;2;203;166;247m";
const char *PROMPT_COLOR = "\033[38;2;166;227;161m";
const char *SUCCESS_COLOR = "\033[38;2;166;227;161m";
const char *ERROR_COLOR = "\033[38;2;243;139;168m";

void initOutput() {
    // Colour only for an interactive stdout, and never when NO_COLOR is present.
    if (!isatty(STDOUT_FILENO) || getenv("NO_COLOR") != nullptr) {
        RESET = "";
        BOLD = "";
        TITLE_COLOR = "";
        PROMPT_COLOR = "";
        SUCCESS_COLOR = "";
        ERROR_COLOR = "";
    }
}

string getHomeDir() {
    const char* home = getenv("HOME");
    if (home) return string(home);
    struct passwd* pw = getpwuid(getuid());
    return pw ? string(pw->pw_dir) : "";
}

string sanitizeName(const string& name) {
    string safe = "";
    for (char c : name) {
        if (isalnum((unsigned char)c) || c == '_' || c == '-') safe += c;
    }
    return safe;
}

// Filesystem names are limited to 255 bytes. A long display name is truncated
// with a stable hash suffix so the same input always produces the same id.
string makeSafeId(const string& name) {
    constexpr size_t MAX_ID = 120;
    string safe = sanitizeName(name);
    if (safe.empty() || safe.size() <= MAX_ID) return safe;

    uint32_t hash = 2166136261u;
    for (unsigned char c : name) {
        hash ^= c;
        hash *= 16777619u;
    }
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), "-%06x", hash & 0xffffffu);

    string id = safe.substr(0, MAX_ID) + suffix;
    return id;
}

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

string stripNewlines(string s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

// Desktop Entry string values: \\, \n, \t and \r are the only escapes.
string escapeDesktopValue(const string& s) {
    string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

bool validEnvKey(const string& key) {
    if (key.empty()) return false;
    unsigned char first = static_cast<unsigned char>(key[0]);
    if (!std::isalpha(first) && key[0] != '_') return false;
    for (unsigned char c : key) {
        if (!std::isalnum(c) && c != '_') return false;
    }
    return true;
}


string escapeBash(const string& s) {
    string res;
    for (char c : s) {
        if (c == '"' || c == '`' || c == '$' || c == '\\') res += '\\';
        res += c;
    }
    return res;
}

string escapeDesktop(const string& s) {
    string res;
    for (char c : s) {
        if (c == '%') res += "%%";
        else res += c;
    }
    return res;
}

[[nodiscard]] int runCommand(const vector<string>& args) {
    if (args.empty()) return EXEC_FORK_FAILED;
    // Flush before fork: the child inherits the buffers, and every fork would
    // otherwise re-emit whatever is still pending.
    std::cout.flush();
    std::cerr.flush();
    pid_t pid = fork();
    if (pid < 0) return EXEC_FORK_FAILED;
    if (pid == 0) {
        // Child: build exec_argv for execvp
        vector<const char*> exec_argv;
        for (const auto& a : args) exec_argv.push_back(a.c_str());
        exec_argv.push_back(nullptr);
        // Silence stdout/stderr
        if (!freopen("/dev/null", "w", stdout)) _exit(EXEC_FAILED);
        if (!freopen("/dev/null", "w", stderr)) _exit(EXEC_FAILED);
        execvp(exec_argv[0], const_cast<char* const*>(exec_argv.data()));
        _exit(EXEC_FAILED); // execvp failed
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : EXEC_FORK_FAILED;
}



bool commandExists(const string& cmd) {
    if (cmd.empty()) return false;

    if (cmd.find('/') != string::npos) {
        return access(cmd.c_str(), X_OK) == 0;
    }

    const char* pathEnv = getenv("PATH");
    if (!pathEnv || !*pathEnv) return false;

    string pathValue(pathEnv);
    size_t start = 0;
    while (start <= pathValue.size()) {
        size_t end = pathValue.find(':', start);
        string dir = pathValue.substr(start, end == string::npos ? string::npos : end - start);
        if (dir.empty()) dir = ".";

        fs::path candidate = fs::path(dir) / cmd;
        if (access(candidate.c_str(), X_OK) == 0) return true;

        if (end == string::npos) break;
        start = end + 1;
    }

    return false;
}

