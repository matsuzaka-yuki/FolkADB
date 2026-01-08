#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// String utilities
char* TrimString(char* str);
char* SplitString(char* str, char delimiter);
void StringToLower(char* str);
int StringStartsWith(const char* str, const char* prefix);

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

// Time utilities
void GetCurrentTimestamp(char* buffer, size_t buffer_size);

#endif // UTILS_H
