#pragma once
#include "common.hpp"

string getHomeDir();
string sanitizeName(const string& name);
string toLower(string s);
[[nodiscard]] int runCommand(const vector<string>& args);
string runCommandOutput(const string& cmd);
bool commandExists(const string& cmd);
