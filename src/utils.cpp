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
    return runCommand({"which", cmd}) == EXEC_SUCCESS;
}

