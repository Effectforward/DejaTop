#pragma once
#include "common.hpp"
#include "utils.hpp"

struct ImageInfo {
    int width = 0;
    int height = 0;
    int bitsPerPixel = 0; // 0 = unknown / not applicable (e.g. SVG)
    bool valid = false;
};

ImageInfo getPngInfo(const string& path);
ImageInfo getIcoInfo(const string& path);

bool protonSortCompare(const string& a, const string& b);
vector<string> findProtonVersions();
vector<string> findIcons(const string& execDir, const string& gameName);
string extractExeIcon(const string& execPath, const string& safeName);
string extractHeroicPrefix(const string& execPath);
string extractLutrisPrefix(const string& execPath);
string extractSteamPrefix(const string& execPath);
string extractSteamLaunchOptions(const string& execPath);
string autoDiscoverPrefix(const string& execPath);
string findDesktopFile(const string& query);
