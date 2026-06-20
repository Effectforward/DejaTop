#include "../include/generator.hpp"

[[nodiscard]] string createWrapper(const string& safeName, const string& execDir, const string& execPath, const string& selProton, const string& customPrefix) {
    string wrapperDir = (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "wrappers").string();
    fs::create_directories(wrapperDir);
    string prefixDir = customPrefix.empty() ? (fs::path(getHomeDir()) / ".local" / "share" / "DejaTop" / "shared_prefix").string() : customPrefix;
    if (customPrefix.empty()) {
        fs::create_directories(prefixDir);
    }
    string wrapperPath = (fs::path(wrapperDir) / (safeName + "_wrapper.sh")).string();

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

[[nodiscard]] bool generateDesktop(const string& gameName, const string& safeName, const string& finalExec, const string& execDir, const string& iconPath) {
    string appsDir = (fs::path(getHomeDir()) / ".local" / "share" / "applications").string();
    fs::create_directories(appsDir);
    string desktopPath = (fs::path(appsDir) / ("dejatop_" + safeName + ".desktop")).string();

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
    return true;
}

