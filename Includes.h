#pragma once

#define _WIN32_DCOM
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_HOURS_UNTIL_REMOVAL 24
#define LOCAL_GROUP L"Administrators"
#define LOCAL_GROUP_STR "Administrators"
#define HOURS_IN_MONTH 730

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <filesystem>
#include <memory>
#include <chrono>
#include <ctime>
#include "Windows.h"
#include "LM.h"
#include "io.h"
#include "fcntl.h"
#include "comdef.h"
#include "taskschd.h"

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "credui.lib")