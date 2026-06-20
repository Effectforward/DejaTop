#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdio>
#include <memory>
#include <array>

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::cout;
using std::cin;
using std::ifstream;
using std::ofstream;
using std::sort;
using std::transform;
using std::to_string;
using std::getline;
using std::ios;
using std::istreambuf_iterator;

constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* TITLE_COLOR = "\033[38;2;203;166;247m";
constexpr const char* PROMPT_COLOR = "\033[38;2;166;227;161m";
constexpr const char* SUCCESS_COLOR = "\033[38;2;166;227;161m";
constexpr const char* ERROR_COLOR = "\033[38;2;243;139;168m";

constexpr const char* VERSION = "1.0.0-beta";

constexpr int EXEC_SUCCESS = 0;
constexpr int EXEC_FAILED = 127;
constexpr int EXEC_FORK_FAILED = -1;
