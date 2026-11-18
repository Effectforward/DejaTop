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
using std::cerr;
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

constexpr const char *VERSION = "1.0.1";

// Initialised by initOutput(): empty when colour is disabled.
extern const char *RESET;
extern const char *BOLD;
extern const char *TITLE_COLOR;
extern const char *PROMPT_COLOR;
extern const char *SUCCESS_COLOR;
extern const char *ERROR_COLOR;

constexpr int EXEC_SUCCESS = 0;
constexpr int EXEC_FAILED = 127;
constexpr int EXEC_FORK_FAILED = -1;
