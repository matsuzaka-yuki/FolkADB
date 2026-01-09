#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// ANSI Colors
#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_RED        "\033[31m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_BLUE       "\033[34m"
#define ANSI_MAGENTA    "\033[35m"
#define ANSI_CYAN       "\033[36m"
#define ANSI_WHITE      "\033[37m"
#define ANSI_BRIGHT_RED "\033[91m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_YELLOW "\033[93m"
#define ANSI_BRIGHT_BLUE "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN "\033[96m"
#define ANSI_BG_BLUE    "\033[44m"
#define ANSI_BG_RESET   "\033[49m"

// String utilities
char* TrimString(char* str);
char* SplitString(char* str, char delimiter);
void StringToLower(char* str);
int StringStartsWith(const char* str, const char* prefix);
int LevenshteinDistance(const char* s1, const char* s2);

// Path utilities
void JoinPath(char* dest, size_t dest_size, const char* path1, const char* path2);
int FileExists(const char* path);
int DirectoryExists(const char* path);

// Error handling
void PrintError(AdbErrorCode code, const char* message);
void PrintWarning(const char* message);
void PrintInfo(const char* message);

// Memory utilities
void* SafeMalloc(size_t size);
void* SafeCalloc(size_t count, size_t size);
void* SafeRealloc(void* ptr, size_t size);

// Run generic process and capture output
ProcessResult* RunProcess(const char* executable_path, const char* args[], int arg_count);

// Config persistence
void SaveConfig(int theme);
int LoadConfig(void);

// Time utilities
void GetCurrentTimestamp(char* buffer, size_t buffer_size);

#endif // UTILS_H
