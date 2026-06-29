#include "../include/generator.hpp"

[[nodiscard]] string createWrapper(const string& safeName, const string& execDir, const string& execPath, const string& selProton, const string& customPrefix, const vector<string>& envVars, const string& launchOptions) {
    string wrapperDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
    fs::create_directories(wrapperDir);
    string logDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "logs").string();
    fs::create_directories(logDir);
    string prefixDir = customPrefix.empty() ? (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "shared_prefix").string() : customPrefix;
    fs::create_directories(prefixDir);

    string wrapperPath = (fs::path(wrapperDir) / (safeName + "_wrapper.sh")).string();
    string logPath = (fs::path(logDir) / (safeName + ".log")).string();

    ofstream ofs(wrapperPath);
    if (!ofs.is_open()) return "";
    
    ofs << "#!/usr/bin/env bash\n";
    ofs << "exec >> \"" << escapeBash(logPath) << "\" 2>&1\n";
    ofs << "echo \"=== Starting DejaTop Wrapper for " << escapeBash(safeName) << " ===\"\n";
    ofs << "date\n";
    
    for (const auto& env : envVars) {
        // Quote the value part so spaces in paths (e.g. WINEPREFIX=/path with spaces) work
        size_t eq = env.find('=');
        if (eq != string::npos) {
            string key = env.substr(0, eq);
            string val = env.substr(eq + 1);
            ofs << "export " << key << "=\"" << escapeBash(val) << "\"\n";
        } else {
            ofs << "export " << env << "\n";
        }
    }
    
    ofs << "export STEAM_COMPAT_DATA_PATH=\"" << escapeBash(prefixDir) << "\"\n";
    ofs << "export STEAM_COMPAT_CLIENT_INSTALL_PATH=\"" << escapeBash(getHomeDir()) << "/.local/share/Steam\"\n";
    ofs << "cd \"" << escapeBash(execDir) << "\"\n";
    
    // Dynamically resolve the latest proton version via dejatop itself
    std::error_code ec;
    string dejatopBin = fs::read_symlink("/proc/self/exe", ec).string();
    if (dejatopBin.empty() || ec) dejatopBin = "dejatop";

    ofs << "LATEST_PROTON=$(\"" << escapeBash(dejatopBin) << "\" --get-latest-proton)\n";
    ofs << "if [ -z \"$LATEST_PROTON\" ]; then\n";
    ofs << "    echo \"Error: Proton not found. Install Proton via Steam or GE-Proton.\"\n";
    ofs << "    exit 1\n";
    ofs << "fi\n";
    
    string cmd = "\"$LATEST_PROTON/proton\" run \"" + escapeBash(execPath) + "\"";
    if (!launchOptions.empty()) {
        size_t pos = launchOptions.find("%command%");
        if (pos != string::npos) {
            cmd = launchOptions.substr(0, pos) + cmd + launchOptions.substr(pos + 9);
        } else {
            cmd = launchOptions + " " + cmd;
        }
    }
    ofs << cmd << "\n";
    ofs.close();

    fs::permissions(wrapperPath, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
    return wrapperPath;
}

[[nodiscard]] bool generateDesktop(const string& gameName, const string& safeName, const string& finalExec, const string& execDir, const string& iconPath, const string& category) {
    string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
    fs::create_directories(appsDir);
    string desktopPath = (fs::path(appsDir) / ("dejatop_" + safeName + ".desktop")).string();

    ofstream dofs(desktopPath);
    if (!dofs.is_open()) return false;
    
    dofs << "[Desktop Entry]\n";
    dofs << "Version=1.0\n";
    dofs << "Type=Application\n";
    dofs << "Name=" << stripNewlines(gameName) << "\n";
    dofs << "Comment=Created by DejaTop\n";
    dofs << "Exec=" << escapeDesktop(stripNewlines(finalExec)) << "\n";
    dofs << "Path=" << escapeDesktop(stripNewlines(execDir)) << "\n"; // Removed double quotes to fix spaces issue
    if (!iconPath.empty()) {
        dofs << "Icon=" << escapeDesktop(stripNewlines(iconPath)) << "\n";
    }
    dofs << "Terminal=false\n";
    dofs << "Categories=" << stripNewlines(category) << "\n";
    dofs.close();
    return true;
}

