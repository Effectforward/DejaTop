#pragma once
#include "common.hpp"

string getHomeDir();
void initOutput();
string sanitizeName(const string& name);
string makeSafeId(const string& name);
string toLower(string s);
string stripNewlines(string s);
string escapeBash(const string& s);
string escapeDesktop(const string& s);
string escapeDesktopValue(const string& s);
bool validEnvKey(const string& key);
[[nodiscard]] int runCommand(const vector<string>& args);

bool commandExists(const string& cmd);
