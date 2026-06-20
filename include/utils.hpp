#pragma once
#include "common.hpp"

string getHomeDir();
string sanitizeName(const string& name);
string toLower(string s);
[[nodiscard]] int runCommand(const vector<string>& args);
bool commandExists(const string& cmd);
