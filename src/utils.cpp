#include "../include/utils.hpp"

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

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

string stripNewlines(string s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
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

string runCommandOutput(const string& cmd) {
    std::array<char, 128> buffer;
    string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
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

