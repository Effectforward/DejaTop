#pragma once
#include "common.hpp"
#include "utils.hpp"

[[nodiscard]] string createWrapper(const string& safeName, const string& execDir, const string& execPath, const string& selProton, const string& customPrefix, const vector<string>& envVars, const string& launchOptions);
[[nodiscard]] bool generateDesktop(const string& gameName, const string& safeName, const string& finalExec, const string& execDir, const string& iconPath, const string& category);
