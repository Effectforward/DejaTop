#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using std::cin;
using std::cout;
using std::getline;
using std::ifstream;
using std::ios;
using std::istreambuf_iterator;
using std::istringstream;
using std::ofstream;
using std::remove;
using std::sort;
using std::string;
using std::to_string;
using std::transform;
using std::vector;

constexpr const char *RESET = "\033[0m";
constexpr const char *BOLD = "\033[1m";
constexpr const char *TITLE_COLOR = "\033[38;2;203;166;247m";
constexpr const char *PROMPT_COLOR = "\033[38;2;166;227;161m";
constexpr const char *SUCCESS_COLOR = "\033[38;2;166;227;161m";
constexpr const char *ERROR_COLOR = "\033[38;2;243;139;168m";

constexpr const char *VERSION = "1.0.0";

constexpr int EXEC_SUCCESS = 0;
constexpr int EXEC_FAILED = 127;
constexpr int EXEC_FORK_FAILED = -1;
