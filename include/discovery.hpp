#pragma once
#include "common.hpp"
#include "utils.hpp"

bool protonSortCompare(const string& a, const string& b);
vector<string> findProtonVersions();
vector<string> findIcons(const string& execDir);
string extractExeIcon(const string& execPath, const string& safeName);
string extractHeroicPrefix(const string& execPath);
string extractLutrisPrefix(const string& execPath);
string extractSteamPrefix(const string& execPath);
string autoDiscoverPrefix(const string& execPath);
string findDesktopFile(const string& query);
